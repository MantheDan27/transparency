#include "app.h"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <array>

// ─── Color constants ────────────────────────────────────────────────────────
#define COL_BG       "#0b0e14"
#define COL_SIDEBAR  "#111520"
#define COL_CARD     "#181d2e"
#define COL_BORDER   "#1f2740"
#define COL_TEXT     "#c8d4f0"
#define COL_TEXT2    "#96a8d2"
#define COL_MUTED    "#4e5f85"
#define COL_ACCENT   "#3d7fff"
#define COL_GLOW     "#00e5ff"
#define COL_SUCCESS  "#00e57a"
#define COL_DANGER   "#ff4060"
#define COL_WARNING  "#ffc832"
#define COL_WATCH    "#a855f7"

struct ProgressData { int pct; std::string msg; App* app; };
struct ScanDoneData { ScanResult result; App* app; };

static std::string execCmd(const std::string& cmd) {
    std::array<char, 4096> buf;
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return "Error running command";
    while (fgets(buf.data(), buf.size(), p)) out += buf.data();
    pclose(p);
    return out;
}

// ─── Forward declarations ───────────────────────────────────────────────────
static const char* TAB_NAMES[] = {"overview","devices","alerts","tools","ledger","privacy"};
static const char* TAB_LABELS[] = {
    "\342\226\246  Overview", "\342\226\241  Devices", "\342\226\262  Alerts",
    "\342\227\206  Tools", "\342\226\244  Ledger", "\342\227\213  Privacy"
};
static GtkWidget* sidebarButtons[6] = {};

struct SidebarPair { App* app; int idx; };

// ─── Static callbacks (cannot use lambdas-with-commas inside GLib macros) ───
static void cb_sidebar_click(gpointer data) {
    auto* pair = (SidebarPair*)data;
    pair->app->switchTab(TAB_NAMES[pair->idx]);
}
static void cb_quick_scan(gpointer d) { ((App*)d)->startScan("quick"); }
static void cb_standard_scan(gpointer d) { ((App*)d)->startScan("standard"); }
static void cb_deep_scan(gpointer d) { ((App*)d)->startScan("deep"); }
static void cb_search_changed(gpointer d) { ((App*)d)->refreshDeviceList(); }

struct FilterPair { App* app; std::string val; };
static void cb_filter_click(gpointer d) {
    auto* p = (FilterPair*)d;
    p->app->_currentFilter = p->val;
    p->app->refreshDeviceList();
}

static void cb_device_selected(gpointer d) {
    auto* app = (App*)d;
    auto* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->_devicesView));
    GtkTreeIter iter;
    GtkTreeModel* model;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        int idx;
        gtk_tree_model_get(model, &iter, 8, &idx, -1);
        app->showDeviceDetail(idx);
    }
}
static void cb_close_detail(gpointer d) { ((App*)d)->hideDeviceDetail(); }

static void cb_alert_selected(gpointer d) {
    auto* app = (App*)d;
    auto* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->_alertsView));
    GtkTreeIter iter;
    GtkTreeModel* model;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        gchar *sev, *type, *dev, *desc;
        gtk_tree_model_get(model, &iter, 0, &sev, 1, &type, 2, &dev, 3, &desc, -1);
        std::lock_guard<std::mutex> lk(app->_dataMutex);
        for (auto& a : app->_lastResult.anomalies) {
            if (a.deviceIp == dev && a.type == type) {
                gtk_text_buffer_set_text(
                    gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->_alertWhat)), a.description.c_str(), -1);
                gtk_text_buffer_set_text(
                    gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->_alertWhy)), a.explanation.c_str(), -1);
                gtk_text_buffer_set_text(
                    gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->_alertDo)), a.remediation.c_str(), -1);
                break;
            }
        }
        g_free(sev); g_free(type); g_free(dev); g_free(desc);
    }
}

struct ToolPair { App* app; std::string cmd; };
static void cb_tool_click(gpointer d) {
    auto* p = (ToolPair*)d;
    const char* target = gtk_entry_get_text(GTK_ENTRY(p->app->_toolTarget));
    p->app->runTool(p->cmd, target);
}

static void cb_clear_ledger(gpointer d) {
    auto* app = (App*)d;
    std::lock_guard<std::mutex> lk(app->_dataMutex);
    app->_ledger.clear();
    app->refreshLedger();
}

static void cb_delete_all(gpointer d) {
    auto* app = (App*)d;
    std::lock_guard<std::mutex> lk(app->_dataMutex);
    app->_lastResult = {};
    app->_ledger.clear();
    app->_alertRules.clear();
    app->_snapshots.clear();
    app->updateUI();
}

static void cb_toggle_api(gpointer d) {
    auto* app = (App*)d;
    if (app->_apiEnabled) app->stopLocalApi();
    else app->startLocalApi();
}

static gboolean cb_tool_output_idle(gpointer d) {
    auto* p = (std::pair<GtkWidget*, std::string>*)d;
    auto* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(p->first));
    gtk_text_buffer_set_text(buf, p->second.c_str(), -1);
    delete p;
    return FALSE;
}

