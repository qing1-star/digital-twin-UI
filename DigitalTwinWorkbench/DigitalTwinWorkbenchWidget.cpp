#include "DigitalTwinWorkbenchWidget.h"

#include <Communicate/RobotController.h>
#include "RobotQtViewerLocalization.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QScrollArea>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

DigitalTwinWorkbenchWidget::DigitalTwinWorkbenchWidget(QWidget* parent)
    : QWidget(parent)
    , m_controller(std::make_shared<RobotController>())
{
    if(QApplication* application = qobject_cast<QApplication*>(QApplication::instance())) {
        m_localization =
            robot_qt_viewer::RobotQtViewerLocalizationService::installedOnApplication(
                *application);
    }
    setupUi();
    setupConnections();
    retranslateUi();
    updateConnectionStatus(false);
}

void DigitalTwinWorkbenchWidget::setLocalizationService(
    const robot_qt_viewer::RobotQtViewerLocalizationService* localization)
{
    m_localization = localization;
    retranslateUi();
}

void DigitalTwinWorkbenchWidget::setJointsUpdatedHandler(std::function<void(const std::vector<float>&, bool)> handler)
{
    m_jointsUpdatedHandler = std::move(handler);
}

bool DigitalTwinWorkbenchWidget::digitalTwinActive() const
{
    return m_digitalTwinActive;
}

void DigitalTwinWorkbenchWidget::deactivate()
{
    if(!m_digitalTwinActive || m_controller == nullptr) {
        return;
    }

    m_controller->stopDigitalTwin();
    m_digitalTwinActive = false;
    if(m_digitalTwinButton != nullptr) {
        m_digitalTwinButton->setText(uiText("digitalTwin.button.digitalTwin"));
    }
    appendStatus(uiText("digitalTwin.log.closed"));
}

QString DigitalTwinWorkbenchWidget::uiText(const QString& key) const
{
    return m_localization != nullptr ? m_localization->text(key, key) : key;
}

