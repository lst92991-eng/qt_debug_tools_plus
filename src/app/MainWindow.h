#pragma once

#include "core/DebugCore.h"

#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVariantMap>

#include <memory>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    void scanPlugins();
    void populatePluginUi();
    void applySelectedPhysicalConfig();
    void connectDevice();
    void disconnectDevice();
    void saveSession();
    void loadSession();
    void saveHistory();
    void loadHistory();
    void clearHistory();
    void editChannelMap();
    void scanSerialBaudRates();
    void detachPluginPages();
    void showSidePage(int index);
    void setActivityButtonState(QToolButton* activeButton);
    void updatePluginOverview();
    void updateConfigPage();
    void appendActivity(const QString& message);
    void updateTrafficCounters();
    void setConnected(bool connected);
    IPhysicalPlugin* selectedPhysical() const;
    IProtocolPlugin* selectedProtocol() const;
    QString pluginRoot() const;

    std::unique_ptr<Ui::MainWindow> m_ui;
    DebugCore* m_core = nullptr;
    QComboBox* m_physicalCombo = nullptr;
    QComboBox* m_protocolCombo = nullptr;
    QPushButton* m_configButton = nullptr;
    QPushButton* m_connectButton = nullptr;
    QPushButton* m_disconnectButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_pluginSummaryLabel = nullptr;
    QLabel* m_rxCounterLabel = nullptr;
    QLabel* m_txCounterLabel = nullptr;
    QLabel* m_stagePhysicalValue = nullptr;
    QLabel* m_stageProtocolValue = nullptr;
    QLabel* m_configTitleLabel = nullptr;
    QTableWidget* m_configTable = nullptr;
    QStackedWidget* m_sideStack = nullptr;
    QToolButton* m_activityConfigButton = nullptr;
    QToolButton* m_activityPluginsButton = nullptr;
    QToolButton* m_activityDashboardButton = nullptr;
    QToolButton* m_activityOutputButton = nullptr;
    QTabWidget* m_visualTabs = nullptr;
    QTabWidget* m_controlTabs = nullptr;
    QTreeWidget* m_pluginTree = nullptr;
    QPlainTextEdit* m_activityLog = nullptr;
    QHash<QString, QVariantMap> m_physicalConfigs;
    QMetaObject::Connection m_activeStatusConnection;
    quint64 m_rxFrames = 0;
    quint64 m_txCommands = 0;
};
