cmake_minimum_required(VERSION 3.20)

set(TARGET_NAME DigitalTwinWorkbench)

set(SOURCES
    DigitalTwinWorkbenchContribution.cpp
    DigitalTwinWorkbenchContribution.h
)

add_library(${TARGET_NAME} STATIC ${SOURCES})
add_library(SMRobotWorkbenchDigitalTwin::${TARGET_NAME} ALIAS ${TARGET_NAME})

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${PACKAGE_NAME}/include/${TARGET_NAME}>
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        SMRobotWorkbenchCommon::WorkbenchCommon
)

target_compile_features(${TARGET_NAME}
    PUBLIC
        cxx_std_17
)

set(${TARGET_NAME}_INSTALL_PUBLIC_DEPENDENCIES
    SMRobotWorkbenchCommon::WorkbenchCommon
)

set_target_properties(${TARGET_NAME}
    PROPERTIES
        FOLDER SMRobotWorkbenchDigitalTwin
        DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
        RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX}
        SMROBOT_WORKBENCH_REGISTRAR_HEADER "DigitalTwinWorkbenchContribution.h"
        SMROBOT_WORKBENCH_REGISTRAR_FUNCTION "robot_qt_viewer::registerDigitalTwinWorkbenchContribution"
        EXPORT_PROPERTIES "SMROBOT_WORKBENCH_REGISTRAR_HEADER;SMROBOT_WORKBENCH_REGISTRAR_FUNCTION"
)

set(${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR ${CMAKE_CURRENT_BINARY_DIR}/install_headers)
file(REMOVE_RECURSE ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR})
file(MAKE_DIRECTORY ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR}/${TARGET_NAME})
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/DigitalTwinWorkbenchContribution.h
    ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR}/${TARGET_NAME}/DigitalTwinWorkbenchContribution.h
    COPYONLY)

function_InstallTarget(
    TARGET_NAME ${TARGET_NAME}
    PACKAGE_NAME ${PACKAGE_NAME}
    PACKAGE_INTERFACE_HEADERS ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR}
)
