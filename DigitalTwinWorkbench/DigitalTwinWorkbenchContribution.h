#pragma once

#include "RobotQtViewerWorkbenchPackageRegistry.h"

namespace robot_qt_viewer
{
    bool registerDigitalTwinWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog,
        RobotQtViewerWorkbenchPackageSource source);
}
