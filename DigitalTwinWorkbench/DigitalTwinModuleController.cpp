#include "DigitalTwinModuleController.h"

#include "DigitalTwinWorkbenchWidget.h"
#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerLocalization.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportServices.h"

#include <SimulationProject/ProjectDocument.h>

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    QString formatJointRangeLine(
        const QString& robotLabel,
        int beginIndex,
        int endIndex)
    {
        return QStringLiteral("%1 <= joints[%2..%3]").arg(robotLabel).arg(beginIndex).arg(endIndex);
    }

    QString dtText(const QString& key)
    {
        QApplication* application = qobject_cast<QApplication*>(QApplication::instance());
        const auto* localization = application != nullptr
            ? robot_qt_viewer::RobotQtViewerLocalizationService::installedOnApplication(
                *application)
            : nullptr;
        return localization != nullptr ? localization->text(key, key) : key;
    }

    bool isRevoluteJointType(const QString& jointType)
    {
        return jointType.compare(QStringLiteral("revolute"), Qt::CaseInsensitive) == 0 ||
            jointType.compare(QStringLiteral("continuous"), Qt::CaseInsensitive) == 0;
    }

    double toRadians(double degrees)
    {
        return degrees * kPi / 180.0;
    }

    double applySceneJointDirection(
        const QString& sceneRobotId,
        const QString& jointName,
        double degrees)
    {
        // The Red4600 URDF defines joints 2 and 5 about the opposite axes to ABB.
        if(sceneRobotId == QStringLiteral("Red4600") &&
           (jointName == QStringLiteral("joint2") ||
            jointName == QStringLiteral("joint5"))) {
            return -degrees;
        }
        return degrees;
    }
}

namespace robot_qt_viewer
{
    DigitalTwinModuleController::DigitalTwinModuleController(
        DigitalTwinWorkbenchWidget& widget,
        RobotQtViewerDocumentContext& context,
        QObject* parent)
        : QObject(parent)
        , m_widget(widget)
        , m_context(context)
    {
        connect(&m_widget, &DigitalTwinWorkbenchWidget::mappingAddRequested,
            this, &DigitalTwinModuleController::handleAddMapping);
        connect(&m_widget, &DigitalTwinWorkbenchWidget::mappingConfigLoadRequested,
            this, &DigitalTwinModuleController::handleLoadMappingConfig);
        connect(&m_widget, &DigitalTwinWorkbenchWidget::mappingRemoveLastRequested,
            this, &DigitalTwinModuleController::handleRemoveLastMapping);
        connect(&m_widget, &DigitalTwinWorkbenchWidget::mappingConfirmRequested,
            this, &DigitalTwinModuleController::handleConfirmMappings);
        connect(&m_widget, &DigitalTwinWorkbenchWidget::digitalTwinStateChanged,
            this, &DigitalTwinModuleController::handleDigitalTwinStateChanged);
        connect(&m_widget, &DigitalTwinWorkbenchWidget::robotConnectionStatusChanged,
            this, &DigitalTwinModuleController::handleRobotConnectionStatusChanged);
        m_widget.setJointsUpdatedHandler([this](const std::vector<float>& angles, bool continuous) {
            QMetaObject::invokeMethod(
                this,
                [this, angles, continuous]() { syncRobotToViewport(angles, continuous); },
                Qt::QueuedConnection);
        });
        setSelectedRobot(m_context.selectionModel().state().robotId);
        refreshSceneRobots();
        refreshRealRobots();
        publishMappingSummary();
    }

    void DigitalTwinModuleController::setRobotRuntime(
        const QString& robotId,
        const QString& robotName,
        const QStringList& movableJoints,
        const QStringList& movableJointTypes)
    {
        if(robotId.isEmpty()) {
            return;
        }

        if(!m_virtualRobotNames.contains(robotId)) {
            m_virtualRobotOrder.push_back(robotId);
        }
        m_virtualRobotNames[robotId] = robotName;
        m_virtualRobotJoints[robotId] = movableJoints;
        m_virtualRobotJointTypes[robotId] = movableJointTypes;
        refreshSceneRobots();
        publishMappingSummary();
    }