void DigitalTwinWorkbenchWidget::setupUi()
{
    auto* root = new QVBoxLayout(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    m_connectionGroup = new QGroupBox(content);
    auto* topGroup = m_connectionGroup;
    auto* topLayout = new QGridLayout(topGroup);
    m_ipLabel = new QLabel(topGroup);
    topLayout->addWidget(m_ipLabel, 0, 0);
    m_ipInput = new QLineEdit("192.168.0.2", topGroup);
    topLayout->addWidget(m_ipInput, 0, 1);
    m_portLabel = new QLabel(topGroup);
    topLayout->addWidget(m_portLabel, 0, 2);
    m_portInput = new QLineEdit("80", topGroup);
    m_portInput->setMaximumWidth(80);
    topLayout->addWidget(m_portInput, 0, 3);
    m_pingButton = new QPushButton(topGroup);
    m_pingButton->setMinimumWidth(72);
    topLayout->addWidget(m_pingButton, 1, 0, 1, 2, Qt::AlignLeft);
    contentLayout->addWidget(topGroup);

    m_pingResultArea = new QTextEdit(this);
    m_pingResultArea->setReadOnly(true);
    m_pingResultArea->setMaximumHeight(100);
    m_pingResultArea->setVisible(false);
    contentLayout->addWidget(m_pingResultArea);

    auto* buttonRow = new QHBoxLayout();
    m_connectButton = new QPushButton(this);
    m_disconnectButton = new QPushButton(this);
    m_stopButton = new QPushButton(this);
    m_getPositionButton = new QPushButton(this);
    m_digitalTwinButton = new QPushButton(this);
    m_connectButton->setStyleSheet("font-size: 12px;");
    m_disconnectButton->setStyleSheet("font-size: 12px;");
    m_stopButton->setStyleSheet("font-size: 12px;");
    m_getPositionButton->setMinimumHeight(44);
    m_digitalTwinButton->setMinimumHeight(44);
    m_getPositionButton->setStyleSheet("font-size: 12px; line-height: 1.15;");
    m_digitalTwinButton->setStyleSheet("font-size: 12px; line-height: 1.15;");
    buttonRow->addWidget(m_connectButton);
    buttonRow->addWidget(m_disconnectButton);
    buttonRow->addWidget(m_stopButton);
    buttonRow->addWidget(m_getPositionButton);
    buttonRow->addWidget(m_digitalTwinButton);
    contentLayout->addLayout(buttonRow);

    m_stateLabel = new QLabel(this);
    m_stateLabel->setStyleSheet("color: red; font-weight: bold;");
    contentLayout->addWidget(m_stateLabel);

    m_moreOptionsFrame = new QFrame(content);
    auto* optionsLayout = new QVBoxLayout(m_moreOptionsFrame);
    m_moreOptionsFrame->setVisible(false);

    auto* ftpGroup = new QGroupBox(m_moreOptionsFrame);
    m_ftpGroup = ftpGroup;
    auto* ftpLayout = new QGridLayout(ftpGroup);
    m_ftpPathLabel = new QLabel(ftpGroup);
    ftpLayout->addWidget(m_ftpPathLabel, 0, 0);
    m_ftpPathInput = new QLineEdit("/Robot", ftpGroup);
    ftpLayout->addWidget(m_ftpPathInput, 0, 1);
    m_ftpUserLabel = new QLabel(ftpGroup);
    ftpLayout->addWidget(m_ftpUserLabel, 1, 0);
    m_ftpUserInput = new QLineEdit("Default User", ftpGroup);
    ftpLayout->addWidget(m_ftpUserInput, 1, 1);
    m_ftpPassLabel = new QLabel(ftpGroup);
    ftpLayout->addWidget(m_ftpPassLabel, 2, 0);
    m_ftpPassInput = new QLineEdit("robotics", ftpGroup);
    m_ftpPassInput->setEchoMode(QLineEdit::Password);
    ftpLayout->addWidget(m_ftpPassInput, 2, 1);
    ftpGroup->setVisible(false);
    optionsLayout->addWidget(ftpGroup);

    auto* localGroup = new QGroupBox(m_moreOptionsFrame);
    m_programGroup = localGroup;
    auto* localLayout = new QGridLayout(localGroup);
    m_localPathLabel = new QLabel(localGroup);
    localLayout->addWidget(m_localPathLabel, 0, 0);
    m_localPathInput = new QLineEdit(localGroup);
    localLayout->addWidget(m_localPathInput, 0, 1);
    m_browseButton = new QPushButton(localGroup);
    m_browseButton->setMinimumWidth(72);
    connect(m_browseButton, &QPushButton::clicked, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(this, uiText("digitalTwin.dialog.selectLocalFile"));
        if(!filePath.isEmpty() && m_localPathInput != nullptr) {
            m_localPathInput->setText(filePath);
        }
    });
    localLayout->addWidget(m_browseButton, 1, 0);
    m_sendFileButton = new QPushButton(localGroup);
    m_sendFileButton->setMinimumWidth(72);
    localLayout->addWidget(m_sendFileButton, 1, 1);
    contentLayout->addWidget(localGroup);

    m_mappingGroup = new QGroupBox(m_moreOptionsFrame);
    auto* mappingLayout = new QGridLayout(m_mappingGroup);
    m_sceneRobotLabel = new QLabel(m_mappingGroup);
    mappingLayout->addWidget(m_sceneRobotLabel, 0, 0);
    m_sceneRobotCombo = new QComboBox(m_mappingGroup);
    mappingLayout->addWidget(m_sceneRobotCombo, 0, 1);
    m_realRobotLabel = new QLabel(m_mappingGroup);
    mappingLayout->addWidget(m_realRobotLabel, 1, 0);
    m_realRobotCombo = new QComboBox(m_mappingGroup);
    mappingLayout->addWidget(m_realRobotCombo, 1, 1);
    m_addMappingButton = new QPushButton(m_mappingGroup);
    mappingLayout->addWidget(m_addMappingButton, 2, 0);
    m_removeMappingButton = new QPushButton(m_mappingGroup);
    mappingLayout->addWidget(m_removeMappingButton, 2, 1);
    m_confirmMappingButton = new QPushButton(m_mappingGroup);
    mappingLayout->addWidget(m_confirmMappingButton, 3, 0, 1, 2);
    m_mappingGroup->setVisible(false);
    optionsLayout->addWidget(m_mappingGroup);

    m_toggleStatusAreaButton = new QPushButton(this);
    contentLayout->addWidget(m_toggleStatusAreaButton);
    contentLayout->addWidget(m_moreOptionsFrame);
    contentLayout->addStretch(1);

    m_programFinishTimer = new QTimer(this);
    m_programFinishTimer->setInterval(1000);

    retranslateUi();
}

