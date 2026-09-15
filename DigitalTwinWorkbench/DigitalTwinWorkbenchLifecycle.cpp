#include "DigitalTwinWorkbenchLifecycle.h"

#include "DigitalTwinWorkbenchWidget.h"
#include "RobotQtViewerLocalization.h"

namespace robot_qt_viewer
{
    DigitalTwinWorkbenchLifecycle::DigitalTwinWorkbenchLifecycle(
        DigitalTwinWorkbenchWidget& widget)
        : m_widget(widget)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    DigitalTwinWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult DigitalTwinWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        m_widget.deactivate();
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult DigitalTwinWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        return workbenchTransitionSucceeded();
    }

    void DigitalTwinWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept
    {
        m_widget.deactivate();
    }

    void DigitalTwinWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext&) noexcept
    {
        m_widget.deactivate();
    }

    DigitalTwinWorkbenchLanguageParticipant::DigitalTwinWorkbenchLanguageParticipant(
        DigitalTwinWorkbenchWidget& widget)
        : m_widget(widget)
    {
    }

    void DigitalTwinWorkbenchLanguageParticipant::retranslateUi(
        const RobotQtViewerLocalizationService& localization) noexcept
    {
        m_widget.setLocalizationService(&localization);
    }
}
