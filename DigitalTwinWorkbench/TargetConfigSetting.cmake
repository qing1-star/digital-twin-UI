cmake_minimum_required(VERSION 3.20)

set(${TARGET_NAME}_RequiredLibsPublic
    Qt5::Widgets
    SMRobotWorkbenchCommon::WorkbenchCommon
    SMRobotWorkbenchCommon::RobotQtModulesShared
    SMRobotSpray::DigitalTwinCommunicate
)

set(${TARGET_NAME}_RequiredLibsPrivate)