    void DigitalTwinModuleController::handleEvent(const RobotQtViewerEvent& event)
    {
        switch(event.kind) {
        case RobotQtViewerEventKind::SelectionChanged:
            setSelectedRobot(event.selection.robotId);
            break;
        case RobotQtViewerEventKind::ProjectOpened:
        case RobotQtViewerEventKind::ProjectDocumentChanged:
        case RobotQtViewerEventKind::ViewportReloaded:
        case RobotQtViewerEventKind::RobotRuntimeChanged:
            refreshSceneRobots();
            refreshRealRobots();
            publishMappingSummary();
            break;
        default:
            break;
        }
    }

    void DigitalTwinModuleController::handleAddMapping(
        const QString& sceneRobotId,
        const QString& sceneRobotLabel,
        const QString& realRobotName)
    {
        if(!m_loadedMappings.isEmpty()) {
            const int mappingCount = m_loadedMappings.size();
            const QString fileName = QFileInfo(m_loadedMappingFilePath).fileName();
            m_pendingMappings = m_loadedMappings;
            m_loadedMappings.clear();
            m_loadedMappingFilePath.clear();
            publishMappingSummary(
                dtText("digitalTwin.mapping.configAdded").arg(mappingCount).arg(fileName));
            return;
        }

        if(sceneRobotId.isEmpty() || realRobotName.isEmpty()) {
            publishMappingSummary(dtText("digitalTwin.mapping.invalidSelection"));
            return;
        }

        MappingEntry entry;
        entry.sceneRobotId = sceneRobotId;
        entry.sceneRobotLabel = sceneRobotLabel;
        entry.realRobotName = realRobotName;
        entry.jointNames = jointNamesForRobot(sceneRobotId);
        if(entry.jointNames.isEmpty()) {
            publishMappingSummary(dtText("digitalTwin.mapping.noVirtualRobotJoints"));
            return;
        }

        m_pendingMappings.push_back(entry);
        publishMappingSummary(
            dtText("digitalTwin.mapping.added").arg(entry.sceneRobotLabel, entry.realRobotName));
    }

    void DigitalTwinModuleController::handleLoadMappingConfig(const QString& filePath)
    {
        m_loadedMappings.clear();
        m_loadedMappingFilePath.clear();

        QFile file(filePath);
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            publishMappingSummary(
                dtText("digitalTwin.mapping.configOpenFailed")
                    .arg(QFileInfo(filePath).fileName(), file.errorString()));
            return;
        }

        QStringList virtualRobotNames;
        QStringList realRobotNames;
        bool readingRealRobots = false;
        bool separatorFound = false;
        QTextStream input(&file);
        while(!input.atEnd()) {
            const QString line = input.readLine().trimmed();
            if(line.isEmpty()) {
                if(!virtualRobotNames.isEmpty()) {
                    readingRealRobots = true;
                    separatorFound = true;
                }
                continue;
            }
            (readingRealRobots ? realRobotNames : virtualRobotNames).push_back(line);
        }

        if(!separatorFound || virtualRobotNames.isEmpty() || realRobotNames.isEmpty()) {
            publishMappingSummary(dtText("digitalTwin.mapping.configInvalidFormat"));
            return;
        }
        if(virtualRobotNames.size() != realRobotNames.size()) {
            publishMappingSummary(
                dtText("digitalTwin.mapping.configCountMismatch")
                    .arg(virtualRobotNames.size())
                    .arg(realRobotNames.size()));
            return;
        }