void DigitalTwinWorkbenchWidget::setupConnections()
{
    connect(m_connectButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onDisconnectClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onStopClicked);
    connect(m_pingButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onPingClicked);
    connect(m_getPositionButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onGetPositionClicked);
    connect(m_digitalTwinButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onDigitalTwinClicked);
    connect(m_sendFileButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onSendFileClicked);
    connect(m_toggleStatusAreaButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onToggleStatusAreaClicked);
    connect(m_addMappingButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onAddMappingClicked);
    connect(m_removeMappingButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onRemoveMappingClicked);
    connect(m_confirmMappingButton, &QPushButton::clicked, this, &DigitalTwinWorkbenchWidget::onConfirmMappingClicked);

    m_controller->setStatusCallback([this](bool connected) {
        QMetaObject::invokeMethod(this, [this, connected]() { updateConnectionStatus(connected); }, Qt::QueuedConnection);
    });
    m_controller->setPingResultCallback([this](const std::string& result) {
        QMetaObject::invokeMethod(this, [this, result]() { updatePingResult(QString::fromLocal8Bit(result.c_str())); }, Qt::QueuedConnection);
    });
    m_controller->setConnectionLostCallback([this]() {
        QMetaObject::invokeMethod(this, [this]() { updateConnectionStatus(false); }, Qt::QueuedConnection);
    });
    m_controller->setJointsUpdatedCallback([this](const std::vector<float>& angles) {
        if(m_jointsUpdatedHandler) {
            m_jointsUpdatedHandler(angles, true);
        }
    });
}

void DigitalTwinWorkbenchWidget::appendStatus(const QString& text)
{
    if(m_pingResultArea != nullptr) {
        m_pingResultArea->append(text);
        m_pingResultArea->moveCursor(QTextCursor::End);
    }
}

void DigitalTwinWorkbenchWidget::onConnectClicked()
{
    const QString ip = m_ipInput->text().trimmed();
    if(ip.isEmpty()) {
        QMessageBox::warning(this, QString(), uiText("digitalTwin.message.ipEmpty"));
        return;
    }

    m_stateLabel->setText(uiText("digitalTwin.status.connecting"));
    m_stateLabel->setStyleSheet("color: orange; font-weight: bold;");

    const int port = m_portInput->text().toInt();
    const bool ok = m_controller->connectRobot(ip.toStdString(), port);

    updateConnectionStatus(ok);
    appendStatus(ok ? uiText("digitalTwin.log.connected") : uiText("digitalTwin.log.connectionFailed"));
}

void DigitalTwinWorkbenchWidget::onDisconnectClicked()
{
    m_controller->disconnectRobot();
    updateConnectionStatus(false);
    appendStatus(uiText("digitalTwin.log.disconnected"));
}

void DigitalTwinWorkbenchWidget::onStopClicked()
{
    m_controller->stopRobot();
    updateConnectionStatus(false);
    appendStatus(uiText("digitalTwin.log.stopSent"));
}

void DigitalTwinWorkbenchWidget::onPingClicked()
{
    const QString ip = m_ipInput->text().trimmed();
    if(ip.isEmpty()) {
        QMessageBox::warning(this, QString(), uiText("digitalTwin.message.inputIp"));
        return;
    }

    appendStatus(uiText("digitalTwin.log.pinging").arg(ip));
    updatePingResult(QString::fromLocal8Bit(m_controller->pingRobot(ip.toStdString()).c_str()));
}