// ─── CSS Theme ──────────────────────────────────────────────────────────────
void App::setupCSS() {
    const char* css = R"CSS(
        window, .background { background-color: #0b0e14; }
        * { color: #c8d4f0; font-family: "Ubuntu", "DejaVu Sans", sans-serif; font-size: 11px; }
        .sidebar { background-color: #111520; border-right: 1px solid #1f2740; }
        .sidebar button { background: transparent; border: none; border-radius: 6px;
            padding: 10px 16px; margin: 2px 8px; color: #96a8d2; font-size: 12px; }
        .sidebar button:hover { background-color: #181d2e; color: #c8d4f0; }
        .sidebar button.active { background-color: #1a325a; color: #3d7fff;
            border-left: 3px solid #3d7fff; }
        .brand-label { color: #3d7fff; font-size: 16px; font-weight: bold;
            padding: 16px 0; }
        .version-label { color: #4e5f85; font-size: 9px; }
        .card { background-color: #181d2e; border: 1px solid #1f2740;
            border-radius: 8px; padding: 12px; margin: 6px; }
        .kpi-value { font-size: 28px; font-weight: bold; }
        .kpi-label { color: #96a8d2; font-size: 10px; }
        .kpi-green .kpi-value { color: #00e57a; }
        .kpi-amber .kpi-value { color: #ffc832; }
        .kpi-red .kpi-value { color: #ff4060; }
        .kpi-blue .kpi-value { color: #3d7fff; }
        .accent-btn { background-color: #3d7fff; color: white; border: none;
            border-radius: 6px; padding: 8px 16px; font-weight: bold; }
        .accent-btn:hover { background-color: #5a94ff; }
        .danger-btn { background-color: #ff4060; color: white; border: none;
            border-radius: 6px; padding: 8px 16px; }
        .subtle-btn { background-color: #181d2e; color: #96a8d2; border: 1px solid #1f2740;
            border-radius: 6px; padding: 6px 12px; }
        .subtle-btn:hover { background-color: #1a325a; color: #c8d4f0; }
        entry { background-color: #181d2e; border: 1px solid #1f2740; border-radius: 4px;
            color: #c8d4f0; padding: 6px 8px; }
        textview, textview text { background-color: #181d2e; color: #c8d4f0; font-family: monospace; }
        treeview { background-color: #0b0e14; color: #c8d4f0; }
        treeview:selected { background-color: #1a325a; }
        treeview header button { background-color: #111520; color: #96a8d2;
            border: none; border-bottom: 1px solid #1f2740; font-weight: bold; }
        scrollbar { background-color: #0b0e14; }
        scrollbar slider { background-color: #1f2740; border-radius: 4px; min-width: 6px; }
        scrollbar slider:hover { background-color: #3d7fff; }
        progressbar trough { background-color: #181d2e; border-radius: 4px; min-height: 6px; }
        progressbar progress { background-color: #3d7fff; border-radius: 4px; min-height: 6px; }
        .section-header { color: #3d7fff; font-size: 13px; font-weight: bold; margin: 8px 0 4px 0; }
        .detail-panel { background-color: #111520; border-left: 1px solid #1f2740; padding: 12px; }
        .detail-label { color: #96a8d2; font-size: 10px; }
        .detail-value { color: #c8d4f0; font-size: 12px; }
        .risk-box { background-color: rgba(255,64,96,0.15); border: 1px solid #ff4060;
            border-radius: 6px; padding: 8px; }
        .vm-badge { background-color: rgba(168,85,247,0.2); border: 1px solid #a855f7;
            border-radius: 4px; padding: 4px 8px; color: #a855f7; }
        notebook header { background-color: #111520; }
        notebook tab { background-color: #111520; color: #96a8d2; padding: 6px 12px; }
        notebook tab:checked { color: #3d7fff; border-bottom: 2px solid #3d7fff; }
        combobox, combobox button { background-color: #181d2e; color: #c8d4f0;
            border: 1px solid #1f2740; }
        separator { background-color: #1f2740; min-height: 1px; }
    )CSS";

    auto* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

// ─── Sidebar ────────────────────────────────────────────────────────────────

void App::switchTab(const char* name) {
    gtk_stack_set_visible_child_name(GTK_STACK(_stack), name);
    for (int i = 0; i < 6; i++) {
        auto* ctx = gtk_widget_get_style_context(sidebarButtons[i]);
        if (strcmp(TAB_NAMES[i], name) == 0)
            gtk_style_context_add_class(ctx, "active");
        else
            gtk_style_context_remove_class(ctx, "active");
    }
}

GtkWidget* App::createSidebar() {
    auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "sidebar");
    gtk_widget_set_size_request(box, 190, -1);

    auto* brand = gtk_label_new("Transparency");
    gtk_style_context_add_class(gtk_widget_get_style_context(brand), "brand-label");
    gtk_box_pack_start(GTK_BOX(box), brand, FALSE, FALSE, 0);

    auto* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 4);

    for (int i = 0; i < 6; i++) {
        auto* btn = gtk_button_new_with_label(TAB_LABELS[i]);
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        sidebarButtons[i] = btn;
        int idx = i;
        auto* sp = new SidebarPair{this, idx};
        g_signal_connect_swapped(btn, "clicked", G_CALLBACK(cb_sidebar_click), sp);
        gtk_box_pack_start(GTK_BOX(box), btn, FALSE, FALSE, 0);
    }

    // Spacer
    auto* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), spacer, TRUE, TRUE, 0);

    auto* ver = gtk_label_new("v3.4.0");
    gtk_style_context_add_class(gtk_widget_get_style_context(ver), "version-label");
    gtk_box_pack_end(GTK_BOX(box), ver, FALSE, FALSE, 8);

    return box;
}

// ─── KPI Helper ─────────────────────────────────────────────────────────────
static GtkWidget* makeKPI(const char* value, const char* label, const char* colorClass) {
    auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "card");
    gtk_style_context_add_class(gtk_widget_get_style_context(box), colorClass);

    auto* val = gtk_label_new(value);
    gtk_style_context_add_class(gtk_widget_get_style_context(val), "kpi-value");
    gtk_box_pack_start(GTK_BOX(box), val, FALSE, FALSE, 0);

    auto* lbl = gtk_label_new(label);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "kpi-label");
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    return box;
}

// ─── Topology Map Drawing ───────────────────────────────────────────────────
gboolean App::drawTopology(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* app = (App*)data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    // Background
    cairo_set_source_rgb(cr, 0.04, 0.05, 0.08);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);

    std::lock_guard<std::mutex> lk(app->_dataMutex);
    auto& devices = app->_lastResult.devices;
    if (devices.empty()) {
        cairo_set_source_rgb(cr, 0.30, 0.41, 0.62);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_move_to(cr, w/2 - 80, h/2);
        cairo_show_text(cr, "Run a scan to see topology");
        return TRUE;
    }

    double cx = w / 2.0, cy = h / 2.0;
    double radius = std::min(w, h) / 2.5;
    int n = (int)devices.size();

    // Draw lines from center to each device
    for (int i = 0; i < n; i++) {
        double angle = 2.0 * M_PI * i / n - M_PI / 2;
        double dx = cx + radius * cos(angle);
        double dy = cy + radius * sin(angle);

        cairo_set_source_rgba(cr, 0.12, 0.15, 0.25, 0.6);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, cx, cy);
        cairo_line_to(cr, dx, dy);
        cairo_stroke(cr);
    }

    // Draw gateway node at center
    cairo_set_source_rgb(cr, 0.24, 0.50, 1.0);
    cairo_arc(cr, cx, cy, 14, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.78, 0.83, 0.94);
    cairo_set_font_size(cr, 9);
    cairo_move_to(cr, cx - 8, cy + 3);
    cairo_show_text(cr, "GW");

    // Draw device nodes
    for (int i = 0; i < n; i++) {
        double angle = 2.0 * M_PI * i / n - M_PI / 2;
        double dx = cx + radius * cos(angle);
        double dy = cy + radius * sin(angle);

        auto& dev = devices[i];
        // Color by trust/state
        double r = 0.30, g = 0.37, b = 0.52; // muted default
        if (!dev.online) { r = 0.30; g = 0.37; b = 0.52; }
        else if (dev.trustState == "owned") { r = 0.0; g = 0.90; b = 0.48; }
        else if (dev.trustState == "known") { r = 0.24; g = 0.50; b = 1.0; }
        else if (dev.trustState == "guest") { r = 1.0; g = 0.78; b = 0.20; }
        else if (dev.trustState == "blocked") { r = 1.0; g = 0.25; b = 0.38; }
        else if (dev.trustState == "watchlist") { r = 0.66; g = 0.33; b = 0.97; }
        else if (dev.iotRisk) { r = 1.0; g = 0.78; b = 0.20; }
        else if (dev.isVM) { r = 0.66; g = 0.33; b = 0.97; }

        // Glow for online devices
        if (dev.online) {
            cairo_set_source_rgba(cr, r, g, b, 0.25);
            cairo_arc(cr, dx, dy, 12, 0, 2 * M_PI);
            cairo_fill(cr);
        }

        cairo_set_source_rgb(cr, r, g, b);
        cairo_arc(cr, dx, dy, 8, 0, 2 * M_PI);
        cairo_fill(cr);

        // Label
        cairo_set_source_rgba(cr, 0.78, 0.83, 0.94, 0.8);
        cairo_set_font_size(cr, 8);
        std::string label = !dev.customName.empty() ? dev.customName :
                           !dev.hostname.empty() ? dev.hostname : dev.ip;
        if (label.size() > 16) label = label.substr(0, 14) + "..";
        cairo_move_to(cr, dx - label.size() * 2.5, dy + 18);
        cairo_show_text(cr, label.c_str());
    }

    return TRUE;
}

// ─── Overview Tab ───────────────────────────────────────────────────────────
GtkWidget* App::createOverviewTab() {
    auto* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);

    // KPI row
    auto* kpiRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    _kpiDevices = makeKPI("0", "Devices Online", "kpi-green");
    _kpiUnknown = makeKPI("0", "Unknown Devices", "kpi-amber");
    _kpiAlerts = makeKPI("0", "Active Alerts", "kpi-red");
    _kpiLatency = makeKPI("--", "Gateway Latency", "kpi-blue");
    gtk_box_pack_start(GTK_BOX(kpiRow), _kpiDevices, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(kpiRow), _kpiUnknown, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(kpiRow), _kpiAlerts, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(kpiRow), _kpiLatency, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), kpiRow, FALSE, FALSE, 0);

    // Scan controls
    auto* scanRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    auto* quickBtn = gtk_button_new_with_label("Quick Scan");
    gtk_style_context_add_class(gtk_widget_get_style_context(quickBtn), "accent-btn");
    g_signal_connect_swapped(quickBtn, "clicked", G_CALLBACK(cb_quick_scan), this);

    auto* stdBtn = gtk_button_new_with_label("Standard Scan");
    gtk_style_context_add_class(gtk_widget_get_style_context(stdBtn), "subtle-btn");
    g_signal_connect_swapped(stdBtn, "clicked", G_CALLBACK(cb_standard_scan), this);

    auto* deepBtn = gtk_button_new_with_label("Deep Scan");
    gtk_style_context_add_class(gtk_widget_get_style_context(deepBtn), "subtle-btn");
    g_signal_connect_swapped(deepBtn, "clicked", G_CALLBACK(cb_deep_scan), this);

    gtk_box_pack_start(GTK_BOX(scanRow), quickBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(scanRow), stdBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(scanRow), deepBtn, FALSE, FALSE, 0);

    // Progress
    _progressBar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(_progressBar), FALSE);
    gtk_box_pack_start(GTK_BOX(scanRow), _progressBar, TRUE, TRUE, 8);
    _statusLabel = gtk_label_new("Ready — run a scan to discover devices");
    gtk_label_set_xalign(GTK_LABEL(_statusLabel), 0);
    gtk_box_pack_start(GTK_BOX(scanRow), _statusLabel, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), scanRow, FALSE, FALSE, 0);

    // Main content: topology + changes
    auto* hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(hpaned), 500);

    // Topology map
    auto* topoFrame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(topoFrame), "card");
    auto* topoLabel = gtk_label_new("Network Topology");
    gtk_style_context_add_class(gtk_widget_get_style_context(topoLabel), "section-header");
    gtk_label_set_xalign(GTK_LABEL(topoLabel), 0);
    gtk_box_pack_start(GTK_BOX(topoFrame), topoLabel, FALSE, FALSE, 0);

    _topoDrawing = gtk_drawing_area_new();
    gtk_widget_set_size_request(_topoDrawing, 300, 300);
    g_signal_connect(_topoDrawing, "draw", G_CALLBACK(drawTopology), this);
    gtk_box_pack_start(GTK_BOX(topoFrame), _topoDrawing, TRUE, TRUE, 0);
    gtk_paned_pack1(GTK_PANED(hpaned), topoFrame, TRUE, TRUE);

    // Recent changes
    auto* changesFrame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(changesFrame), "card");
    auto* changesLabel = gtk_label_new("Recent Changes");
    gtk_style_context_add_class(gtk_widget_get_style_context(changesLabel), "section-header");
    gtk_label_set_xalign(GTK_LABEL(changesLabel), 0);
    gtk_box_pack_start(GTK_BOX(changesFrame), changesLabel, FALSE, FALSE, 0);

    _changesStore = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    _changesView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_changesStore));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(_changesView), TRUE);

    auto addCol = [](GtkWidget* view, const char* title, int col) {
        auto* r = gtk_cell_renderer_text_new();
        auto* c = gtk_tree_view_column_new_with_attributes(title, r, "text", col, nullptr);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(c), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    };
    addCol(_changesView, "Severity", 0);
    addCol(_changesView, "Type", 1);
    addCol(_changesView, "Device", 2);
    addCol(_changesView, "Description", 3);

    auto* sw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(sw), _changesView);
    gtk_box_pack_start(GTK_BOX(changesFrame), sw, TRUE, TRUE, 0);
    gtk_paned_pack2(GTK_PANED(hpaned), changesFrame, TRUE, TRUE);

    gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);

    return vbox;
}

// ─── Devices Tab ────────────────────────────────────────────────────────────
GtkWidget* App::createDevicesTab() {
    auto* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    auto* leftBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(leftBox, 12);
    gtk_widget_set_margin_top(leftBox, 8);
    gtk_widget_set_margin_bottom(leftBox, 8);

    // Search + filter row
    auto* filterRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    _searchEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(_searchEntry), "Search devices...");
    gtk_widget_set_size_request(_searchEntry, 200, -1);
    g_signal_connect_swapped(_searchEntry, "changed", G_CALLBACK(cb_search_changed), this);
    gtk_box_pack_start(GTK_BOX(filterRow), _searchEntry, FALSE, FALSE, 0);

    const char* filters[] = {"All","Online","Unknown","Owned","Watchlist","VMs"};
    const char* filterVals[] = {"all","online","unknown","owned","watchlist","vm"};
    for (int i = 0; i < 6; i++) {
        auto* btn = gtk_button_new_with_label(filters[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "subtle-btn");
        auto* val = new FilterPair{this, filterVals[i]};
        g_signal_connect_swapped(btn, "clicked", G_CALLBACK(cb_filter_click), val);
        gtk_box_pack_start(GTK_BOX(filterRow), btn, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(leftBox), filterRow, FALSE, FALSE, 0);

    // Device list
    _devicesStore = gtk_list_store_new(9, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    _devicesView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_devicesStore));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(_devicesView), TRUE);

    auto addDevCol = [](GtkWidget* v, const char* t, int c, int w) {
        auto* r = gtk_cell_renderer_text_new();
        auto* col = gtk_tree_view_column_new_with_attributes(t, r, "text", c, nullptr);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(col), TRUE);
        if (w > 0) gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(col), w);
        gtk_tree_view_append_column(GTK_TREE_VIEW(v), col);
    };
    addDevCol(_devicesView, "Status", 0, 60);
    addDevCol(_devicesView, "Name", 1, 140);
    addDevCol(_devicesView, "IP Address", 2, 120);
    addDevCol(_devicesView, "MAC", 3, 140);
    addDevCol(_devicesView, "Vendor", 4, 120);
    addDevCol(_devicesView, "Type", 5, 130);
    addDevCol(_devicesView, "Trust", 6, 80);
    addDevCol(_devicesView, "Ports", 7, 160);

    auto* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(_devicesView));
    g_signal_connect_swapped(sel, "changed", G_CALLBACK(cb_device_selected), this);

    auto* devSw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(devSw), _devicesView);
    gtk_box_pack_start(GTK_BOX(leftBox), devSw, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), leftBox, TRUE, TRUE, 0);

    // Detail panel (right side, hidden by default)
    _detailPanel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(_detailPanel), "detail-panel");
    gtk_widget_set_size_request(_detailPanel, 320, -1);
    gtk_widget_set_no_show_all(_detailPanel, TRUE);
    gtk_widget_hide(_detailPanel);

    auto* closeBtn = gtk_button_new_with_label("Close");
    gtk_style_context_add_class(gtk_widget_get_style_context(closeBtn), "subtle-btn");
    g_signal_connect_swapped(closeBtn, "clicked", G_CALLBACK(cb_close_detail), this);
    gtk_box_pack_start(GTK_BOX(_detailPanel), closeBtn, FALSE, FALSE, 0);

    auto addDetail = [&](const char* label, GtkWidget** val) {
        auto* lbl = gtk_label_new(label);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "detail-label");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_box_pack_start(GTK_BOX(_detailPanel), lbl, FALSE, FALSE, 0);
        *val = gtk_label_new("");
        gtk_style_context_add_class(gtk_widget_get_style_context(*val), "detail-value");
        gtk_label_set_xalign(GTK_LABEL(*val), 0);
        gtk_label_set_line_wrap(GTK_LABEL(*val), TRUE);
        gtk_box_pack_start(GTK_BOX(_detailPanel), *val, FALSE, FALSE, 0);
    };

    addDetail("DEVICE NAME", &_detailName);
    addDetail("TYPE / CONFIDENCE", &_detailType);
    addDetail("ALTERNATIVES", &_detailAlts);
    addDetail("IP ADDRESS", &_detailIP);
    addDetail("MAC ADDRESS", &_detailMAC);
    addDetail("VENDOR", &_detailVendor);
    addDetail("OPEN PORTS", &_detailPorts);
    addDetail("CONFIDENCE", &_detailConfidence);
    addDetail("TRUST STATE", &_detailTrust);

    _detailRisk = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(_detailRisk), "risk-box");
    gtk_label_set_line_wrap(GTK_LABEL(_detailRisk), TRUE);
    gtk_label_set_xalign(GTK_LABEL(_detailRisk), 0);
    gtk_widget_set_no_show_all(_detailRisk, TRUE);
    gtk_widget_hide(_detailRisk);
    gtk_box_pack_start(GTK_BOX(_detailPanel), _detailRisk, FALSE, FALSE, 4);

    // Notes
    auto* notesLbl = gtk_label_new("NOTES");
    gtk_style_context_add_class(gtk_widget_get_style_context(notesLbl), "detail-label");
    gtk_label_set_xalign(GTK_LABEL(notesLbl), 0);
    gtk_box_pack_start(GTK_BOX(_detailPanel), notesLbl, FALSE, FALSE, 0);
    _detailNotes = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(_detailNotes), GTK_WRAP_WORD);
    gtk_widget_set_size_request(_detailNotes, -1, 80);
    gtk_box_pack_start(GTK_BOX(_detailPanel), _detailNotes, FALSE, FALSE, 0);

    // Detail scroll
    auto* detailSw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(detailSw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(detailSw), _detailPanel);
    gtk_widget_set_size_request(detailSw, 320, -1);
    gtk_widget_set_no_show_all(detailSw, TRUE);
    // We need to manage the scroll window visibility together with the detail panel
    // Actually let's just put the detail panel directly
    gtk_box_pack_end(GTK_BOX(hbox), _detailPanel, FALSE, FALSE, 0);

    return hbox;
}

// ─── Alerts Tab ─────────────────────────────────────────────────────────────
GtkWidget* App::createAlertsTab() {
    auto* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 12);

    auto* hdr = gtk_label_new("Active Alerts");
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "section-header");
    gtk_label_set_xalign(GTK_LABEL(hdr), 0);
    gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

    // Alert list
    _alertsStore = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    _alertsView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_alertsStore));
    auto addCol = [](GtkWidget* v, const char* t, int c) {
        auto* r = gtk_cell_renderer_text_new();
        auto* col = gtk_tree_view_column_new_with_attributes(t, r, "text", c, nullptr);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(col), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(v), col);
    };
    addCol(_alertsView, "Severity", 0);
    addCol(_alertsView, "Type", 1);
    addCol(_alertsView, "Device", 2);
    addCol(_alertsView, "Description", 3);

    auto* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(_alertsView));
    g_signal_connect_swapped(sel, "changed", G_CALLBACK(cb_alert_selected), this);

    auto* sw1 = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_size_request(sw1, -1, 180);
    gtk_container_add(GTK_CONTAINER(sw1), _alertsView);
    gtk_box_pack_start(GTK_BOX(vbox), sw1, FALSE, FALSE, 0);

    // Explanation tri-section
    auto* explBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(explBox), "card");

    auto makeSection = [&](const char* title, GtkWidget** tv) {
        auto* lbl = gtk_label_new(title);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "detail-label");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_box_pack_start(GTK_BOX(explBox), lbl, FALSE, FALSE, 0);
        *tv = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(*tv), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(*tv), GTK_WRAP_WORD);
        gtk_widget_set_size_request(*tv, -1, 50);
        gtk_box_pack_start(GTK_BOX(explBox), *tv, FALSE, FALSE, 0);
    };
    makeSection("What happened:", &_alertWhat);
    makeSection("Why it matters:", &_alertWhy);
    makeSection("What to do:", &_alertDo);

    gtk_box_pack_start(GTK_BOX(vbox), explBox, FALSE, FALSE, 0);

    // Alert Rules section
    auto* rulesHdr = gtk_label_new("Alert Rules");
    gtk_style_context_add_class(gtk_widget_get_style_context(rulesHdr), "section-header");
    gtk_label_set_xalign(GTK_LABEL(rulesHdr), 0);
    gtk_box_pack_start(GTK_BOX(vbox), rulesHdr, FALSE, FALSE, 4);

    _rulesStore = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    _rulesView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_rulesStore));
    addCol(_rulesView, "Name", 0);
    addCol(_rulesView, "Event Type", 1);
    addCol(_rulesView, "Severity", 2);
    addCol(_rulesView, "Enabled", 3);

    auto* sw2 = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(sw2), _rulesView);
    gtk_box_pack_start(GTK_BOX(vbox), sw2, TRUE, TRUE, 0);

    return vbox;
}

