#pragma once

#include "RobotQtViewerLocalization.h"
#include "RobotQtViewerWorkbenchLifecycle.h"

class DigitalTwinWorkbenchWidget;

namespace robot_qt_viewer
{
    class DigitalTwinWorkbenchLifecycle final : public IRobotQtViewerWorkbenchLifecycle
    {
    public:
        explicit DigitalTwinWorkbenchLifecycle(DigitalTwinWorkbenchWidget& widget);

        RobotQtViewerWorkbenchTransitionResult prepareDeactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) override;
        RobotQtViewerWorkbenchTransitionResult deactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) override;
        RobotQtViewerWorkbenchTransitionResult activate(
            const RobotQtViewerWorkbenchActivationContext& context) override;
        void releaseProject(
            const RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept override;
        void shutdown(
            const RobotQtViewerWorkbenchShutdownContext& context) noexcept override;

    private:
        DigitalTwinWorkbenchWidget& m_widget;
    };

    class DigitalTwinWorkbenchLanguageParticipant final
        : public IRobotQtViewerLanguageParticipant
    {
    public:
        explicit DigitalTwinWorkbenchLanguageParticipant(DigitalTwinWorkbenchWidget& widget);
        void retranslateUi(
            const RobotQtViewerLocalizationService& localization) noexcept override;

    private:
        DigitalTwinWorkbenchWidget& m_widget;
    };
}