void DigitalTwinWorkbenchWidget::onGetPositionClicked()
{
    if(!m_controller->isConnected()) {
        QMessageBox::warning(this, QString(), uiText("digitalTwin.message.connectFirst"));
        return;
    }

    const auto position = m_controller->getRobotPosition();
    QString text = uiText("digitalTwin.log.robotPosition") + "\n";
    for(std::size_t i = 0; i < position.size(); ++i) {
        text += uiText("digitalTwin.log.joint").arg(i + 1).arg(position[static_cast<std::size_t>(i)], 0, 'f', 3);
        if(i + 1 < position.size()) {
            text += "\n";
        }
    }
    appendStatus(text);
    m_stateLabel->setText(uiText("digitalTwin.status.getPose"));
    if(m_jointsUpdatedHandler) {
        m_jointsUpdatedHandler(position, false);
    }
}

void DigitalTwinWorkbenchWidget::onDigitalTwinClicked()
{
    if(!m_controller->isConnected()) {
        QMessageBox::warning(this, QString(), uiText("digitalTwin.message.connectFirst"));
        return;
    }

    m_digitalTwinActive = !m_digitalTwinActive;
    const bool ok = m_digitalTwinActive ? m_controller->startDigitalTwin() : m_controller->stopDigitalTwin();
    if(ok) {
        m_digitalTwinButton->setText(m_digitalTwinActive ? uiText("digitalTwin.button.digitalTwinOn") : uiText("digitalTwin.button.digitalTwin"));
        appendStatus(m_digitalTwinActive ? uiText("digitalTwin.log.started") : uiText("digitalTwin.log.closed"));
        m_stateLabel->setText(m_digitalTwinActive ? uiText("digitalTwin.status.running") : uiText("digitalTwin.status.stopped"));
    } else {
        m_digitalTwinActive = false;
        appendStatus(uiText("digitalTwin.message.connectFirst"));
    }
}

void DigitalTwinWorkbenchWidget::onSendFileClicked()
{
    const QString localPath = m_localPathInput->text().trimmed();
    if(localPath.isEmpty()) {
        appendStatus(uiText("digitalTwin.message.localPathEmpty"));
        return;
    }

    QFileInfo fileInfo(localPath);
    if(!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::warning(this, uiText("digitalTwin.message.warning"), uiText("digitalTwin.message.fileInvalid"));
        return;
    }

    const QString ip = m_ipInput->text().trimmed();
    if(ip.isEmpty()) {
        QMessageBox::warning(this, uiText("digitalTwin.message.warning"), uiText("digitalTwin.message.ipEmpty"));
        return;
    }

    bool portOk = false;
    const int port = m_portInput->text().trimmed().toInt(&portOk);
    const int ftpPort = portOk && port > 0 && port < 65536 ? port : 80;

    const bool ok = m_controller->sendFileToFTP(
        ip.toStdString(),
        ftpPort,
        localPath.toStdString(),
        m_ftpPathInput->text().trimmed().toStdString(),
        m_ftpUserInput->text().trimmed().toStdString(),
        m_ftpPassInput->text().trimmed().toStdString());

    m_ftpsend = ok;
    appendStatus(uiText("digitalTwin.log.uploadStart").arg(localPath).arg(m_ftpPathInput->text().trimmed()).arg(ip).arg(ftpPort));
    appendStatus(ok ? uiText("digitalTwin.log.fileUploaded") : uiText("digitalTwin.log.fileUploadFailed"));
}

void DigitalTwinWorkbenchWidget::onToggleStatusAreaClicked()
{
    if(m_pingResultArea == nullptr || m_toggleStatusAreaButton == nullptr || m_moreOptionsFrame == nullptr) {
        return;
    }
    const bool visible = !m_pingResultArea->isVisible();
    m_pingResultArea->setVisible(visible);
    if(m_moreOptionsFrame != nullptr) {
        m_moreOptionsFrame->setVisible(visible);
    }
    if(m_ftpGroup != nullptr) {
        m_ftpGroup->setVisible(visible);
    }
    if(m_mappingGroup != nullptr) {
        m_mappingGroup->setVisible(visible);
    }
    m_toggleStatusAreaButton->setText(visible ? uiText("digitalTwin.button.hideAll") : uiText("digitalTwin.button.showAll"));
}