// ─── Tools Tab ──────────────────────────────────────────────────────────────
GtkWidget* App::createToolsTab() {
    auto* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 12);

    auto* hdr = gtk_label_new("Diagnostic Tools");
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "section-header");
    gtk_label_set_xalign(GTK_LABEL(hdr), 0);
    gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

    // Target input
    auto* inputRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    auto* lbl = gtk_label_new("Target:");
    gtk_box_pack_start(GTK_BOX(inputRow), lbl, FALSE, FALSE, 0);
    _toolTarget = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(_toolTarget), "IP address or hostname");
    gtk_entry_set_text(GTK_ENTRY(_toolTarget), "8.8.8.8");
    gtk_box_pack_start(GTK_BOX(inputRow), _toolTarget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), inputRow, FALSE, FALSE, 0);

    // Tool buttons
    auto* btnRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    const char* tools[] = {"Ping","Traceroute","DNS Lookup","TCP Connect","HTTP Test","Port Scan"};
    const char* toolCmds[] = {"ping","traceroute","dns","tcp","http","portscan"};
    for (int i = 0; i < 6; i++) {
        auto* btn = gtk_button_new_with_label(tools[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "subtle-btn");
        auto* pair = new ToolPair{this, toolCmds[i]};
        g_signal_connect_swapped(btn, "clicked", G_CALLBACK(cb_tool_click), pair);
        gtk_box_pack_start(GTK_BOX(btnRow), btn, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(vbox), btnRow, FALSE, FALSE, 0);

    // Output
    _toolOutput = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(_toolOutput), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(_toolOutput), GTK_WRAP_WORD);
    auto* sw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(sw), _toolOutput);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

    return vbox;
}

