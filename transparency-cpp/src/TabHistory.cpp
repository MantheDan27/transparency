#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>

#include "TabHistory.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

const wchar_t* TabHistory::s_className = L"TabHistoryWnd";

bool TabHistory::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
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
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);
    return _hwnd != nullptr;
}

LRESULT CALLBACK TabHistory::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabHistory* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabHistory*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabHistory*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:      return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:        self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:       return self->OnPaint(hwnd);
    case WM_COMMAND:     return self->OnCommand(hwnd, wp, lp);
    case WM_ERASEBKGND:  return 1;
    case WM_SCAN_COMPLETE: self->Refresh(); return 0;

    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lp);
        if (hdr->code == NM_CUSTOMDRAW)
            return self->OnCustomDraw(hdr);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        SetTextColor((HDC)wp, Theme::TEXT_PRIMARY);
        SetBkColor((HDC)wp, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }

    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabHistory::OnCreate(HWND hwnd, LPCREATESTRUCT) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    return 0;
}

LRESULT TabHistory::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabHistory::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdcScreen = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int cx = rc.right, cy = rc.bottom;
    if (cx <= 0 || cy <= 0) { EndPaint(hwnd, &ps); return 0; }

    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hOld = (HBITMAP)SelectObject(hdc, hBmp);

    FillRect(hdc, &rc, Theme::BrushSurface());

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontH2());
    RECT titleRc = { 16, 12, cx - 16, 38 };
    DrawText(hdc, L"History", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, Theme::TEXT_SECONDARY);
    SelectObject(hdc, Theme::FontBodySm());
    RECT subRc = { 16, 38, cx - 16, 54 };
    DrawText(hdc, L"Device connection and disconnection events", -1, &subRc,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT sep = { 16, 58, cx - 16, 59 };
    FillRect(hdc, &sep, Theme::BrushBorderSubtle());

    BitBlt(hdcScreen, 0, 0, cx, cy, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldFont);
    SelectObject(hdc, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdc);
    EndPaint(hwnd, &ps);
    return 0;
}

void TabHistory::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    auto mkBtn = [&](const wchar_t* txt, int id, int x, int y, int w, int h) -> HWND {
        HWND hw = CreateWindowEx(0, L"BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };

    int y = 66;

    // Filter buttons
    _hBtnAll    = mkBtn(L"All Events",   IDC_BTN_HIST_ALL,        16, y, 90, 26);
    _hBtnJoined = mkBtn(L"\u2191 Joined",  IDC_BTN_HIST_JOINED,   114, y, 80, 26);
    _hBtnLeft   = mkBtn(L"\u2193 Left",    IDC_BTN_HIST_LEFT,     202, y, 70, 26);
    _hBtnRecon  = mkBtn(L"\u21BA Reconnected", IDC_BTN_HIST_RECONNECTED, 280, y, 110, 26);

    // Clear button — right-aligned
    _hBtnClear  = mkBtn(L"Clear History", IDC_BTN_HIST_CLEAR, cx - 116, y, 100, 26);

    y += 36;

    // Main list
    _hList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        16, y, cx - 32, cy - y - 8,
        hwnd, (HMENU)(INT_PTR)IDC_HIST_LIST, hInst, nullptr);
    SendMessage(_hList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hList);

    LVCOLUMN col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
    struct ColDef { const wchar_t* name; int w; };
    static const ColDef COLS[] = {
        { L"Time",         140 },
        { L"Event",         90 },
        { L"Device Name",  160 },
        { L"IP Address",   110 },
        { L"MAC Address",  130 },
        { L"Vendor",       160 },
    };
    for (int i = 0; i < 6; i++) {
        col.cx = COLS[i].w; col.pszText = (LPWSTR)COLS[i].name;
        ListView_InsertColumn(_hList, i, &col);
    }

    UpdateFilterButtons();
    PopulateList();
}

void TabHistory::LayoutControls(int cx, int cy) {
    if (!_hList) return;

    // Reposition Clear button to right edge
    if (_hBtnClear) SetWindowPos(_hBtnClear, nullptr, cx - 116, 66, 100, 26, SWP_NOZORDER);

    // Resize list to fill remaining space
    SetWindowPos(_hList, nullptr, 16, 102, cx - 32, cy - 110, SWP_NOZORDER);
}