void DigitalTwinWorkbenchWidget::onAddMappingClicked()
{
    if(m_sceneRobotCombo == nullptr || m_realRobotCombo == nullptr || m_sceneRobotCombo->count() == 0) {
        appendMappingStatus(uiText("digitalTwin.mapping.noSceneRobot"));
        return;
    }

    emit mappingAddRequested(
        m_sceneRobotCombo->currentData().toString(),
        m_sceneRobotCombo->currentText(),
        m_realRobotCombo->currentText());
}

void DigitalTwinWorkbenchWidget::onRemoveMappingClicked()
{
    emit mappingRemoveLastRequested();
}

void DigitalTwinWorkbenchWidget::onConfirmMappingClicked()
{
    emit mappingConfirmRequested();
}

void DigitalTwinWorkbenchWidget::updateConnectionStatus(bool connected)
{
    if(m_stateLabel == nullptr) {
        return;
    }

    m_stateLabel->setText(connected ? uiText("digitalTwin.status.connected") : uiText("digitalTwin.status.disconnected"));
    m_stateLabel->setStyleSheet(connected
        ? "color: green; font-weight: bold;"
        : "color: red; font-weight: bold;");

    if(m_connectButton != nullptr) {
        m_connectButton->setEnabled(!connected);
    }
    if(m_disconnectButton != nullptr) {
        m_disconnectButton->setEnabled(connected);
    }
    if(!connected) {
        m_digitalTwinActive = false;
        if(m_digitalTwinButton != nullptr) {
            m_digitalTwinButton->setText(uiText("digitalTwin.button.digitalTwin"));
        }
    }
}

void DigitalTwinWorkbenchWidget::updatePingResult(const QString& result)
{
    appendStatus(result);
    appendStatus(QStringLiteral("------------------------"));
}