// ─── Ledger Tab ─────────────────────────────────────────────────────────────
GtkWidget* App::createLedgerTab() {
    auto* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 12);

    auto* hdr = gtk_label_new("Data Ledger -- transparent record of all network events");
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "section-header");
    gtk_label_set_xalign(GTK_LABEL(hdr), 0);
    gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

    auto* btnRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    auto* clearBtn = gtk_button_new_with_label("Clear Ledger");
    gtk_style_context_add_class(gtk_widget_get_style_context(clearBtn), "danger-btn");
    g_signal_connect_swapped(clearBtn, "clicked", G_CALLBACK(cb_clear_ledger), this);
    gtk_box_pack_start(GTK_BOX(btnRow), clearBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btnRow, FALSE, FALSE, 0);

    _ledgerStore = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    _ledgerView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_ledgerStore));
    auto addCol = [](GtkWidget* v, const char* t, int c) {
        auto* r = gtk_cell_renderer_text_new();
        auto* col = gtk_tree_view_column_new_with_attributes(t, r, "text", c, nullptr);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(col), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(v), col);
    };
    addCol(_ledgerView, "Time", 0);
    addCol(_ledgerView, "Action", 1);
    addCol(_ledgerView, "Details", 2);

    auto* sw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(sw), _ledgerView);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

    return vbox;
}

