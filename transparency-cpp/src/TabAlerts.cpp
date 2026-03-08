#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>

#include "TabAlerts.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

const wchar_t* TabAlerts::s_className = L"TransparencyTabAlerts";

static const wchar_t* ALERT_FILTER_LABELS[] = { L"All", L"High", L"Medium", L"Low", L"Unack'd" };

bool TabAlerts::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
    _mainWnd = mainWnd;

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hbrBackground = Theme::BrushApp();
    wc.lpszClassName = s_className;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wc);

    _hwnd = CreateWindowEx(0, s_className, nullptr,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);

    return _hwnd != nullptr;
}

LRESULT CALLBACK TabAlerts::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabAlerts* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabAlerts*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabAlerts*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:     return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:       self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:      return self->OnPaint(hwnd);
    case WM_ERASEBKGND: { RECT rc; GetClientRect(hwnd,&rc); FillRect((HDC)wp,&rc,Theme::BrushApp()); return 1; }
    case WM_COMMAND:    return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY:     return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        SetTextColor((HDC)wp, Theme::TEXT_PRIMARY);
        SetBkColor((HDC)wp, Theme::BG_APP);
        return (LRESULT)Theme::BrushApp();
    }
    case WM_SCAN_COMPLETE: return self->OnScanComplete(hwnd);
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabAlerts::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    return 0;
}

