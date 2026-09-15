#pragma once

#include <functional>
#include <QStringList>
#include <QWidget>
#include <memory>
#include <vector>

class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QTextEdit;
class QFrame;
class QGroupBox;
class QTimer;
class RobotController;
namespace robot_qt_viewer { class RobotQtViewerLocalizationService; }

class DigitalTwinWorkbenchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DigitalTwinWorkbenchWidget(QWidget* parent = nullptr);
    void setLocalizationService(
        const robot_qt_viewer::RobotQtViewerLocalizationService* localization);
    void setJointsUpdatedHandler(std::function<void(const std::vector<float>&, bool)> handler);
    bool digitalTwinActive() const;
    void deactivate();
    void setSceneRobotOptions(const QStringList& labels, const QStringList& robotIds);
    void setRealRobotOptions(const QStringList& labels);
    void setMappingSummary(const QStringList& lines);
    void appendMappingStatus(const QString& text);
    QStringList realRobotNames() const;
    int realRobotAxisCount(const QString& realRobotName) const;

signals:
    void mappingAddRequested(const QString& sceneRobotId, const QString& sceneRobotLabel, const QString& realRobotName);
    void mappingRemoveLastRequested();
    void mappingConfirmRequested();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onStopClicked();
    void onPingClicked();
    void onGetPositionClicked();
    void onDigitalTwinClicked();
    void onSendFileClicked();
    void onToggleStatusAreaClicked();
    void onAddMappingClicked();
    void onRemoveMappingClicked();
    void onConfirmMappingClicked();
    void updateConnectionStatus(bool connected);
    void updatePingResult(const QString& result);

private:
    void setupUi();
    void setupConnections();
    void retranslateUi();
    QString uiText(const QString& key) const;
    void appendStatus(const QString& text);

    std::function<void(const std::vector<float>&, bool)> m_jointsUpdatedHandler;
    std::shared_ptr<RobotController> m_controller;
    const robot_qt_viewer::RobotQtViewerLocalizationService* m_localization = nullptr;
    QLineEdit* m_ipInput = nullptr;
    QLineEdit* m_portInput = nullptr;
    QLineEdit* m_ftpPathInput = nullptr;
    QLineEdit* m_ftpUserInput = nullptr;
    QLineEdit* m_ftpPassInput = nullptr;
    QLineEdit* m_localPathInput = nullptr;
    QComboBox* m_sceneRobotCombo = nullptr;
    QComboBox* m_realRobotCombo = nullptr;
    QGroupBox* m_connectionGroup = nullptr;
    QLabel* m_ipLabel = nullptr;
    QLabel* m_portLabel = nullptr;
    QLabel* m_ftpPathLabel = nullptr;
    QLabel* m_ftpUserLabel = nullptr;
    QLabel* m_ftpPassLabel = nullptr;
    QLabel* m_localPathLabel = nullptr;
    QLabel* m_sceneRobotLabel = nullptr;
    QLabel* m_realRobotLabel = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_pingButton = nullptr;
    QPushButton* m_connectButton = nullptr;
    QPushButton* m_disconnectButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_getPositionButton = nullptr;
    QPushButton* m_digitalTwinButton = nullptr;
    QPushButton* m_sendFileButton = nullptr;
    QPushButton* m_toggleStatusAreaButton = nullptr;
    QPushButton* m_addMappingButton = nullptr;
    QPushButton* m_removeMappingButton = nullptr;
    QPushButton* m_confirmMappingButton = nullptr;
    QGroupBox* m_programGroup = nullptr;
    QGroupBox* m_ftpGroup = nullptr;
    QGroupBox* m_mappingGroup = nullptr;
    QLabel* m_stateLabel = nullptr;
    QTextEdit* m_pingResultArea = nullptr;
    QFrame* m_moreOptionsFrame = nullptr;
    bool m_digitalTwinActive = false;
    bool m_ftpsend = false;
    QTimer* m_programFinishTimer = nullptr;
};