// ─── Privacy Tab ────────────────────────────────────────────────────────────
GtkWidget* App::createPrivacyTab() {
    auto* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 12);

    // Privacy explanation
    auto* explCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(explCard), "card");
    auto* explText = gtk_label_new(
        "Privacy Policy: All data stays on this device. No cloud sync, no tracking, "
        "no telemetry. Network scan data is stored in memory only and cleared on exit. "
        "Export your data anytime for backup.");
    gtk_label_set_line_wrap(GTK_LABEL(explText), TRUE);
    gtk_label_set_xalign(GTK_LABEL(explText), 0);
    gtk_box_pack_start(GTK_BOX(explCard), explText, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), explCard, FALSE, FALSE, 0);

    // Stats
    auto* statsHdr = gtk_label_new("Data Statistics");
    gtk_style_context_add_class(gtk_widget_get_style_context(statsHdr), "section-header");
    gtk_label_set_xalign(GTK_LABEL(statsHdr), 0);
    gtk_box_pack_start(GTK_BOX(vbox), statsHdr, FALSE, FALSE, 0);

    _privStats = gtk_label_new("Devices: 0 | Alerts: 0 | Ledger entries: 0");
    gtk_label_set_xalign(GTK_LABEL(_privStats), 0);
    gtk_box_pack_start(GTK_BOX(vbox), _privStats, FALSE, FALSE, 0);

    auto* deleteBtn = gtk_button_new_with_label("Delete All Data");
    gtk_style_context_add_class(gtk_widget_get_style_context(deleteBtn), "danger-btn");
    g_signal_connect_swapped(deleteBtn, "clicked", G_CALLBACK(cb_delete_all), this);
    gtk_box_pack_start(GTK_BOX(vbox), deleteBtn, FALSE, FALSE, 0);

    // REST API section
    auto* apiHdr = gtk_label_new("Local REST API (port 7722)");
    gtk_style_context_add_class(gtk_widget_get_style_context(apiHdr), "section-header");
    gtk_label_set_xalign(GTK_LABEL(apiHdr), 0);
    gtk_box_pack_start(GTK_BOX(vbox), apiHdr, FALSE, FALSE, 8);

    auto* apiRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    auto* apiToggle = gtk_button_new_with_label("Enable API");
    gtk_style_context_add_class(gtk_widget_get_style_context(apiToggle), "accent-btn");
    g_signal_connect_swapped(apiToggle, "clicked", G_CALLBACK(cb_toggle_api), this);
    gtk_box_pack_start(GTK_BOX(apiRow), apiToggle, FALSE, FALSE, 0);

    _apiStatus = gtk_label_new("Stopped");
    gtk_box_pack_start(GTK_BOX(apiRow), _apiStatus, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), apiRow, FALSE, FALSE, 0);

    _apiKeyLabel = gtk_label_new("API Key: (none)");
    gtk_label_set_xalign(GTK_LABEL(_apiKeyLabel), 0);
    gtk_label_set_selectable(GTK_LABEL(_apiKeyLabel), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), _apiKeyLabel, FALSE, FALSE, 0);

    auto* endpoints = gtk_label_new(
        "Endpoints:\n"
        "  GET /api/health    - Health check\n"
        "  GET /api/status    - Scan status\n"
        "  GET /api/devices   - All discovered devices\n"
        "  GET /api/alerts    - Active anomalies\n"
        "  GET /api/snapshots - Historical scans\n\n"
        "Header: X-API-Key: <your-key>");
    gtk_label_set_xalign(GTK_LABEL(endpoints), 0);
    gtk_box_pack_start(GTK_BOX(vbox), endpoints, FALSE, FALSE, 0);

    // Spacer
    auto* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), spacer, TRUE, TRUE, 0);

    return vbox;
}