        QVector<MappingEntry> loadedMappings;
        loadedMappings.reserve(virtualRobotNames.size());
        QSet<QString> mappedVirtualRobots;
        QSet<QString> mappedRealRobots;
        for(int index = 0; index < virtualRobotNames.size(); ++index) {
            const QString configuredVirtualName = virtualRobotNames.at(index);
            QString sceneRobotId;
            for(const QString& robotId : m_virtualRobotOrder) {
                if(robotId.compare(configuredVirtualName, Qt::CaseInsensitive) == 0 ||
                   m_virtualRobotNames.value(robotId).compare(
                       configuredVirtualName, Qt::CaseInsensitive) == 0) {
                    sceneRobotId = robotId;
                    break;
                }
            }
            if(sceneRobotId.isEmpty()) {
                publishMappingSummary(
                    dtText("digitalTwin.mapping.configVirtualNotFound")
                        .arg(configuredVirtualName));
                return;
            }

            const QString virtualKey = sceneRobotId.toCaseFolded();
            const QString configuredRealName = realRobotNames.at(index);
            const QString realKey = configuredRealName.toCaseFolded();
            if(mappedVirtualRobots.contains(virtualKey) || mappedRealRobots.contains(realKey)) {
                publishMappingSummary(
                    dtText("digitalTwin.mapping.configDuplicatePair")
                        .arg(configuredVirtualName, configuredRealName));
                return;
            }

            MappingEntry entry;
            entry.sceneRobotId = sceneRobotId;
            entry.sceneRobotLabel = QStringLiteral("%1 (%2)")
                                        .arg(m_virtualRobotNames.value(sceneRobotId), sceneRobotId);
            entry.realRobotName = configuredRealName;
            entry.jointNames = jointNamesForRobot(sceneRobotId);
            if(entry.jointNames.isEmpty()) {
                publishMappingSummary(dtText("digitalTwin.mapping.noVirtualRobotJoints"));
                return;
            }

            mappedVirtualRobots.insert(virtualKey);
            mappedRealRobots.insert(realKey);
            loadedMappings.push_back(entry);
        }