void TabAlerts::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Filter buttons
    int btnX = 16;
    for (int i = 0; i < 5; i++) {
        _hFilterBtns[i] = CreateWindowEx(0, L"BUTTON", ALERT_FILTER_LABELS[i],
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            btnX, 12, 80, 26, hwnd, (HMENU)(9600 + i), hInst, nullptr);
        SendMessage(_hFilterBtns[i], WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
        btnX += 84;
    }

    _hBtnClearAll = CreateWindowEx(0, L"BUTTON", L"Clear All",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cx - 100, 12, 84, 26, hwnd, (HMENU)IDC_BTN_CLEAR_ALERTS, hInst, nullptr);
    SendMessage(_hBtnClearAll, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    // Alert list
    int alertH = (cy - 80) / 3;
    _hAlertList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_VSCROLL,
        16, 48, cx - 32, alertH,
        hwnd, (HMENU)IDC_LIST_ALERTS, hInst, nullptr);

    SendMessage(_hAlertList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hAlertList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hAlertList);

    struct ColDef { const wchar_t* name; int w; };
    static const ColDef ALERT_COLS[] = {
        { L"Severity", 80 }, { L"Title", 240 }, { L"Device", 120 },
        { L"Time", 130 }, { L"Status", 80 }
    };
    LVCOLUMN col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
    for (int i = 0; i < 5; i++) {
        col.cx = ALERT_COLS[i].w; col.pszText = (LPWSTR)ALERT_COLS[i].name;
        ListView_InsertColumn(_hAlertList, i, &col);
    }

    // Explanation panel (three-part: What / Why / What to do)
    int explainY = 48 + alertH + 6;
    int explainH = alertH - 6;
    _hExplainPanel = CreateWindowEx(WS_EX_STATICEDGE, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE,
        16, explainY, cx - 32, explainH, hwnd, nullptr, hInst, nullptr);

    auto mkExplainLbl = [&](const wchar_t* hdr, int y, int h) -> HWND {
        // Section label inside panel
        HWND hw = CreateWindowEx(0, L"STATIC", hdr, WS_CHILD | WS_VISIBLE | SS_LEFT,
            4, y, 80, 16, _hExplainPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
        return hw;
    };
    auto mkExplainEdit = [&](int id, int y, int h) -> HWND {
        HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            4, y, cx - 40, h, _hExplainPanel, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        Theme::ApplyDarkEdit(hw);
        return hw;
    };
    int ey = 2;
    int thirdH = (explainH - 6) / 3 - 20;
    mkExplainLbl(L"What happened:", 0, 16);
    _hExplainWhat = mkExplainEdit(9650, 16, thirdH);
    ey = 16 + thirdH + 4;
    mkExplainLbl(L"Why it matters:", ey, 16); ey += 16;
    _hExplainWhy = mkExplainEdit(9651, ey, thirdH); ey += thirdH + 4;
    mkExplainLbl(L"What to do:", ey, 16); ey += 16;
    _hExplainDo  = mkExplainEdit(9652, ey, thirdH);

    // Default text when no alert selected
    if (_hExplainWhat) SetWindowText(_hExplainWhat, L"Select an alert above to see details.");

    // Rules section
    int rulesY = 48 + alertH * 2 + 18;

    HWND hRulesHdr = CreateWindowEx(0, L"STATIC", L"ALERT RULES",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, rulesY - 20, 200, 18, hwnd, nullptr, hInst, nullptr);
    SendMessage(hRulesHdr, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    _hBtnAddRule = CreateWindowEx(0, L"BUTTON", L"Add Rule",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cx - 292, rulesY - 22, 88, 22, hwnd, (HMENU)IDC_BTN_ADD_RULE, hInst, nullptr);
    SendMessage(_hBtnAddRule, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    _hBtnEditRule = CreateWindowEx(0, L"BUTTON", L"Edit",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cx - 200, rulesY - 22, 60, 22, hwnd, (HMENU)IDC_BTN_EDIT_RULE, hInst, nullptr);
    SendMessage(_hBtnEditRule, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    _hBtnDelRule = CreateWindowEx(0, L"BUTTON", L"Delete",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cx - 136, rulesY - 22, 60, 22, hwnd, (HMENU)IDC_BTN_DEL_RULE, hInst, nullptr);
    SendMessage(_hBtnDelRule, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    int ruleH = cy - rulesY - 16;
    _hRuleList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_VSCROLL,
        16, rulesY, cx - 32, ruleH,
        hwnd, (HMENU)IDC_LIST_RULES, hInst, nullptr);

    SendMessage(_hRuleList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hRuleList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES);
    Theme::ApplyDarkScrollbar(_hRuleList);

    static const ColDef RULE_COLS[] = {
        { L"Name", 160 }, { L"Event", 160 }, { L"Filter", 100 },
        { L"Severity", 80 }, { L"Enabled", 60 }
    };
    for (int i = 0; i < 5; i++) {
        col.cx = RULE_COLS[i].w; col.pszText = (LPWSTR)RULE_COLS[i].name;
        ListView_InsertColumn(_hRuleList, i, &col);
    }
}

void TabAlerts::LayoutControls(int cx, int cy) {
    int alertH = (cy - 80) / 3;
    if (_hAlertList) SetWindowPos(_hAlertList, nullptr, 16, 48, cx - 32, alertH, SWP_NOZORDER);
    if (_hBtnClearAll) SetWindowPos(_hBtnClearAll, nullptr, cx - 100, 12, 84, 26, SWP_NOZORDER);

    int explainY = 48 + alertH + 6;
    int explainH = alertH - 6;
    if (_hExplainPanel) SetWindowPos(_hExplainPanel, nullptr, 16, explainY, cx - 32, explainH, SWP_NOZORDER);

    int rulesY = 48 + alertH * 2 + 18;
    int ruleH = cy - rulesY - 16;
    if (_hRuleList) SetWindowPos(_hRuleList, nullptr, 16, rulesY, cx - 32, ruleH, SWP_NOZORDER);
    if (_hBtnAddRule) SetWindowPos(_hBtnAddRule, nullptr, cx - 292, rulesY - 22, 88, 22, SWP_NOZORDER);
    if (_hBtnEditRule) SetWindowPos(_hBtnEditRule, nullptr, cx - 200, rulesY - 22, 60, 22, SWP_NOZORDER);
    if (_hBtnDelRule) SetWindowPos(_hBtnDelRule, nullptr, cx - 136, rulesY - 22, 60, 22, SWP_NOZORDER);
}

LRESULT TabAlerts::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabAlerts::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushApp());
    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT TabAlerts::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    if (id >= 9600 && id <= 9604) {
        _alertFilter = id - 9600;
        PopulateAlerts();
        return 0;
    }

    switch (id) {
    case IDC_BTN_CLEAR_ALERTS:
        if (_mainWnd) {
            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            _mainWnd->_lastResult.anomalies.clear();
        }
        PopulateAlerts();
        break;

    case IDC_BTN_ADD_RULE:
        ShowRuleDialog();
        break;

    case IDC_BTN_EDIT_RULE: {
        int sel = ListView_GetNextItem(_hRuleList, -1, LVNI_SELECTED);
        if (sel >= 0 && _mainWnd) {
            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            if (sel < (int)_mainWnd->_alertRules.size())
                ShowRuleDialog(&_mainWnd->_alertRules[sel]);
        }
        break;
    }

    case IDC_BTN_DEL_RULE: {
        int sel = ListView_GetNextItem(_hRuleList, -1, LVNI_SELECTED);
        if (sel >= 0 && _mainWnd) {
            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            if (sel < (int)_mainWnd->_alertRules.size())
                _mainWnd->_alertRules.erase(_mainWnd->_alertRules.begin() + sel);
        }
        PopulateRules();
        break;
    }
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

void TabAlerts::ShowAlertExplanation(int anomalyIdx) {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();

    // Build visible anomaly list (respecting current filter)
    std::vector<const Anomaly*> visible;
    for (auto& a : r.anomalies) {
        if (_alertFilter == 1 && a.severity != L"high" && a.severity != L"critical") continue;
        if (_alertFilter == 2 && a.severity != L"medium") continue;
        if (_alertFilter == 3 && a.severity != L"low") continue;
        visible.push_back(&a);
    }

    if (anomalyIdx < 0 || anomalyIdx >= (int)visible.size()) {
        if (_hExplainWhat) SetWindowText(_hExplainWhat, L"Select an alert above to see details.");
        if (_hExplainWhy)  SetWindowText(_hExplainWhy, L"");
        if (_hExplainDo)   SetWindowText(_hExplainDo, L"");
        return;
    }

    const Anomaly& a = *visible[anomalyIdx];

    wstring what = a.description;
    if (!a.deviceIp.empty()) what += L"\r\nDevice: " + a.deviceIp;

    wstring why = a.explanation.empty() ?
        L"This event may indicate a security or configuration issue on your network." :
        a.explanation;

    wstring whatToDo = a.remediation.empty() ?
        L"1. Investigate the device.\r\n2. Check your router's logs.\r\n3. Run a Deep scan for more detail." :
        a.remediation;

    if (_hExplainWhat) SetWindowText(_hExplainWhat, what.c_str());
    if (_hExplainWhy)  SetWindowText(_hExplainWhy, why.c_str());
    if (_hExplainDo)   SetWindowText(_hExplainDo, whatToDo.c_str());
}

LRESULT TabAlerts::OnNotify(HWND hwnd, NMHDR* hdr) {
    if (!hdr) return 0;

    if (hdr->idFrom == IDC_LIST_ALERTS) {
        if (hdr->code == NM_CLICK || hdr->code == NM_DBLCLK) {
            NMITEMACTIVATE* nm = (NMITEMACTIVATE*)hdr;
            if (nm->iItem >= 0) ShowAlertExplanation(nm->iItem);
        }
    }

    if (hdr->idFrom == IDC_LIST_ALERTS && hdr->code == NM_CUSTOMDRAW) {
        NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)hdr;
        switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
            int row = (int)cd->nmcd.dwItemSpec;
            // Color by severity
            COLORREF bg = Theme::BG_APP;
            if (_mainWnd) {
                ScanResult r = _mainWnd->GetLastResult();
                int visible = 0;
                for (auto& a : r.anomalies) {
                    if (visible == row) {
                        if (a.severity == L"critical" || a.severity == L"high")
                            bg = RGB(50, 20, 20);
                        else if (a.severity == L"medium")
                            bg = RGB(50, 35, 10);
                        else
                            bg = RGB(10, 40, 20);
                        break;
                    }
                    visible++;
                }
            }
            cd->clrTextBk = bg;
            cd->clrText   = Theme::TEXT_PRIMARY;
            return CDRF_NEWFONT;
        }
        }
    }

    return CDRF_DODEFAULT;
}

LRESULT TabAlerts::OnScanComplete(HWND hwnd) {
    PopulateAlerts();
    return 0;
}

void TabAlerts::PopulateAlerts() {
    if (!_hAlertList || !_mainWnd) return;

    ScanResult r = _mainWnd->GetLastResult();
    ListView_DeleteAllItems(_hAlertList);

    int row = 0;
    for (auto& a : r.anomalies) {
        if (_alertFilter == 1 && a.severity != L"high" && a.severity != L"critical") continue;
        if (_alertFilter == 2 && a.severity != L"medium") continue;
        if (_alertFilter == 3 && a.severity != L"low") continue;

        LVITEM item = {};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)a.severity.c_str();
        ListView_InsertItem(_hAlertList, &item);

        ListView_SetItemText(_hAlertList, row, 1, (LPWSTR)a.description.c_str());
        ListView_SetItemText(_hAlertList, row, 2, (LPWSTR)a.deviceIp.c_str());
        ListView_SetItemText(_hAlertList, row, 3, (LPWSTR)r.scannedAt.c_str());
        ListView_SetItemText(_hAlertList, row, 4, (LPWSTR)L"New");
        row++;
    }
}