// ─── Actions ────────────────────────────────────────────────────────────────

void App::addLedgerEntry(const std::string& action, const std::string& details) {
    LedgerEntry e;
    e.timestamp = nowTimestamp();
    e.action = action;
    e.details = details;
    std::lock_guard<std::mutex> lk(_dataMutex);
    _ledger.push_back(e);
}

gboolean App::onScanProgress(gpointer data) {
    auto* pd = (ProgressData*)data;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pd->app->_progressBar), pd->pct / 100.0);
    gtk_label_set_text(GTK_LABEL(pd->app->_statusLabel), pd->msg.c_str());
    delete pd;
    return FALSE;
}

gboolean App::onScanDone(gpointer data) {
    auto* sd = (ScanDoneData*)data;
    auto* app = sd->app;
    {
        std::lock_guard<std::mutex> lk(app->_dataMutex);
        app->_lastResult = sd->result;
        app->_snapshots.push_back(sd->result);
        if (app->_snapshots.size() > 20)
            app->_snapshots.erase(app->_snapshots.begin());
    }
    app->_scanning = false;
    app->addLedgerEntry("Scan Complete",
        "Found " + std::to_string(sd->result.devices.size()) + " devices, " +
        std::to_string(sd->result.anomalies.size()) + " anomalies (" + sd->result.mode + ")");
    app->updateUI();
    delete sd;
    return FALSE;
}

void App::startScan(const std::string& mode) {
    if (_scanning) return;
    _scanning = true;
    addLedgerEntry("Scan Started", mode + " scan initiated");

    std::thread([this, mode]() {
        auto cb = [this](int pct, const std::string& msg) {
            auto* pd = new ProgressData{pct, msg, this};
            g_idle_add(onScanProgress, pd);
        };

        std::future<ScanResult> fut;
        if (mode == "quick") fut = _scanner.QuickScan(cb);
        else if (mode == "deep") fut = _scanner.DeepScan(cb);
        else fut = _scanner.StandardScan(cb);

        auto result = fut.get();
        auto* sd = new ScanDoneData{result, this};
        g_idle_add(onScanDone, sd);
    }).detach();
}

void App::updateUI() {
    refreshKPIs();
    refreshDeviceList();
    refreshAlerts();
    refreshLedger();
    gtk_widget_queue_draw(_topoDrawing);

    // Update privacy stats
    std::lock_guard<std::mutex> lk(_dataMutex);
    std::string stats = "Devices: " + std::to_string(_lastResult.devices.size()) +
                        " | Alerts: " + std::to_string(_lastResult.anomalies.size()) +
                        " | Ledger entries: " + std::to_string(_ledger.size()) +
                        " | Snapshots: " + std::to_string(_snapshots.size());
    gtk_label_set_text(GTK_LABEL(_privStats), stats.c_str());
}

void App::refreshKPIs() {
    std::lock_guard<std::mutex> lk(_dataMutex);
    int online = 0, unknown = 0;
    int gwLatency = -1;
    for (auto& d : _lastResult.devices) {
        if (d.online) online++;
        if (d.trustState == "unknown") unknown++;
    }
    if (!_lastResult.devices.empty())
        gwLatency = _lastResult.devices[0].latencyMs;

    auto setKPI = [](GtkWidget* box, const std::string& val) {
        auto* list = gtk_container_get_children(GTK_CONTAINER(box));
        if (list) {
            gtk_label_set_text(GTK_LABEL(list->data), val.c_str());
            g_list_free(list);
        }
    };
    setKPI(_kpiDevices, std::to_string(online));
    setKPI(_kpiUnknown, std::to_string(unknown));
    setKPI(_kpiAlerts, std::to_string(_lastResult.anomalies.size()));
    setKPI(_kpiLatency, gwLatency >= 0 ? std::to_string(gwLatency) + "ms" : "--");
}

void App::refreshDeviceList() {
    gtk_list_store_clear(_devicesStore);
    std::lock_guard<std::mutex> lk(_dataMutex);

    std::string search;
    const char* searchText = gtk_entry_get_text(GTK_ENTRY(_searchEntry));
    if (searchText) {
        search = searchText;
        std::transform(search.begin(), search.end(), search.begin(), ::tolower);
    }

    for (int i = 0; i < (int)_lastResult.devices.size(); i++) {
        auto& dev = _lastResult.devices[i];

        // Filter
        if (_currentFilter == "online" && !dev.online) continue;
        if (_currentFilter == "unknown" && dev.trustState != "unknown") continue;
        if (_currentFilter == "owned" && dev.trustState != "owned") continue;
        if (_currentFilter == "watchlist" && dev.trustState != "watchlist") continue;
        if (_currentFilter == "vm" && !dev.isVM && !dev.isHypervisor) continue;

        // Search
        if (!search.empty()) {
            std::string haystack = dev.ip + " " + dev.mac + " " + dev.hostname +
                " " + dev.vendor + " " + dev.deviceType + " " + dev.customName;
            std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
            if (haystack.find(search) == std::string::npos) continue;
        }

        std::string status = dev.online ? "Online" : "Offline";
        std::string name = !dev.customName.empty() ? dev.customName :
                          !dev.hostname.empty() ? dev.hostname : dev.ip;
        std::string ports;
        for (int j = 0; j < std::min((int)dev.openPorts.size(), 6); j++) {
            if (!ports.empty()) ports += ", ";
            auto it = ScanEngine::PORT_NAMES.find(dev.openPorts[j]);
            ports += it != ScanEngine::PORT_NAMES.end() ?
                     it->second : std::to_string(dev.openPorts[j]);
        }
        if (dev.openPorts.size() > 6)
            ports += " +" + std::to_string(dev.openPorts.size() - 6) + " more";

        GtkTreeIter iter;
        gtk_list_store_append(_devicesStore, &iter);
        gtk_list_store_set(_devicesStore, &iter,
            0, status.c_str(),
            1, name.c_str(),
            2, dev.ip.c_str(),
            3, dev.mac.c_str(),
            4, dev.vendor.c_str(),
            5, dev.deviceType.c_str(),
            6, dev.trustState.c_str(),
            7, ports.c_str(),
            8, i,
            -1);
    }
}

