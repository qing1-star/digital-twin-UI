#include "DigitalTwinModuleController.h"

#include "DigitalTwinWorkbenchWidget.h"
#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportServices.h"
#include "../../../SMRobotApps/RobotQtViewer/RobotQtViewerLanguage.h"

#include <SimulationProject/ProjectDocument.h>

#include <QMetaObject>

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
        return robot_qt_viewer::LanguageManager::text(
            robot_qt_viewer::LanguageManager::savedLanguage(),
            key);
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
        connect(&m_widget, &DigitalTwinWorkbenchWidget::mappingRemoveLastRequested,
            this, &DigitalTwinModuleController::handleRemoveLastMapping);
        connect(&m_widget, &DigitalTwinWorkbenchWidget::mappingConfirmRequested,
            this, &DigitalTwinModuleController::handleConfirmMappings);
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
        QString error;
        if(!validateMappings(m_pendingMappings, error)) {
            publishMappingSummary(error);
            return;
        }

        m_confirmedMappings = m_pendingMappings;
        publishMappingSummary(dtText("digitalTwin.mapping.confirmed"));
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
        QStringList labels;
        const QStringList realRobotNames = m_widget.realRobotNames();
        for(const QString& realRobotName : realRobotNames) {
            const int axisCount = m_widget.realRobotAxisCount(realRobotName);
            if(axisCount <= 0) {
                continue;
            }
            m_realRobotAxisCounts.insert(realRobotName, axisCount);
            labels.push_back(realRobotName);
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

    void DigitalTwinModuleController::publishMappingSummary(const QString& tailMessage)
    {
        QStringList lines;
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
            const int expected = m_realRobotAxisCounts.value(entry.realRobotName, 0);
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
        int sourceOffset = 0;
        for(const MappingEntry& entry : mappings) {
            const int sourceCount = entry.realRobotName.isEmpty()
                ? entry.jointNames.size()
                : m_realRobotAxisCounts.value(entry.realRobotName, 0);
            if(sourceCount <= 0 || static_cast<int>(angles.size()) <= sourceOffset) {
                continue;
            }

            const int count = std::min<int>(
                std::min(sourceCount, static_cast<int>(entry.jointNames.size())),
                static_cast<int>(angles.size()) - sourceOffset);
            if(primaryRobotId.isEmpty() && count > 0) {
                primaryRobotId = entry.sceneRobotId;
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
            sourceOffset += sourceCount;
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
        }
    }
}