void TabAlerts::PopulateRules() {
    if (!_hRuleList || !_mainWnd) return;

    ListView_DeleteAllItems(_hRuleList);

    std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
    int row = 0;
    for (auto& rule : _mainWnd->_alertRules) {
        LVITEM item = {};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)rule.name.c_str();
        ListView_InsertItem(_hRuleList, &item);

        ListView_SetItemText(_hRuleList, row, 1, (LPWSTR)rule.eventType.c_str());
        ListView_SetItemText(_hRuleList, row, 2, (LPWSTR)rule.deviceFilter.c_str());
        ListView_SetItemText(_hRuleList, row, 3, (LPWSTR)rule.severity.c_str());
        ListView_SetItemText(_hRuleList, row, 4, (LPWSTR)(rule.enabled ? L"Yes" : L"No"));
        row++;
    }
}

void TabAlerts::RefreshAlerts() { PopulateAlerts(); }
void TabAlerts::RefreshRules()  { PopulateRules(); }

// ─── Rule Dialog ─────────────────────────────────────────────────────────────

struct RuleDlgData {
    AlertRule* rule;      // nullptr = new
    AlertRule  result;
    bool       ok = false;
};

INT_PTR CALLBACK TabAlerts::RuleDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    RuleDlgData* data = reinterpret_cast<RuleDlgData*>(GetWindowLongPtr(dlg, DWLP_USER));

    switch (msg) {
    case WM_INITDIALOG: {
        data = reinterpret_cast<RuleDlgData*>(lp);
        SetWindowLongPtr(dlg, DWLP_USER, (LONG_PTR)data);

        // Populate controls
        HWND hName = GetDlgItem(dlg, 1001);
        HWND hEvent = GetDlgItem(dlg, 1002);
        HWND hFilter = GetDlgItem(dlg, 1003);
        HWND hSev = GetDlgItem(dlg, 1004);
        HWND hDebounce = GetDlgItem(dlg, 1005);
        HWND hWebhook = GetDlgItem(dlg, 1006);
        HWND hEnabled = GetDlgItem(dlg, 1007);

        // Event types
        const wchar_t* events[] = {
            L"new_device", L"risky_port", L"port_changed", L"device_offline",
            L"ip_changed", L"internet_outage", L"gateway_mac_changed", L"dns_changed", L"high_latency"
        };
        for (auto* e : events) SendMessage(hEvent, CB_ADDSTRING, 0, (LPARAM)e);

        // Device filters
        const wchar_t* filters[] = { L"all", L"unknown", L"watchlist", L"owned", L"guest" };
        for (auto* f : filters) SendMessage(hFilter, CB_ADDSTRING, 0, (LPARAM)f);

        // Severities
        const wchar_t* sevs[] = { L"low", L"medium", L"high", L"critical" };
        for (auto* s : sevs) SendMessage(hSev, CB_ADDSTRING, 0, (LPARAM)s);

        if (data->rule) {
            SetWindowText(hName, data->rule->name.c_str());
            SetDlgItemInt(dlg, 1005, data->rule->debounceMinutes, FALSE);
            SetWindowText(hWebhook, data->rule->webhookUrl.c_str());
            SendMessage(hEnabled, BM_SETCHECK, data->rule->enabled ? BST_CHECKED : BST_UNCHECKED, 0);
        } else {
            SetDlgItemInt(dlg, 1005, 5, FALSE);
            SendMessage(hEnabled, BM_SETCHECK, BST_CHECKED, 0);
            SendMessage(hSev, CB_SETCURSEL, 1, 0);
            SendMessage(hEvent, CB_SETCURSEL, 0, 0);
            SendMessage(hFilter, CB_SETCURSEL, 0, 0);
        }

        SetWindowText(dlg, data->rule ? L"Edit Alert Rule" : L"Add Alert Rule");
        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDOK && data) {
            wchar_t buf[512] = {};
            GetDlgItemText(dlg, 1001, buf, 512);
            data->result.name = buf;
            data->result.debounceMinutes = GetDlgItemInt(dlg, 1005, nullptr, FALSE);
            GetDlgItemText(dlg, 1006, buf, 512);
            data->result.webhookUrl = buf;
            data->result.enabled = SendMessage(GetDlgItem(dlg, 1007), BM_GETCHECK, 0, 0) == BST_CHECKED;

            // Event type
            int sel = (int)SendMessage(GetDlgItem(dlg, 1002), CB_GETCURSEL, 0, 0);
            wchar_t eventBuf[128] = {};
            SendMessage(GetDlgItem(dlg, 1002), CB_GETLBTEXT, sel, (LPARAM)eventBuf);
            data->result.eventType = eventBuf;

            sel = (int)SendMessage(GetDlgItem(dlg, 1003), CB_GETCURSEL, 0, 0);
            wchar_t filterBuf[128] = {};
            SendMessage(GetDlgItem(dlg, 1003), CB_GETLBTEXT, sel, (LPARAM)filterBuf);
            data->result.deviceFilter = filterBuf;

            sel = (int)SendMessage(GetDlgItem(dlg, 1004), CB_GETCURSEL, 0, 0);
            wchar_t sevBuf[64] = {};
            SendMessage(GetDlgItem(dlg, 1004), CB_GETLBTEXT, sel, (LPARAM)sevBuf);
            data->result.severity = sevBuf;

            data->ok = true;
            EndDialog(dlg, IDOK);
        } else if (id == IDCANCEL) {
            EndDialog(dlg, IDCANCEL);
        }
        return TRUE;
    }
    }

    return FALSE;
}

