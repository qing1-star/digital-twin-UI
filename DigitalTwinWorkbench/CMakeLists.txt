cmake_minimum_required(VERSION 3.20)

set(TARGET_NAME DigitalTwinWorkbench)

set(CMAKE_AUTOMOC ON)

set(SOURCES
    DigitalTwinModuleController.cpp
    DigitalTwinModuleController.h
    DigitalTwinWorkbenchWidget.cpp
    DigitalTwinWorkbenchWidget.h
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
        Qt5::Widgets
        SMRobotWorkbenchCommon::WorkbenchCommon
        SMRobotWorkbenchCommon::RobotQtModulesShared
        SMRobotSpray::DigitalTwinCommunicate
)

target_compile_features(${TARGET_NAME}
    PUBLIC
        cxx_std_17
)

set(${TARGET_NAME}_INSTALL_PUBLIC_DEPENDENCIES
    Qt5::Widgets
    SMRobotWorkbenchCommon::WorkbenchCommon
    SMRobotWorkbenchCommon::RobotQtModulesShared
    SMRobotSpray::DigitalTwinCommunicate
)

set_target_properties(${TARGET_NAME}
    PROPERTIES
        FOLDER SMRobotWorkbenchDigitalTwin
)

set(${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR ${CMAKE_CURRENT_BINARY_DIR}/install_headers)
file(MAKE_DIRECTORY ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR})
foreach(header_ IN LISTS SOURCES)
    if(header_ MATCHES "\\.h$")
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/${header_}
            ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR}/${TARGET_NAME}/${header_}
            COPYONLY)
    endif()
endforeach()

if(ExportPacakge${PACKAGE_NAME}_${TARGET_NAME})
    function_InstallTarget(
        TARGET_NAME ${TARGET_NAME}
        PACKAGE_NAME ${PACKAGE_NAME}
        PACKAGE_INTERFACE_HEADERS ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR}
    )
endif()
