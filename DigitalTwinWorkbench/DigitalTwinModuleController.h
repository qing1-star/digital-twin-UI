#pragma once

#include "RobotQtViewerEvents.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <vector>

class DigitalTwinWorkbenchWidget;

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentContext;

    class DigitalTwinModuleController : public QObject
    {
        Q_OBJECT

    public:
        DigitalTwinModuleController(
            DigitalTwinWorkbenchWidget& widget,
            RobotQtViewerDocumentContext& context,
            QObject* parent = nullptr);

        void setRobotRuntime(
            const QString& robotId,
            const QString& robotName,
            const QStringList& movableJoints,
            const QStringList& movableJointTypes);
        void handleEvent(const RobotQtViewerEvent& event);

    signals:
        void statusMessageRequested(const QString& message, int timeoutMs);

    private:
        struct MappingEntry
        {
            QString sceneRobotId;
            QString sceneRobotLabel;
            QString realRobotName;
            QStringList jointNames;
        };

        void handleAddMapping(const QString& sceneRobotId, const QString& sceneRobotLabel, const QString& realRobotName);
        void handleLoadMappingConfig(const QString& filePath);
        void handleRemoveLastMapping();
        void handleConfirmMappings();
        void handleDigitalTwinStateChanged(bool active);
        void handleRobotConnectionStatusChanged(bool connected);
        void setSelectedRobot(const QString& robotId);
        void syncRobotToViewport(const std::vector<float>& angles, bool continuous);
        void refreshSceneRobots();
        void refreshRealRobots();
        QStringList jointNamesForRobot(const QString& robotId) const;
        int realRobotAxisCount(const QString& realRobotName) const;
        int realRobotAxisOffset(const QString& realRobotName) const;
        void publishMappingSummary(const QString& tailMessage = QString());
        QVector<MappingEntry> activeMappings() const;
        bool validateMappings(const QVector<MappingEntry>& mappings, QString& error) const;

        DigitalTwinWorkbenchWidget& m_widget;
        RobotQtViewerDocumentContext& m_context;
        QString m_selectedRobotId;
        QHash<QString, int> m_realRobotAxisCounts;
        QHash<QString, int> m_realRobotAxisOffsets;
        QHash<QString, QString> m_virtualRobotNames;
        QHash<QString, QStringList> m_virtualRobotJoints;
        QHash<QString, QStringList> m_virtualRobotJointTypes;
        QStringList m_virtualRobotOrder;
        QVector<MappingEntry> m_loadedMappings;
        QString m_loadedMappingFilePath;
        QVector<MappingEntry> m_pendingMappings;
        QVector<MappingEntry> m_confirmedMappings;
        bool m_twinSyncReported = false;
    };
}