void App::refreshAlerts() {
    gtk_list_store_clear(_alertsStore);
    gtk_list_store_clear(_changesStore);
    std::lock_guard<std::mutex> lk(_dataMutex);

    for (auto& a : _lastResult.anomalies) {
        GtkTreeIter iter;
        gtk_list_store_append(_alertsStore, &iter);
        gtk_list_store_set(_alertsStore, &iter,
            0, a.severity.c_str(),
            1, a.type.c_str(),
            2, a.deviceIp.c_str(),
            3, a.description.c_str(),
            -1);

        gtk_list_store_append(_changesStore, &iter);
        gtk_list_store_set(_changesStore, &iter,
            0, a.severity.c_str(),
            1, a.type.c_str(),
            2, a.deviceIp.c_str(),
            3, a.description.c_str(),
            -1);
    }
}

void App::refreshLedger() {
    gtk_list_store_clear(_ledgerStore);
    std::lock_guard<std::mutex> lk(_dataMutex);
    // Reverse order (newest first)
    for (auto it = _ledger.rbegin(); it != _ledger.rend(); ++it) {
        GtkTreeIter iter;
        gtk_list_store_append(_ledgerStore, &iter);
        gtk_list_store_set(_ledgerStore, &iter,
            0, it->timestamp.c_str(),
            1, it->action.c_str(),
            2, it->details.c_str(),
            -1);
    }
}

void App::showDeviceDetail(int idx) {
    std::lock_guard<std::mutex> lk(_dataMutex);
    if (idx < 0 || idx >= (int)_lastResult.devices.size()) return;
    auto& dev = _lastResult.devices[idx];
    _selectedDevice = idx;

    std::string name = !dev.customName.empty() ? dev.customName :
                      !dev.hostname.empty() ? dev.hostname : dev.ip;
    gtk_label_set_text(GTK_LABEL(_detailName), name.c_str());

    std::string typeConf = dev.deviceType + " (" + std::to_string(dev.confidence) + "%)";
    if (dev.isVM) typeConf += "  [VM]";
    if (dev.isHypervisor) typeConf += "  [Hypervisor]";
    gtk_label_set_text(GTK_LABEL(_detailType), typeConf.c_str());

    std::string alts;
    if (!dev.altType1.empty())
        alts = "Alt 1: " + dev.altType1 + " (" + std::to_string(dev.altConf1) + "%)";
    if (!dev.altType2.empty())
        alts += "\nAlt 2: " + dev.altType2 + " (" + std::to_string(dev.altConf2) + "%)";
    gtk_label_set_text(GTK_LABEL(_detailAlts), alts.empty() ? "None" : alts.c_str());

    gtk_label_set_text(GTK_LABEL(_detailIP), dev.ip.c_str());
    gtk_label_set_text(GTK_LABEL(_detailMAC), dev.mac.c_str());
    gtk_label_set_text(GTK_LABEL(_detailVendor), dev.vendor.empty() ? "Unknown" : dev.vendor.c_str());

    std::string ports;
    for (int p : dev.openPorts) {
        if (!ports.empty()) ports += "\n";
        auto it = ScanEngine::PORT_NAMES.find(p);
        ports += std::to_string(p);
        if (it != ScanEngine::PORT_NAMES.end()) ports += " (" + it->second + ")";
    }
    gtk_label_set_text(GTK_LABEL(_detailPorts), ports.empty() ? "None" : ports.c_str());

    std::string confBar;
    int bars = dev.confidence / 10;
    for (int i = 0; i < 10; i++) confBar += (i < bars) ? "|" : ".";
    confBar += " " + std::to_string(dev.confidence) + "%";
    gtk_label_set_text(GTK_LABEL(_detailConfidence), confBar.c_str());

    gtk_label_set_text(GTK_LABEL(_detailTrust), dev.trustState.c_str());

    if (dev.iotRisk) {
        gtk_label_set_text(GTK_LABEL(_detailRisk), ("IoT RISK: " + dev.iotRiskDetail).c_str());
        gtk_widget_show(_detailRisk);
    } else {
        gtk_widget_hide(_detailRisk);
    }

    gtk_widget_show(_detailPanel);
}

void App::hideDeviceDetail() {
    gtk_widget_hide(_detailPanel);
    _selectedDevice = -1;
}

void App::runTool(const std::string& tool, const std::string& target) {
    addLedgerEntry("Tool: " + tool, "Target: " + target);

    auto* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(_toolOutput));
    gtk_text_buffer_set_text(buf, ("Running " + tool + " on " + target + "...\n").c_str(), -1);

    std::string targetCopy = target;
    std::string toolCopy = tool;
    auto* outputWidget = _toolOutput;

    std::thread([targetCopy, toolCopy, outputWidget]() {
        std::string cmd;
        // Sanitize target - only allow alphanumeric, dots, colons, hyphens
        std::string safeTarget;
        for (char c : targetCopy) {
            if (isalnum(c) || c == '.' || c == ':' || c == '-') safeTarget += c;
        }

        if (toolCopy == "ping")
            cmd = "ping -c 4 " + safeTarget + " 2>&1";
        else if (toolCopy == "traceroute")
            cmd = "traceroute -m 20 " + safeTarget + " 2>&1";
        else if (toolCopy == "dns")
            cmd = "dig " + safeTarget + " A +short 2>&1 && echo '---' && dig " + safeTarget + " AAAA +short 2>&1";
        else if (toolCopy == "tcp")
            cmd = "bash -c 'echo > /dev/tcp/" + safeTarget + "/80 && echo \"Port 80: OPEN\" || echo \"Port 80: CLOSED\"' 2>&1";
        else if (toolCopy == "http")
            cmd = "curl -sI -m 5 http://" + safeTarget + "/ 2>&1";
        else if (toolCopy == "portscan") {
            // Quick port scan of common ports
            cmd = "for p in 22 80 443 3389 5900 8080; do "
                  "(echo > /dev/tcp/" + safeTarget + "/$p) 2>/dev/null && echo \"Port $p: OPEN\" || echo \"Port $p: closed\"; "
                  "done 2>&1";
        }

        std::string result = execCmd(cmd);
        auto* data = new std::pair<GtkWidget*, std::string>(outputWidget, result);
        g_idle_add(cb_tool_output_idle, data);
    }).detach();
}