        m_loadedMappings = loadedMappings;
        m_loadedMappingFilePath = filePath;
        publishMappingSummary(
            dtText("digitalTwin.mapping.configLoaded")
                .arg(QFileInfo(filePath).fileName())
                .arg(loadedMappings.size()));
    }

    void DigitalTwinModuleController::handleRemoveLastMapping()
    {
        if(m_pendingMappings.isEmpty()) {
            publishMappingSummary(dtText("digitalTwin.mapping.noPendingToRemove"));
            return;
        }

        const MappingEntry removed = m_pendingMappings.back();
        m_pendingMappings.pop_back();
        publishMappingSummary(
            dtText("digitalTwin.mapping.removed").arg(removed.sceneRobotLabel, removed.realRobotName));
    }

    void DigitalTwinModuleController::handleConfirmMappings()
    {
        refreshRealRobots();
        QString error;
        if(!validateMappings(m_pendingMappings, error)) {
            publishMappingSummary(error);
            return;
        }

        m_confirmedMappings = m_pendingMappings;
        publishMappingSummary(
            dtText("digitalTwin.mapping.confirmedCount").arg(m_confirmedMappings.size()));
    }

    void DigitalTwinModuleController::handleDigitalTwinStateChanged(bool active)
    {
        m_twinSyncReported = false;
        if(active) {
            publishMappingSummary(
                dtText("digitalTwin.mapping.twinStarted").arg(activeMappings().size()));
        }
    }

    void DigitalTwinModuleController::handleRobotConnectionStatusChanged(bool)
    {
        refreshRealRobots();
    }

    void DigitalTwinModuleController::setSelectedRobot(const QString& robotId)
    {
        m_selectedRobotId = robotId;
    }

    void DigitalTwinModuleController::refreshSceneRobots()
    {
        QStringList labels;
        QStringList robotIds;
        for(const QString& robotId : m_virtualRobotOrder) {
            const auto it = m_virtualRobotNames.constFind(robotId);
            if(it == m_virtualRobotNames.constEnd()) {
                continue;
            }
            labels.push_back(QStringLiteral("%1 (%2)").arg(it.value(), robotId));
            robotIds.push_back(robotId);
        }
        m_widget.setSceneRobotOptions(labels, robotIds);
    }

    void DigitalTwinModuleController::refreshRealRobots()
    {
        m_realRobotAxisCounts.clear();
        m_realRobotAxisOffsets.clear();
        QStringList labels;
        const QStringList realRobotNames = m_widget.realRobotNames();
        int axisOffset = 0;
        for(const QString& realRobotName : realRobotNames) {
            const int axisCount = m_widget.realRobotAxisCount(realRobotName);
            if(axisCount <= 0) {
                continue;
            }
            m_realRobotAxisCounts.insert(realRobotName, axisCount);
            m_realRobotAxisOffsets.insert(realRobotName, axisOffset);
            labels.push_back(realRobotName);
            axisOffset += axisCount;
        }
        m_widget.setRealRobotOptions(labels);
    }

    QStringList DigitalTwinModuleController::jointNamesForRobot(const QString& robotId) const
    {
        QStringList jointNames;
        if(robotId.isEmpty()) {
            return jointNames;
        }
        return m_virtualRobotJoints.value(robotId);
    }

    int DigitalTwinModuleController::realRobotAxisCount(const QString& realRobotName) const
    {
        for(auto it = m_realRobotAxisCounts.constBegin();
            it != m_realRobotAxisCounts.constEnd(); ++it) {
            if(it.key().compare(realRobotName, Qt::CaseInsensitive) == 0) {
                return it.value();
            }
        }
        return 0;
    }

    int DigitalTwinModuleController::realRobotAxisOffset(const QString& realRobotName) const
    {
        for(auto it = m_realRobotAxisOffsets.constBegin();
            it != m_realRobotAxisOffsets.constEnd(); ++it) {
            if(it.key().compare(realRobotName, Qt::CaseInsensitive) == 0) {
                return it.value();
            }
        }
        return -1;
    }

    void DigitalTwinModuleController::publishMappingSummary(const QString& tailMessage)
    {
        QStringList lines;
        if(!m_loadedMappings.isEmpty()) {
            lines.push_back(
                dtText("digitalTwin.mapping.loadedConfigHeader")
                    .arg(QFileInfo(m_loadedMappingFilePath).fileName()));
            for(const MappingEntry& entry : m_loadedMappings) {
                lines.push_back(
                    QStringLiteral("  %1 <- %2")
                        .arg(entry.sceneRobotLabel, entry.realRobotName));
            }
            lines.push_back(QStringLiteral("------------------------"));
        }
        if(m_confirmedMappings.isEmpty() && m_pendingMappings.isEmpty()) {
            lines.push_back(dtText("digitalTwin.mapping.noneConfigured"));
            QVector<MappingEntry> defaults = activeMappings();
            if(!defaults.isEmpty()) {
                lines.push_back(dtText("digitalTwin.mapping.defaultAllocationHeader"));
                int jointOffset = 1;
                for(const MappingEntry& entry : defaults) {
                    const int jointCount = entry.jointNames.size();
                    if(jointCount <= 0) {
                        continue;
                    }
                    lines.push_back(formatJointRangeLine(
                        entry.sceneRobotLabel,
                        jointOffset,
                        jointOffset + jointCount - 1));
                    jointOffset += jointCount;
                }
            }
        } else {
            lines.push_back(dtText("digitalTwin.mapping.pendingHeader"));
            if(m_pendingMappings.isEmpty()) {
                lines.push_back(dtText("digitalTwin.mapping.none"));
            } else {
                for(const MappingEntry& entry : m_pendingMappings) {
                    lines.push_back(QStringLiteral("  %1 <- %2").arg(entry.sceneRobotLabel, entry.realRobotName));
                }
            }

            lines.push_back(dtText("digitalTwin.mapping.confirmedHeader"));
            if(m_confirmedMappings.isEmpty()) {
                lines.push_back(dtText("digitalTwin.mapping.none"));
            } else {
                for(const MappingEntry& entry : m_confirmedMappings) {
                    lines.push_back(QStringLiteral("  %1 <- %2").arg(entry.sceneRobotLabel, entry.realRobotName));
                }
            }
        }

        if(!tailMessage.isEmpty()) {
            lines.push_back(QStringLiteral("------------------------"));
            lines.push_back(tailMessage);
            emit statusMessageRequested(tailMessage, 4000);
        }
        m_widget.setMappingSummary(lines);
    }

    QVector<DigitalTwinModuleController::MappingEntry> DigitalTwinModuleController::activeMappings() const
    {
        if(!m_confirmedMappings.isEmpty()) {
            return m_confirmedMappings;
        }

        QVector<MappingEntry> defaults;
        if(m_virtualRobotOrder.isEmpty()) {
            return defaults;
        }

        for(const QString& robotId : m_virtualRobotOrder) {
            const QStringList jointNames = jointNamesForRobot(robotId);
            if(jointNames.isEmpty()) {
                continue;
            }

            MappingEntry entry;
            entry.sceneRobotId = robotId;
            entry.sceneRobotLabel = QStringLiteral("%1 (%2)")
                                        .arg(m_virtualRobotNames.value(robotId), robotId);
            entry.jointNames = jointNames;
            defaults.push_back(entry);
        }
        return defaults;
    }

    bool DigitalTwinModuleController::validateMappings(const QVector<MappingEntry>& mappings, QString& error) const
    {
        if(mappings.isEmpty()) {
            return true;
        }

        for(const MappingEntry& entry : mappings) {
            const int expected = realRobotAxisCount(entry.realRobotName);
            if(expected <= 0) {
                error = dtText("digitalTwin.mapping.unsupportedRealRobot").arg(entry.realRobotName);
                return false;
            }
            if(entry.jointNames.size() != expected) {
                error = dtText("digitalTwin.mapping.jointCountMismatch")
                            .arg(entry.realRobotName)
                            .arg(expected)
                            .arg(entry.sceneRobotLabel)
                            .arg(entry.jointNames.size());
                return false;
            }
        }
        return true;
    }

    void DigitalTwinModuleController::syncRobotToViewport(const std::vector<float>& angles, bool continuous)
    {
        if(continuous && !m_widget.digitalTwinActive()) {
            return;
        }

        RobotQtViewerViewportServices* viewportServices = m_context.viewportServices();
        if(viewportServices == nullptr) {
            return;
        }

        const QVector<MappingEntry> mappings = activeMappings();
        QString primaryRobotId;
        bool updated = false;
        int updatedMappingCount = 0;
        int defaultSourceOffset = 0;
        for(const MappingEntry& entry : mappings) {
            const int sourceCount = entry.realRobotName.isEmpty()
                ? entry.jointNames.size()
                : realRobotAxisCount(entry.realRobotName);
            const int sourceOffset = entry.realRobotName.isEmpty()
                ? defaultSourceOffset
                : realRobotAxisOffset(entry.realRobotName);
            if(entry.realRobotName.isEmpty()) {
                defaultSourceOffset += sourceCount;
            }
            if(sourceCount <= 0 || sourceOffset < 0 ||
               static_cast<int>(angles.size()) <= sourceOffset) {
                continue;
            }

            const int count = std::min<int>(
                std::min(sourceCount, static_cast<int>(entry.jointNames.size())),
                static_cast<int>(angles.size()) - sourceOffset);
            if(primaryRobotId.isEmpty() && count > 0) {
                primaryRobotId = entry.sceneRobotId;
            }
            if(count > 0) {
                ++updatedMappingCount;
            }
            const QStringList jointTypes = m_virtualRobotJointTypes.value(entry.sceneRobotId);
            for(int i = 0; i < count; ++i) {
                const QString jointType = i < jointTypes.size() ? jointTypes.at(i) : QString();
                const QString& jointName = entry.jointNames.at(i);
                const double sceneDegrees = applySceneJointDirection(
                    entry.sceneRobotId,
                    jointName,
                    angles[static_cast<std::size_t>(sourceOffset + i)]);
                const double runtimeValue = isRevoluteJointType(jointType)
                    ? toRadians(sceneDegrees)
                    : sceneDegrees;
                viewportServices->setRobotJointValue(
                    entry.sceneRobotId,
                    jointName,
                    runtimeValue);
                updated = true;
            }
        }

        if(updated) {
            if(!primaryRobotId.isEmpty() && m_context.selectionModel().state().robotId != primaryRobotId) {
                m_context.selectionModel().selectRobotLink(
                    primaryRobotId,
                    QString(),
                    continuous ? QStringLiteral("digitalTwinContinuousSelection") : QStringLiteral("digitalTwinSingleSelection"));
            }
            m_context.documentController().publishRobotRuntimeChanged(
                continuous ? QStringLiteral("digitalTwinContinuousSync") : QStringLiteral("digitalTwinSingleSync"));
            if(continuous && !m_twinSyncReported) {
                const QString message = dtText("digitalTwin.mapping.syncSucceeded")
                                            .arg(updatedMappingCount);
                m_widget.appendMappingStatus(message);
                emit statusMessageRequested(message, 5000);
                m_twinSyncReported = true;
            }
        }
    }
}
