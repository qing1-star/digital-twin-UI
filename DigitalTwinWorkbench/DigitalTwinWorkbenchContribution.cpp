#include "DigitalTwinWorkbenchContribution.h"

namespace robot_qt_viewer
{
    bool registerDigitalTwinWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.digital-twin");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Digital Twin"))) ||
            !catalog.registerWorkbench(makeRobotQtViewerWorkbench(
               packageId,
               RobotQtViewerWorkbenchKind::DigitalTwin,
               QStringLiteral("digitalTwinWorkbench"),
               80,
               { QStringLiteral("smrobot.feature.digital-twin") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
            QStringLiteral("smrobot.feature.digital-twin"),
            QStringLiteral("Digital Twin"),
            packageId,
            { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::DigitalTwin) }));
    }
}