// ── Population ────────────────────────────────────────────────────────────────

void TabHistory::PopulateList() {
    if (!_hList || !_mainWnd) return;
    ListView_DeleteAllItems(_hList);

    auto history = _mainWnd->GetDeviceHistory();

    // Display newest events first
    int row = 0;
    for (int i = (int)history.size() - 1; i >= 0; i--) {
        const auto& ev = history[i];
        if (_filter == 1 && ev.eventType != L"joined")      continue;
        if (_filter == 2 && ev.eventType != L"left")        continue;
        if (_filter == 3 && ev.eventType != L"reconnected") continue;

        // Build event label
        wstring label;
        if      (ev.eventType == L"joined")      label = L"\u2191 Joined";
        else if (ev.eventType == L"left")        label = L"\u2193 Left";
        else if (ev.eventType == L"reconnected") label = L"\u21BA Reconnected";
        else                                     label = ev.eventType;

        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = row;
        lvi.pszText = (LPWSTR)ev.timestamp.c_str();
        ListView_InsertItem(_hList, &lvi);
        ListView_SetItemText(_hList, row, 1, (LPWSTR)label.c_str());
        ListView_SetItemText(_hList, row, 2, (LPWSTR)ev.deviceName.c_str());
        ListView_SetItemText(_hList, row, 3, (LPWSTR)ev.deviceIp.c_str());
        ListView_SetItemText(_hList, row, 4, (LPWSTR)ev.deviceMac.c_str());
        ListView_SetItemText(_hList, row, 5, (LPWSTR)ev.vendor.c_str());

        // Store eventType index in lParam for custom draw colouring
        LVITEM li2 = {}; li2.mask = LVIF_PARAM; li2.iItem = row;
        li2.lParam = (ev.eventType == L"joined") ? 1 :
                     (ev.eventType == L"left")   ? 2 : 3;
        ListView_SetItem(_hList, &li2);

        row++;
    }
}

void TabHistory::UpdateFilterButtons() {
    // Bold the active filter button by toggling BS_DEFPUSHBUTTON
    struct { HWND h; int idx; } btns[] = {
        { _hBtnAll,    0 }, { _hBtnJoined, 1 },
        { _hBtnLeft,   2 }, { _hBtnRecon,  3 },
    };
    for (auto& b : btns) {
        if (!b.h) continue;
        LONG style = GetWindowLong(b.h, GWL_STYLE) & ~BS_DEFPUSHBUTTON;
        if (b.idx == _filter) style |= BS_DEFPUSHBUTTON;
        SetWindowLong(b.h, GWL_STYLE, style);
        InvalidateRect(b.h, nullptr, TRUE);
    }
}

// ── Custom draw — colour event column ─────────────────────────────────────────

LRESULT TabHistory::OnCustomDraw(NMHDR* hdr) {
    if (!_hList || hdr->hwndFrom != _hList) return CDRF_DODEFAULT;
    auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(hdr);

    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
        return CDRF_NOTIFYITEMDRAW;

    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
        return CDRF_NOTIFYSUBITEMDRAW;

    if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
        if (cd->iSubItem == 1) {
            LPARAM type = cd->nmcd.lItemlParam;
            if      (type == 1) cd->clrText = Theme::ACCENT_GREEN;  // joined
            else if (type == 2) cd->clrText = Theme::ACCENT_RED;    // left
            else if (type == 3) cd->clrText = Theme::ACCENT_AMBER;  // reconnected
            return CDRF_NEWFONT;
        }
    }
    return CDRF_DODEFAULT;
}

// ── Commands ──────────────────────────────────────────────────────────────────

LRESULT TabHistory::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    switch (LOWORD(wp)) {
    case IDC_BTN_HIST_ALL:        _filter = 0; break;
    case IDC_BTN_HIST_JOINED:     _filter = 1; break;
    case IDC_BTN_HIST_LEFT:       _filter = 2; break;
    case IDC_BTN_HIST_RECONNECTED:_filter = 3; break;
    case IDC_BTN_HIST_CLEAR:
        if (_mainWnd) _mainWnd->ClearDeviceHistory();
        break;
    default:
        return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
    }
    UpdateFilterButtons();
    PopulateList();
    return 0;
}

void TabHistory::Refresh() {
    PopulateList();
}