// ─── Local REST API ─────────────────────────────────────────────────────────
void App::startLocalApi() {
    if (_apiEnabled) return;
    _apiEnabled = true;

    // Generate API key
    srand(time(nullptr));
    char keybuf[32];
    snprintf(keybuf, sizeof(keybuf), "tr-%04x%04x%04x", rand()&0xFFFF, rand()&0xFFFF, rand()&0xFFFF);
    _apiKey = keybuf;

    gtk_label_set_text(GTK_LABEL(_apiStatus), "Running on :7722");
    gtk_label_set_text(GTK_LABEL(_apiKeyLabel), ("API Key: " + _apiKey).c_str());

    addLedgerEntry("API Started", "Local REST API on port 7722");

    std::thread([this]() {
        int srv = socket(AF_INET, SOCK_STREAM, 0);
        if (srv < 0) { _apiEnabled = false; return; }

        int opt = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(_apiPort);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0 ||
            listen(srv, 4) != 0) {
            close(srv);
            _apiEnabled = false;
            return;
        }

        while (_apiEnabled) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(srv, &rfds);
            struct timeval tv = {1, 0};
            if (select(srv + 1, &rfds, nullptr, nullptr, &tv) <= 0) continue;

            int client = accept(srv, nullptr, nullptr);
            if (client < 0) continue;

            char buf[4096] = {};
            int n = recv(client, buf, sizeof(buf) - 1, 0);
            if (n <= 0) { close(client); continue; }
            std::string req(buf, n);

            // Check API key
            bool authorized = _apiKey.empty() || req.find("X-API-Key: " + _apiKey) != std::string::npos;
            if (!authorized) {
                std::string resp = "HTTP/1.0 401 Unauthorized\r\nContent-Type: application/json\r\n\r\n{\"error\":\"Unauthorized\"}\r\n";
                send(client, resp.c_str(), resp.size(), 0);
                close(client);
                continue;
            }

            std::string body;
            bool found = false;

            auto route = [&](const char* path) { return req.find(std::string("GET ") + path) == 0; };

            if (route("/api/health")) {
                body = "{\"status\":\"ok\",\"version\":\"3.4.0\"}\r\n";
                found = true;
            } else if (route("/api/status")) {
                std::lock_guard<std::mutex> lk(_dataMutex);
                body = "{\"devices\":" + std::to_string(_lastResult.devices.size()) +
                       ",\"anomalies\":" + std::to_string(_lastResult.anomalies.size()) + "}\r\n";
                found = true;
            } else if (route("/api/devices")) {
                std::lock_guard<std::mutex> lk(_dataMutex);
                body = "[";
                for (size_t i = 0; i < _lastResult.devices.size(); i++) {
                    auto& d = _lastResult.devices[i];
                    if (i) body += ",";
                    body += "{\"ip\":\"" + d.ip + "\",\"mac\":\"" + d.mac +
                            "\",\"hostname\":\"" + d.hostname + "\",\"vendor\":\"" + d.vendor +
                            "\",\"type\":\"" + d.deviceType + "\",\"trust\":\"" + d.trustState +
                            "\",\"online\":" + (d.online ? "true" : "false") +
                            ",\"confidence\":" + std::to_string(d.confidence) +
                            ",\"isVM\":" + (d.isVM ? "true" : "false") + "}";
                }
                body += "]\r\n";
                found = true;
            } else if (route("/api/alerts")) {
                std::lock_guard<std::mutex> lk(_dataMutex);
                body = "[";
                for (size_t i = 0; i < _lastResult.anomalies.size(); i++) {
                    auto& a = _lastResult.anomalies[i];
                    if (i) body += ",";
                    body += "{\"type\":\"" + a.type + "\",\"severity\":\"" + a.severity +
                            "\",\"device\":\"" + a.deviceIp + "\",\"description\":\"" + a.description + "\"}";
                }
                body += "]\r\n";
                found = true;
            }

            std::string resp;
            if (found) {
                resp = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: "
                     + std::to_string(body.size()) + "\r\n\r\n" + body;
            } else {
                resp = "HTTP/1.0 404 Not Found\r\nContent-Type: application/json\r\n\r\n{\"error\":\"Not found\"}\r\n";
            }
            send(client, resp.c_str(), resp.size(), 0);
            close(client);
        }
        close(srv);
    }).detach();
}

void App::stopLocalApi() {
    _apiEnabled = false;
    gtk_label_set_text(GTK_LABEL(_apiStatus), "Stopped");
    addLedgerEntry("API Stopped", "Local REST API disabled");
}

// ─── Main Run ───────────────────────────────────────────────────────────────
int App::run(int argc, char** argv) {
    gtk_init(&argc, &argv);
    setupCSS();

    _window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(_window), "Transparency - Network Monitor v3.4.0");
    gtk_window_set_default_size(GTK_WINDOW(_window), 1400, 860);
    g_signal_connect(_window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    auto* mainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(_window), mainBox);

    // Sidebar
    _sidebar = createSidebar();
    gtk_box_pack_start(GTK_BOX(mainBox), _sidebar, FALSE, FALSE, 0);

    // Stack for tab content
    _stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(_stack), 150);

    gtk_stack_add_named(GTK_STACK(_stack), createOverviewTab(), "overview");
    gtk_stack_add_named(GTK_STACK(_stack), createDevicesTab(), "devices");
    gtk_stack_add_named(GTK_STACK(_stack), createAlertsTab(), "alerts");
    gtk_stack_add_named(GTK_STACK(_stack), createToolsTab(), "tools");
    gtk_stack_add_named(GTK_STACK(_stack), createLedgerTab(), "ledger");
    gtk_stack_add_named(GTK_STACK(_stack), createPrivacyTab(), "privacy");

    gtk_box_pack_start(GTK_BOX(mainBox), _stack, TRUE, TRUE, 0);

    // Set initial tab
    switchTab("overview");

    addLedgerEntry("App Started", "Transparency v3.4.0 initialized");

    gtk_widget_show_all(_window);

    // Auto-start quick scan
    startScan("quick");

    gtk_main();
    return 0;
}