void DigitalTwinWorkbenchWidget::retranslateUi()
{
    if(m_pingButton != nullptr) m_pingButton->setText(uiText("digitalTwin.button.ping"));
    if(m_connectButton != nullptr) m_connectButton->setText(uiText("digitalTwin.button.connect"));
    if(m_disconnectButton != nullptr) m_disconnectButton->setText(uiText("digitalTwin.button.disconnect"));
    if(m_stopButton != nullptr) m_stopButton->setText(uiText("digitalTwin.button.stop"));
    if(m_getPositionButton != nullptr) {
        QString text = uiText("digitalTwin.button.getPosition");
        m_getPositionButton->setText(text.replace(' ', '\n'));
    }
    if(m_digitalTwinButton != nullptr) {
        QString text = m_digitalTwinActive ? uiText("digitalTwin.button.digitalTwinOn") : uiText("digitalTwin.button.digitalTwin");
        text.replace(' ', '\n');
        m_digitalTwinButton->setText(text);
    }
    if(m_toggleStatusAreaButton != nullptr) {
        m_toggleStatusAreaButton->setText(m_pingResultArea != nullptr && m_pingResultArea->isVisible()
            ? uiText("digitalTwin.button.hideAll") : uiText("digitalTwin.button.showAll"));
    }
    if(m_stateLabel != nullptr) {
        m_stateLabel->setText(m_controller != nullptr && m_controller->isConnected()
            ? uiText("digitalTwin.status.connected")
            : uiText("digitalTwin.status.disconnected"));
    }
    if(m_connectionGroup != nullptr) {
        m_connectionGroup->setTitle(uiText("digitalTwin.group.connection"));
    }
    if(m_ipLabel != nullptr) m_ipLabel->setText(uiText("digitalTwin.label.robotIp"));
    if(m_portLabel != nullptr) m_portLabel->setText(uiText("digitalTwin.label.port"));
    if(m_pingButton != nullptr) m_pingButton->setText(uiText("digitalTwin.button.ping"));
    if(m_ftpGroup != nullptr) m_ftpGroup->setTitle(uiText("digitalTwin.group.ftp"));
    if(m_ftpPathLabel != nullptr) m_ftpPathLabel->setText(uiText("digitalTwin.label.ftpPath"));
    if(m_ftpUserLabel != nullptr) m_ftpUserLabel->setText(uiText("digitalTwin.label.user"));
    if(m_ftpPassLabel != nullptr) m_ftpPassLabel->setText(uiText("digitalTwin.label.password"));
    if(m_programGroup != nullptr) m_programGroup->setTitle(uiText("digitalTwin.group.programFile"));
    if(m_localPathLabel != nullptr) m_localPathLabel->setText(uiText("digitalTwin.label.localFile"));
    if(m_browseButton != nullptr) m_browseButton->setText(uiText("digitalTwin.button.browse"));
    if(m_sendFileButton != nullptr) m_sendFileButton->setText(uiText("digitalTwin.button.send"));
    if(m_mappingGroup != nullptr) m_mappingGroup->setTitle(uiText("digitalTwin.group.mapping"));
    if(m_sceneRobotLabel != nullptr) m_sceneRobotLabel->setText(uiText("digitalTwin.label.sceneRobot"));
    if(m_realRobotLabel != nullptr) m_realRobotLabel->setText(uiText("digitalTwin.label.realRobot"));
    if(m_addMappingButton != nullptr) m_addMappingButton->setText(uiText("digitalTwin.button.addMapping"));
    if(m_removeMappingButton != nullptr) m_removeMappingButton->setText(uiText("digitalTwin.button.removeMapping"));
    if(m_confirmMappingButton != nullptr) m_confirmMappingButton->setText(uiText("digitalTwin.button.confirmMapping"));
    if(m_stateLabel != nullptr && m_controller != nullptr) {
        m_stateLabel->setText(m_controller->isConnected()
            ? uiText("digitalTwin.status.connected")
            : uiText("digitalTwin.status.disconnected"));
    }
}
void DigitalTwinWorkbenchWidget::setSceneRobotOptions(const QStringList& labels, const QStringList& robotIds)
{
    if(m_sceneRobotCombo == nullptr) {
        return;
    }

    m_sceneRobotCombo->clear();
    const int count = std::min(labels.size(), robotIds.size());
    for(int i = 0; i < count; ++i) {
        m_sceneRobotCombo->addItem(labels.at(i), robotIds.at(i));
    }
}

void DigitalTwinWorkbenchWidget::setRealRobotOptions(const QStringList& labels)
{
    if(m_realRobotCombo == nullptr) {
        return;
    }

    m_realRobotCombo->clear();
    m_realRobotCombo->addItems(labels);
}

void DigitalTwinWorkbenchWidget::setMappingSummary(const QStringList& lines)
{
    if(m_pingResultArea == nullptr) {
        return;
    }

    m_pingResultArea->clear();
    for(const QString& line : lines) {
        m_pingResultArea->append(line);
    }
    m_pingResultArea->moveCursor(QTextCursor::End);
}

void DigitalTwinWorkbenchWidget::appendMappingStatus(const QString& text)
{
    appendStatus(text);
}

QStringList DigitalTwinWorkbenchWidget::realRobotNames() const
{
    QStringList names;
    if(m_controller == nullptr) {
        return names;
    }

    for(const auto& unit : m_controller->mechanicalUnits()) {
        names.push_back(QString::fromStdString(unit.name));
    }
    return names;
}

int DigitalTwinWorkbenchWidget::realRobotAxisCount(const QString& realRobotName) const
{
    if(m_controller == nullptr) {
        return 0;
    }

    for(const auto& unit : m_controller->mechanicalUnits()) {
        if(QString::fromStdString(unit.name).compare(realRobotName, Qt::CaseInsensitive) == 0) {
            return unit.axisCount;
        }
    }
    return 0;
}
