#pragma once
#include <gtk/gtk.h>
#include "models.h"
#include "scanner.h"
#include <mutex>
#include <atomic>
#include <thread>

class App {
public:
    int run(int argc, char** argv);
    GtkWidget* _window = nullptr;
    GtkWidget* _sidebar = nullptr;
    GtkWidget* _stack = nullptr;
    GtkWidget* _progressBar = nullptr;
    GtkWidget* _statusLabel = nullptr;

    // Overview tab
    GtkWidget* _kpiDevices = nullptr;
    GtkWidget* _kpiUnknown = nullptr;
    GtkWidget* _kpiAlerts = nullptr;
    GtkWidget* _kpiLatency = nullptr;
    GtkWidget* _topoDrawing = nullptr;
    GtkWidget* _changesView = nullptr;
    GtkListStore* _changesStore = nullptr;

    // Devices tab
    GtkWidget* _devicesView = nullptr;
    GtkListStore* _devicesStore = nullptr;
    GtkWidget* _searchEntry = nullptr;
    GtkWidget* _detailPanel = nullptr;
    GtkWidget* _detailName = nullptr;
    GtkWidget* _detailType = nullptr;
    GtkWidget* _detailIP = nullptr;
    GtkWidget* _detailMAC = nullptr;
    GtkWidget* _detailVendor = nullptr;
    GtkWidget* _detailPorts = nullptr;
    GtkWidget* _detailConfidence = nullptr;
    GtkWidget* _detailTrust = nullptr;
    GtkWidget* _detailNotes = nullptr;
    GtkWidget* _detailRisk = nullptr;
    GtkWidget* _detailAlts = nullptr;

    // Alerts tab
    GtkWidget* _alertsView = nullptr;
    GtkListStore* _alertsStore = nullptr;
    GtkWidget* _alertWhat = nullptr;
    GtkWidget* _alertWhy = nullptr;
    GtkWidget* _alertDo = nullptr;
    GtkWidget* _rulesView = nullptr;
    GtkListStore* _rulesStore = nullptr;

    // Tools tab
    GtkWidget* _toolOutput = nullptr;
    GtkWidget* _toolTarget = nullptr;

    // Ledger tab
    GtkWidget* _ledgerView = nullptr;
    GtkListStore* _ledgerStore = nullptr;

    // Privacy tab
    GtkWidget* _privStats = nullptr;
    GtkWidget* _apiStatus = nullptr;
    GtkWidget* _apiKeyLabel = nullptr;

    // Data
    ScanEngine _scanner;
    ScanResult _lastResult;
    std::vector<LedgerEntry> _ledger;
    std::vector<AlertRule> _alertRules;
    std::vector<ScanResult> _snapshots;
    std::vector<PluginHook> _pluginHooks;
    ScheduledScan _scheduledScan;
    MonitorConfig _monitorConfig;
    std::mutex _dataMutex;
    std::atomic<bool> _scanning{false};
    std::atomic<bool> _monitoring{false};
    std::atomic<bool> _apiEnabled{false};
    std::string _apiKey;
    int _apiPort = 7722;
    std::string _currentFilter = "all";
    int _selectedDevice = -1;

    void setupCSS();
    GtkWidget* createSidebar();
    GtkWidget* createOverviewTab();
    GtkWidget* createDevicesTab();
    GtkWidget* createAlertsTab();
    GtkWidget* createToolsTab();
    GtkWidget* createLedgerTab();
    GtkWidget* createPrivacyTab();

    void switchTab(const char* name);
    void startScan(const std::string& mode);
    void updateUI();
    void refreshDeviceList();
    void refreshAlerts();
    void refreshLedger();
    void refreshKPIs();
    void addLedgerEntry(const std::string& action, const std::string& details);
    void showDeviceDetail(int idx);
    void hideDeviceDetail();
    void runTool(const std::string& tool, const std::string& target);
    void startLocalApi();
    void stopLocalApi();

    static gboolean onScanDone(gpointer data);
    static gboolean onScanProgress(gpointer data);
    static gboolean drawTopology(GtkWidget* widget, cairo_t* cr, gpointer data);
};