void TabAlerts::ShowRuleDialog(const AlertRule* existing) {
    // Build a simple dialog template in memory
    // We create a modal dialog programmatically to avoid needing .rc files

    struct alignas(DWORD) DlgTmpl {
        DLGTEMPLATE dt;
        WORD menu = 0, cls = 0;
        wchar_t title[32] = L"Alert Rule";
    };

    // Since creating a full in-memory dialog template for complex controls is tedious,
    // we use a simpler approach: create a popup window as the "dialog"
    HWND dlgWnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"#32770", L"Alert Rule Builder",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT,
        100, 100, 460, 380,
        _hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!dlgWnd) return;

    HINSTANCE hInst = GetModuleHandle(nullptr);
    auto mk = [&](const wchar_t* cls, const wchar_t* text, int id, int x, int y, int w, int h, DWORD style) {
        HWND hw = CreateWindowEx(0, cls, text, WS_CHILD | WS_VISIBLE | style,
            x, y, w, h, dlgWnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };

    mk(L"STATIC",  L"Rule Name:", 0, 10, 10, 90, 20, SS_LEFT);
    HWND hName = mk(L"EDIT", existing ? existing->name.c_str() : L"", 1001, 105, 8, 340, 22, WS_BORDER | ES_AUTOHSCROLL);

    mk(L"STATIC",  L"Event Type:", 0, 10, 38, 90, 20, SS_LEFT);
    HWND hEvent = mk(L"COMBOBOX", nullptr, 1002, 105, 36, 340, 120, CBS_DROPDOWNLIST | WS_VSCROLL);

    mk(L"STATIC",  L"Device Filter:", 0, 10, 66, 90, 20, SS_LEFT);
    HWND hFilter = mk(L"COMBOBOX", nullptr, 1003, 105, 64, 200, 100, CBS_DROPDOWNLIST | WS_VSCROLL);

    mk(L"STATIC",  L"Severity:", 0, 10, 94, 90, 20, SS_LEFT);
    HWND hSev = mk(L"COMBOBOX", nullptr, 1004, 105, 92, 150, 100, CBS_DROPDOWNLIST | WS_VSCROLL);

    mk(L"STATIC",  L"Debounce (min):", 0, 10, 122, 110, 20, SS_LEFT);
    mk(L"EDIT",    L"5", 1005, 125, 120, 60, 22, WS_BORDER | ES_NUMBER);

    mk(L"STATIC",  L"Webhook URL:", 0, 10, 150, 90, 20, SS_LEFT);
    mk(L"EDIT",    existing ? existing->webhookUrl.c_str() : L"", 1006, 105, 148, 340, 22, WS_BORDER | ES_AUTOHSCROLL);

    mk(L"STATIC",  L"Works with Home Assistant, Slack, Discord webhooks", 0, 105, 174, 340, 18, SS_LEFT);

    HWND hEnabled = mk(L"BUTTON", L"Enabled", 1007, 10, 200, 100, 22, BS_AUTOCHECKBOX);
    SendMessage(hEnabled, BM_SETCHECK, BST_CHECKED, 0);

    mk(L"BUTTON",  L"OK",     IDOK,     100, 300, 80, 28, BS_DEFPUSHBUTTON);
    mk(L"BUTTON",  L"Cancel", IDCANCEL, 190, 300, 80, 28, BS_PUSHBUTTON);

    // Populate combos
    const wchar_t* events[] = {
        L"new_device", L"risky_port", L"port_changed", L"device_offline",
        L"ip_changed", L"internet_outage", L"gateway_mac_changed", L"dns_changed", L"high_latency"
    };
    for (auto* e : events) SendMessage(hEvent, CB_ADDSTRING, 0, (LPARAM)e);
    SendMessage(hEvent, CB_SETCURSEL, 0, 0);

    const wchar_t* filters[] = { L"all", L"unknown", L"watchlist", L"owned", L"guest" };
    for (auto* f : filters) SendMessage(hFilter, CB_ADDSTRING, 0, (LPARAM)f);
    SendMessage(hFilter, CB_SETCURSEL, 0, 0);

    const wchar_t* sevs[] = { L"low", L"medium", L"high", L"critical" };
    for (auto* s : sevs) SendMessage(hSev, CB_ADDSTRING, 0, (LPARAM)s);
    SendMessage(hSev, CB_SETCURSEL, 1, 0);

    // Dark theme
    SetWindowLongPtr(dlgWnd, GWL_STYLE, GetWindowLong(dlgWnd, GWL_STYLE));

    Theme::SetDarkTitlebar(dlgWnd);
    ShowWindow(dlgWnd, SW_SHOW);
    UpdateWindow(dlgWnd);

    // Mini message loop
    MSG msg;
    while (IsWindow(dlgWnd) && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND &&
            (GetParent(msg.hwnd) == dlgWnd || msg.hwnd == dlgWnd)) {
            int id = LOWORD(msg.wParam);
            if (id == IDOK || id == IDCANCEL) {
                if (id == IDOK && _mainWnd) {
                    AlertRule rule;
                    wchar_t buf[512];

                    GetWindowText(hName, buf, 512); rule.name = buf;
                    int sel = (int)SendMessage(hEvent, CB_GETCURSEL, 0, 0);
                    SendMessage(hEvent, CB_GETLBTEXT, sel, (LPARAM)buf);
                    rule.eventType = buf;

                    sel = (int)SendMessage(hFilter, CB_GETCURSEL, 0, 0);
                    SendMessage(hFilter, CB_GETLBTEXT, sel, (LPARAM)buf);
                    rule.deviceFilter = buf;

                    sel = (int)SendMessage(hSev, CB_GETCURSEL, 0, 0);
                    SendMessage(hSev, CB_GETLBTEXT, sel, (LPARAM)buf);
                    rule.severity = buf;

                    rule.enabled = SendMessage(hEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    rule.debounceMinutes = 5;
                    rule.id = std::to_wstring(GetTickCount());

                    std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
                    _mainWnd->_alertRules.push_back(rule);
                    PopulateRules();
                }
                DestroyWindow(dlgWnd);
                break;
            }
        }
        if (!IsDialogMessage(dlgWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}
