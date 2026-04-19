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

// Fixed content layout heights — tab scrolls vertically to fit all sections
static const int ALERT_LIST_Y  = 52;    // top of alert list (below filter bar)
static const int ALERT_LIST_H  = 280;   // alert list (~12 rows)
static const int RISK_PREV_Y   = 360;   // prevalence label (below list + section header)
static const int RISK_ACT_Y    = 384;   // action recommendation label
static const int EXPLAIN_Y     = 430;   // explain panel (pushed down to fit risk context)
static const int EXPLAIN_H     = 280;   // explain panel
static const int RULE_LIST_Y   = 746;   // EXPLAIN_Y + EXPLAIN_H + 36
static const int RULE_LIST_H   = 260;   // rule list
static const int TAB_CONTENT_H = 1026;  // RULE_LIST_Y + RULE_LIST_H + 20

bool TabAlerts::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
    _mainWnd = mainWnd;

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hbrBackground = Theme::BrushSurface();
    wc.lpszClassName = s_className;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wc);

    _hwnd = CreateWindowEx(0, s_className, nullptr,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
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
    case WM_ERASEBKGND: return 1;  // Suppress — OnPaint handles all drawing
    case WM_COMMAND:    return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY:     return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (!dis) return 0;
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;

        // Filter pill buttons
        if (dis->CtlID >= 9600 && dis->CtlID <= 9604) {
            int filterIdx = dis->CtlID - 9600;
            bool active = (filterIdx == self->_alertFilter);
            Theme::DrawGlassPill(hdc, rc, 15, active, pressed);

            wchar_t text[32] = {};
            GetWindowText(dis->hwndItem, text, 32);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, active ? Theme::ACCENT_BLUE : Theme::TEXT_SECONDARY);
            HFONT old = (HFONT)SelectObject(hdc, Theme::FontCaption());
            DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, old);
            return TRUE;
        }
        // Clear All — destructive glass button
        if (dis->CtlID == IDC_BTN_CLEAR_ALERTS) {
            bool focused = (dis->itemState & ODS_FOCUS) != 0;
            Theme::DrawGlassButton(hdc, rc, Theme::RADIUS_MD, pressed, 2, focused);

            wchar_t text[32] = {};
            GetWindowText(dis->hwndItem, text, 32);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, Theme::ACCENT_RED);
            HFONT old = (HFONT)SelectObject(hdc, Theme::FontNavActive());
            DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, old);
            return TRUE;
        }
        // Rule buttons (Add/Edit/Delete)
        if (dis->CtlID == IDC_BTN_ADD_RULE || dis->CtlID == IDC_BTN_EDIT_RULE ||
            dis->CtlID == IDC_BTN_DEL_RULE) {
            bool isDel = (dis->CtlID == IDC_BTN_DEL_RULE);
            bool isAdd = (dis->CtlID == IDC_BTN_ADD_RULE);
            int variant = isAdd ? 0 : isDel ? 2 : 1;
            bool focused = (dis->itemState & ODS_FOCUS) != 0;
            Theme::DrawGlassButton(hdc, rc, Theme::RADIUS_SM, pressed, variant, focused);

            wchar_t text[32] = {};
            GetWindowText(dis->hwndItem, text, 32);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isAdd ? RGB(255,255,255) : isDel ? Theme::ACCENT_RED : Theme::TEXT_PRIMARY);
            HFONT old = (HFONT)SelectObject(hdc, Theme::FontCaption());
            DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, old);
            return TRUE;
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        HWND ctrl = (HWND)lp;
        SetBkColor(hdc, Theme::BG_SURFACE);
        if (self && ctrl == self->_hRiskAction)
            SetTextColor(hdc, self->_actionColor);
        else
            SetTextColor(hdc, Theme::TEXT_PRIMARY);
        return (LRESULT)Theme::BrushSurface();
    }
    case WM_VSCROLL:    return self->OnVScroll(hwnd, wp);
    case WM_MOUSEWHEEL: return self->OnMouseWheel(hwnd, GET_WHEEL_DELTA_WPARAM(wp));
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

    // Filter buttons — owner-drawn pill style
    int btnX = 16;
    for (int i = 0; i < 5; i++) {
        _hFilterBtns[i] = CreateWindowEx(0, L"BUTTON", ALERT_FILTER_LABELS[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            btnX, 10, 80, 30, hwnd, (HMENU)(9600 + i), hInst, nullptr);
        btnX += 84;
    }

    _hBtnClearAll = CreateWindowEx(0, L"BUTTON", L"Clear All",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        cx - 110, 10, 94, 30, hwnd, (HMENU)IDC_BTN_CLEAR_ALERTS, hInst, nullptr);

    // Alert list — fixed height, scrollable tab compensates
    _hAlertList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_VSCROLL,
        16, ALERT_LIST_Y, cx - 32, ALERT_LIST_H,
        hwnd, (HMENU)IDC_LIST_ALERTS, hInst, nullptr);

    SendMessage(_hAlertList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hAlertList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hAlertList);

    struct ColDef { const wchar_t* name; int w; };
    static const ColDef ALERT_COLS[] = {
        { L"Severity", 80 }, { L"Title", 360 }, { L"Device", 120 },
        { L"Time", 130 }, { L"Status", 80 }
    };
    LVCOLUMN col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
    for (int i = 0; i < 5; i++) {
        col.cx = ALERT_COLS[i].w; col.pszText = (LPWSTR)ALERT_COLS[i].name;
        ListView_InsertColumn(_hAlertList, i, &col);
    }

    // Explanation panel (three-part: What / Why / What to do)
    _hExplainPanel = CreateWindowEx(WS_EX_STATICEDGE, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE,
        16, EXPLAIN_Y, cx - 32, EXPLAIN_H, hwnd, nullptr, hInst, nullptr);

    auto mkExplainLbl = [&](const wchar_t* hdr, int y) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", hdr, WS_CHILD | WS_VISIBLE | SS_LEFT,
            6, y, cx - 48, 18, _hExplainPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
        return hw;
    };
    auto mkExplainEdit = [&](int id, int y, int h) -> HWND {
        HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            6, y, cx - 48, h, _hExplainPanel, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        Theme::ApplyDarkEdit(hw);
        return hw;
    };
    int thirdH = (EXPLAIN_H - 12) / 3 - 18;
    int ey = 4;
    _hLblWhat = mkExplainLbl(L"What happened:", ey); ey += 18;
    _hExplainWhat = mkExplainEdit(9650, ey, thirdH); ey += thirdH + 4;
    _hLblWhy = mkExplainLbl(L"Why it matters:", ey); ey += 18;
    _hExplainWhy = mkExplainEdit(9651, ey, thirdH); ey += thirdH + 4;
    _hLblDo = mkExplainLbl(L"What to do:", ey); ey += 18;
    _hExplainDo  = mkExplainEdit(9652, ey, thirdH);

    if (_hExplainWhat) SetWindowText(_hExplainWhat, L"Select an alert above to see details.");

    // Risk context labels — direct children of tab (not explain panel) so WM_CTLCOLORSTATIC reaches WndProc
    _hRiskPrevalence = CreateWindowEx(0, L"STATIC",
        L"Select an alert above to see risk context.",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        16, RISK_PREV_Y, cx - 32, 22, hwnd, nullptr, hInst, nullptr);
    SendMessage(_hRiskPrevalence, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);

    _hRiskAction = CreateWindowEx(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        16, RISK_ACT_Y, cx - 32, 22, hwnd, nullptr, hInst, nullptr);
    SendMessage(_hRiskAction, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);

    // Rules section — positioned below explain panel
    _hBtnAddRule = CreateWindowEx(0, L"BUTTON", L"Add Rule",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        cx - 308, RULE_LIST_Y - 30, 96, 28, hwnd, (HMENU)IDC_BTN_ADD_RULE, hInst, nullptr);

    _hBtnEditRule = CreateWindowEx(0, L"BUTTON", L"Edit",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        cx - 206, RULE_LIST_Y - 30, 68, 28, hwnd, (HMENU)IDC_BTN_EDIT_RULE, hInst, nullptr);

    _hBtnDelRule = CreateWindowEx(0, L"BUTTON", L"Delete",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        cx - 132, RULE_LIST_Y - 30, 68, 28, hwnd, (HMENU)IDC_BTN_DEL_RULE, hInst, nullptr);

    _hRuleList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_VSCROLL,
        16, RULE_LIST_Y, cx - 32, RULE_LIST_H,
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
    _viewHeight    = cy;
    _contentHeight = TAB_CONTENT_H;

    int maxScroll = std::max(0, _contentHeight - _viewHeight);
    if (_scrollY > maxScroll) _scrollY = maxScroll;

    int sOff = -_scrollY;

    HDWP hdwp = BeginDeferWindowPos(16);
    auto deferMove = [&](HWND hw, int x, int y, int w, int h) {
        if (hw && hdwp)
            hdwp = DeferWindowPos(hdwp, hw, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    // Filter bar (scrolls with content)
    int btnX = 16;
    for (int i = 0; i < 5; i++) {
        deferMove(_hFilterBtns[i], btnX, 10 + sOff, 80, 30);
        btnX += 84;
    }
    deferMove(_hBtnClearAll, cx - 110, 10 + sOff, 94, 30);

    // Alert list — fixed height
    deferMove(_hAlertList, 16, ALERT_LIST_Y + sOff, cx - 32, ALERT_LIST_H);

    // Risk context labels
    deferMove(_hRiskPrevalence, 16, RISK_PREV_Y + sOff, cx - 32, 22);
    deferMove(_hRiskAction,     16, RISK_ACT_Y  + sOff, cx - 32, 22);

    // Explain panel — fixed height, children stay relative to panel
    deferMove(_hExplainPanel, 16, EXPLAIN_Y + sOff, cx - 32, EXPLAIN_H);

    // Re-layout explain panel children on width change
    int panelW = cx - 48;
    int thirdH = (EXPLAIN_H - 12) / 3 - 18;
    int ey = 4;
    if (_hLblWhat)     SetWindowPos(_hLblWhat,     nullptr, 6, ey, panelW, 18,     SWP_NOZORDER); ey += 18;
    if (_hExplainWhat) SetWindowPos(_hExplainWhat, nullptr, 6, ey, panelW, thirdH, SWP_NOZORDER); ey += thirdH + 4;
    if (_hLblWhy)      SetWindowPos(_hLblWhy,      nullptr, 6, ey, panelW, 18,     SWP_NOZORDER); ey += 18;
    if (_hExplainWhy)  SetWindowPos(_hExplainWhy,  nullptr, 6, ey, panelW, thirdH, SWP_NOZORDER); ey += thirdH + 4;
    if (_hLblDo)       SetWindowPos(_hLblDo,       nullptr, 6, ey, panelW, 18,     SWP_NOZORDER); ey += 18;
    if (_hExplainDo)   SetWindowPos(_hExplainDo,   nullptr, 6, ey, panelW, thirdH, SWP_NOZORDER);

    // Rules section — fixed height
    deferMove(_hBtnAddRule,  cx - 308, RULE_LIST_Y - 30 + sOff, 96, 28);
    deferMove(_hBtnEditRule, cx - 206, RULE_LIST_Y - 30 + sOff, 68, 28);
    deferMove(_hBtnDelRule,  cx - 132, RULE_LIST_Y - 30 + sOff, 68, 28);
    deferMove(_hRuleList,    16,       RULE_LIST_Y + sOff,       cx - 32, RULE_LIST_H);

    if (hdwp) EndDeferWindowPos(hdwp);

    UpdateScrollBar(_hwnd);
}

void TabAlerts::UpdateScrollBar(HWND hwnd) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = _contentHeight;
    si.nPage  = _viewHeight;
    si.nPos   = _scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

LRESULT TabAlerts::OnVScroll(HWND hwnd, WPARAM wp) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    int oldPos = _scrollY;
    switch (LOWORD(wp)) {
    case SB_LINEUP:     _scrollY -= 30;         break;
    case SB_LINEDOWN:   _scrollY += 30;         break;
    case SB_PAGEUP:     _scrollY -= si.nPage;   break;
    case SB_PAGEDOWN:   _scrollY += si.nPage;   break;
    case SB_THUMBTRACK: _scrollY = si.nTrackPos; break;
    }

    int maxScroll = std::max(0, _contentHeight - _viewHeight);
    if (_scrollY < 0)          _scrollY = 0;
    if (_scrollY > maxScroll)  _scrollY = maxScroll;

    if (_scrollY != oldPos) {
        RECT rc; GetClientRect(hwnd, &rc);
        LayoutControls(rc.right, rc.bottom);
        UpdateScrollBar(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
}

LRESULT TabAlerts::OnMouseWheel(HWND hwnd, int delta) {
    int oldPos = _scrollY;
    _scrollY -= delta / 2;

    int maxScroll = std::max(0, _contentHeight - _viewHeight);
    if (_scrollY < 0)         _scrollY = 0;
    if (_scrollY > maxScroll) _scrollY = maxScroll;

    if (_scrollY != oldPos) {
        RECT rc; GetClientRect(hwnd, &rc);
        LayoutControls(rc.right, rc.bottom);
        UpdateScrollBar(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
}

LRESULT TabAlerts::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabAlerts::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushSurface());

    int sOff = -_scrollY;

    // Separator below filter bar
    RECT sep = { 16, 44 + sOff, rc.right - 16, 45 + sOff };
    FillRect(hdc, &sep, Theme::BrushBorderSubtle());

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontCaption());

    // "ACTIVE ALERTS" — just above the alert list
    RECT hdr1 = { 16, ALERT_LIST_Y - 16 + sOff, 200, ALERT_LIST_Y - 2 + sOff };
    DrawText(hdc, L"ACTIVE ALERTS", -1, &hdr1, DT_LEFT | DT_SINGLELINE);

    // separator + "RISK CONTEXT" between list and risk labels
    RECT sep2 = { 16, RISK_PREV_Y - 16 + sOff, rc.right - 16, RISK_PREV_Y - 15 + sOff };
    FillRect(hdc, &sep2, Theme::BrushBorderSubtle());
    RECT hdr_risk = { 16, RISK_PREV_Y - 14 + sOff, 200, RISK_PREV_Y - 2 + sOff };
    DrawText(hdc, L"RISK CONTEXT", -1, &hdr_risk, DT_LEFT | DT_SINGLELINE);

    // separator + "ALERT DETAIL" between risk context and explain panel
    RECT sep3 = { 16, EXPLAIN_Y - 16 + sOff, rc.right - 16, EXPLAIN_Y - 15 + sOff };
    FillRect(hdc, &sep3, Theme::BrushBorderSubtle());
    RECT hdr_detail = { 16, EXPLAIN_Y - 14 + sOff, 200, EXPLAIN_Y - 2 + sOff };
    DrawText(hdc, L"ALERT DETAIL", -1, &hdr_detail, DT_LEFT | DT_SINGLELINE);

    // "ALERT RULES" — just above the rule buttons
    RECT hdr2 = { 16, RULE_LIST_Y - 48 + sOff, 200, RULE_LIST_Y - 34 + sOff };
    DrawText(hdc, L"ALERT RULES", -1, &hdr2, DT_LEFT | DT_SINGLELINE);

    SelectObject(hdc, old);
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

struct AlertRisk {
    int prevalence;              // 0-100, how often this appears on typical home networks
    const wchar_t* label;        // "Very Common", "Common", "Uncommon", "Rare"
    const wchar_t* actionText;   // short action verdict
    COLORREF actionColor;        // semantic color from Theme
    const wchar_t* actionReason; // one-line rationale
};

static AlertRisk GetAlertRisk(const wstring& type) {
    if (type == L"new_device")
        return {78, L"Very Common",  L"Review Suggested",      Theme::ACCENT_CYAN,   L"New devices join routinely \u2014 verify it\u2019s one you recognize"};
    if (type == L"risky_port")
        return {28, L"Uncommon",     L"Action Required",        Theme::ACCENT_RED,    L"Exposed services create a real attack surface on your network"};
    if (type == L"port_changed")
        return {38, L"Uncommon",     L"Action Recommended",     Theme::ACCENT_AMBER,  L"Port changes may indicate a firmware update or an unauthorized service"};
    if (type == L"device_offline")
        return {85, L"Very Common",  L"No Action Needed",       Theme::ACCENT_GREEN,  L"Devices go offline constantly \u2014 this is expected behavior"};
    if (type == L"ip_changed")
        return {60, L"Common",       L"No Action Needed",       Theme::ACCENT_GREEN,  L"DHCP lease renewals are normal \u2014 only investigate if the device is sensitive"};
    if (type == L"internet_outage")
        return {55, L"Common",       L"Review Suggested",       Theme::ACCENT_CYAN,   L"Check router/modem if the outage lasts more than a few minutes"};
    if (type == L"gateway_mac_changed")
        return {3,  L"Rare",         L"Action Required",        Theme::ACCENT_RED,    L"Strongly indicates ARP spoofing or an unauthorized router replacement"};
    if (type == L"dns_changed")
        return {6,  L"Rare",         L"Action Required",        Theme::ACCENT_RED,    L"DNS hijacking can silently redirect all internet traffic on your network"};
    if (type == L"high_latency")
        return {65, L"Common",       L"No Action Needed",       Theme::ACCENT_GREEN,  L"Usually transient \u2014 investigate only if latency is persistent or severe"};
    if (type == L"hostname_changed")
        return {20, L"Uncommon",     L"Action Recommended",     Theme::ACCENT_AMBER,  L"Device was renamed \u2014 verify this change was intentional"};
    return    {25, L"Uncommon",     L"Review Suggested",       Theme::ACCENT_CYAN,   L"Investigate to confirm this is expected behavior on your network"};
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
        if (_hExplainWhat)    SetWindowText(_hExplainWhat, L"Select an alert above to see details.");
        if (_hExplainWhy)     SetWindowText(_hExplainWhy, L"");
        if (_hExplainDo)      SetWindowText(_hExplainDo, L"");
        if (_hRiskPrevalence) SetWindowText(_hRiskPrevalence, L"Select an alert above to see risk context.");
        if (_hRiskAction)     SetWindowText(_hRiskAction, L"");
        _actionColor = Theme::TEXT_SECONDARY;
        return;
    }

    const Anomaly& a = *visible[anomalyIdx];

    // Risk context
    AlertRisk risk = GetAlertRisk(a.type);
    wstring prevText = L"Prevalence on home networks: " + wstring(risk.label) +
                       L" (" + std::to_wstring(risk.prevalence) + L"/100)  \u00B7  " +
                       risk.actionReason;
    if (_hRiskPrevalence) SetWindowText(_hRiskPrevalence, prevText.c_str());
    if (_hRiskAction) {
        SetWindowText(_hRiskAction, risk.actionText);
        _actionColor = risk.actionColor;
        InvalidateRect(_hRiskAction, nullptr, FALSE);
    }

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
            COLORREF bg = Theme::BG_SURFACE;
            if (_mainWnd) {
                ScanResult r = _mainWnd->GetLastResult();
                int visible = 0;
                for (auto& a : r.anomalies) {
                    if (visible == row) {
                        if (a.severity == L"critical" || a.severity == L"high")
                            bg = Theme::AlphaBlend(Theme::ACCENT_RED, Theme::BG_SURFACE, 10);
                        else if (a.severity == L"medium")
                            bg = Theme::AlphaBlend(Theme::ACCENT_AMBER, Theme::BG_SURFACE, 10);
                        else
                            bg = Theme::AlphaBlend(Theme::ACCENT_GREEN, Theme::BG_SURFACE, 10);
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

    // Dark theme with glassmorphism-inspired styling
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
