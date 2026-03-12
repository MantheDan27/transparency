#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <cstring>

#include "TabDevices.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"
#include "Scanner.h"
#include "FirebaseClient.h"
#include <shellapi.h>

using std::wstring;

const wchar_t* TabDevices::s_className = L"TransparencyTabDevices";

// ─── Forward declarations for drawing helpers ─────────────────────────────────
static void FillRoundRect(HDC hdc, const RECT& rc, int r, HBRUSH brush);

// ─── Detail panel subclass proc — routes WM_DRAWITEM for conf bar ─────────────
static LRESULT CALLBACK DetailPanelSubclassProc(
    HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    TabDevices* self = reinterpret_cast<TabDevices*>(dwRefData);

    if (msg == WM_DRAWITEM) {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
        if (dis->CtlID == IDC_CONF_BAR && self) {
            HDC  hdc  = dis->hDC;
            RECT rc   = dis->rcItem;
            int  conf = self->_detailConfidence;

            // Background track
            HBRUSH trackBr = CreateSolidBrush(Theme::BG_ELEVATED);
            FillRoundRect(hdc, rc, rc.bottom - rc.top, trackBr);
            DeleteObject(trackBr);

            // Filled portion
            if (conf > 0) {
                int fillW = (int)((rc.right - rc.left) * conf / 100.0);
                if (fillW > 0) {
                    RECT fillRc = rc;
                    fillRc.right = fillRc.left + fillW;
                    COLORREF fillColor = conf >= 80 ? Theme::SUCCESS
                                       : conf >= 50 ? Theme::ACCENT
                                                    : Theme::WARNING;
                    HBRUSH fillBr = CreateSolidBrush(fillColor);
                    FillRoundRect(hdc, fillRc, rc.bottom - rc.top, fillBr);
                    DeleteObject(fillBr);
                }
            }
            return TRUE;
        }
    }

    if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORBTN) {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_CARD);
        return (LRESULT)Theme::BrushCard();
    }

    if (msg == WM_ERASEBKGND) {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, Theme::BrushCard());
        return 1;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static const wchar_t* FILTER_LABELS[] = {
    L"All", L"Online", L"Unknown", L"Watchlist", L"Owned", L"Changed"
};

// ─── Risky ports ──────────────────────────────────────────────────────────────
static bool IsRiskyPort(int port) {
    switch (port) {
    case 23: case 135: case 139: case 445: case 3389: case 5900:
        return true;
    default:
        return false;
    }
}

// ─── Rounded rectangle helper ─────────────────────────────────────────────────
static void FillRoundRect(HDC hdc, const RECT& rc, int r, HBRUSH brush) {
    HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right, rc.bottom, r, r);
    FillRgn(hdc, rgn, brush);
    DeleteObject(rgn);
}

static void FrameRoundRect(HDC hdc, const RECT& rc, int r, HPEN pen) {
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, r, r);
    SelectObject(hdc, old);
    SelectObject(hdc, oldBr);
}

// ─── Draw a small pill/badge centered in a cell rect ──────────────────────────
static void DrawPill(HDC hdc, const RECT& cellRc, const wchar_t* text,
                     COLORREF bgColor, COLORREF textColor,
                     HFONT font, bool centered = true) {
    // Measure text
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SIZE sz = {};
    GetTextExtentPoint32(hdc, text, (int)wcslen(text), &sz);
    SelectObject(hdc, oldFont);

    int padX = 8, padY = 2;
    int pillW = sz.cx + padX * 2;
    int pillH = sz.cy + padY * 2;

    int x, y;
    if (centered) {
        x = cellRc.left + (cellRc.right - cellRc.left - pillW) / 2;
    } else {
        x = cellRc.left + 4;
    }
    y = cellRc.top + (cellRc.bottom - cellRc.top - pillH) / 2;

    RECT pillRc = { x, y, x + pillW, y + pillH };

    HBRUSH br = CreateSolidBrush(bgColor);
    FillRoundRect(hdc, pillRc, pillH, br);
    DeleteObject(br);

    oldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, textColor);
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, text, -1, &pillRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

// ─── Draw a status dot (filled circle) ────────────────────────────────────────
static void DrawStatusDot(HDC hdc, const RECT& cellRc, COLORREF color) {
    int dotDiam = 8;
    int cx = cellRc.left + (cellRc.right - cellRc.left) / 2;
    int cy = cellRc.top  + (cellRc.bottom - cellRc.top)  / 2;

    RECT dotRc = {
        cx - dotDiam / 2,
        cy - dotDiam / 2,
        cx + dotDiam / 2,
        cy + dotDiam / 2
    };

    HBRUSH br = CreateSolidBrush(color);
    HBRUSH old = (HBRUSH)SelectObject(hdc, br);
    HPEN pen = CreatePen(PS_SOLID, 0, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    Ellipse(hdc, dotRc.left, dotRc.top, dotRc.right, dotRc.bottom);
    SelectObject(hdc, old);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);
}

// ─── Trust pill color config ───────────────────────────────────────────────────
static void GetTrustColors(const wstring& trust, COLORREF& bg, COLORREF& fg) {
    if (trust == L"owned") {
        bg = Theme::SUCCESS;
        fg = RGB(8, 24, 16);
    } else if (trust == L"watchlist") {
        bg = Theme::TRUST_WATCHLIST;
        fg = RGB(255, 255, 255);
    } else if (trust == L"blocked") {
        bg = Theme::DANGER;
        fg = RGB(255, 255, 255);
    } else if (trust == L"guest") {
        bg = Theme::ACCENT;
        fg = RGB(255, 255, 255);
    } else {
        // unknown
        bg = Theme::WARNING;
        fg = RGB(30, 24, 8);
    }
}

// ─── JSON helpers ─────────────────────────────────────────────────────────────
static wstring EscapeJson(const wstring& text) {
    wstring out;
    out.reserve(text.size() + 8);
    for (wchar_t ch : text) {
        switch (ch) {
        case L'\\': out += L"\\\\"; break;
        case L'"':  out += L"\\\""; break;
        case L'\r': out += L"\\r"; break;
        case L'\n': out += L"\\n"; break;
        case L'\t': out += L"\\t"; break;
        default:
            if (ch < 0x20) out += L' ';
            else out += ch;
            break;
        }
    }
    return out;
}

static wstring PortsToJsonArray(const Device& dev) {
    wstring json = L"[";
    for (size_t i = 0; i < dev.openPorts.size(); ++i) {
        if (i > 0) json += L",";
        json += std::to_wstring(dev.openPorts[i]);
    }
    json += L"]";
    return json;
}

static wstring BuildDeviceSummaryJson(const Device& dev) {
    wstring name = dev.customName.empty() ? dev.hostname : dev.customName;
    if (name.empty()) name = dev.ip;

    wstring json = L"{\r\n";
    json += L"  \"ip\": \""     + EscapeJson(dev.ip)         + L"\",\r\n";
    json += L"  \"mac\": \""    + EscapeJson(dev.mac)        + L"\",\r\n";
    json += L"  \"hostnameOrCustomName\": \"" + EscapeJson(name) + L"\",\r\n";
    json += L"  \"vendor\": \"" + EscapeJson(dev.vendor)     + L"\",\r\n";
    json += L"  \"type\": \""   + EscapeJson(dev.deviceType) + L"\",\r\n";
    json += L"  \"trust\": \""  + EscapeJson(dev.trustState) + L"\",\r\n";
    json += L"  \"ports\": "    + PortsToJsonArray(dev)      + L",\r\n";
    json += L"  \"online\": ";
    json += dev.online ? L"true" : L"false";
    json += L",\r\n";
    json += L"  \"confidence\": " + std::to_wstring(dev.confidence) + L"\r\n";
    json += L"}";
    return json;
}

static bool CopyToClipboard(HWND hwnd, const wstring& text) {
    if (!OpenClipboard(hwnd)) return false;
    EmptyClipboard();

    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hg) { CloseClipboard(); return false; }

    void* ptr = GlobalLock(hg);
    if (!ptr) { GlobalFree(hg); CloseClipboard(); return false; }
    memcpy(ptr, text.c_str(), bytes);
    GlobalUnlock(hg);

    if (!SetClipboardData(CF_UNICODETEXT, hg)) {
        GlobalFree(hg);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

// ─── Create ───────────────────────────────────────────────────────────────────
bool TabDevices::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
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

// ─── Window procedure ─────────────────────────────────────────────────────────
LRESULT CALLBACK TabDevices::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabDevices* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabDevices*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabDevices*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:
        return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));

    case WM_SIZE:
        self->OnSize(hwnd, LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_PAINT:
        return self->OnPaint(hwnd);

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, Theme::BrushApp());
        return 1;
    }

    case WM_COMMAND:
        return self->OnCommand(hwnd, wp, lp);

    case WM_NOTIFY:
        return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));

    // Owner-draw filter pill buttons (direct children of this hwnd)
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
        int id = (int)dis->CtlID;
        if (id >= IDC_BTN_FILTER_ALL && id <= IDC_BTN_FILTER_CHANGED) {
            self->DrawFilterButton(dis, id - IDC_BTN_FILTER_ALL);
            return TRUE;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        HWND hCtrl = (HWND)lp;
        // Detail panel labels - use card background
        if (self->_hDetailPanel && IsChild(self->_hDetailPanel, hCtrl)) {
            SetTextColor(hdc, Theme::TEXT_PRIMARY);
            SetBkColor(hdc, Theme::BG_CARD);
            return (LRESULT)Theme::BrushCard();
        }
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_APP);
        return (LRESULT)Theme::BrushApp();
    }

    case WM_SCAN_COMPLETE:
        return self->OnScanComplete(hwnd);

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

// ─── Draw a filter pill button (owner-draw) ───────────────────────────────────
void TabDevices::DrawFilterButton(DRAWITEMSTRUCT* dis, int filterIdx) {
    HDC hdc     = dis->hDC;
    RECT rc     = dis->rcItem;
    bool active = (_filterMode == filterIdx);

    // Background pill
    COLORREF bg   = active ? Theme::ACCENT    : Theme::BG_INPUT;
    COLORREF fg   = active ? RGB(255,255,255) : Theme::TEXT_SECONDARY;
    COLORREF borC = active ? Theme::ACCENT    : Theme::BORDER;

    // Fill rounded pill
    HBRUSH br = CreateSolidBrush(bg);
    FillRoundRect(hdc, rc, rc.bottom - rc.top, br);
    DeleteObject(br);

    // Border outline
    if (!active) {
        HPEN pen = CreatePen(PS_SOLID, 1, borC);
        FrameRoundRect(hdc, rc, rc.bottom - rc.top, pen);
        DeleteObject(pen);
    }

    // Label text
    const wchar_t* label = FILTER_LABELS[filterIdx];

    // Count badge value
    int count = _filterCounts[filterIdx];
    wchar_t countStr[16];
    wsprintf(countStr, L"%d", count);

    // Measure label
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontSmall());
    SIZE labelSz = {};
    GetTextExtentPoint32(hdc, label, (int)wcslen(label), &labelSz);

    // Measure badge
    HFONT monoFont = Theme::FontSmall();
    SIZE badgeSz = {};
    GetTextExtentPoint32(hdc, countStr, (int)wcslen(countStr), &badgeSz);
    SelectObject(hdc, oldFont);

    int gap = 4;
    int badgePadX = 5;
    int badgeW = badgeSz.cx + badgePadX * 2;
    if (badgeW < 18) badgeW = 18;
    int badgeH = badgeSz.cy + 2;

    int totalW = labelSz.cx + gap + badgeW;
    int startX = rc.left + (rc.right - rc.left - totalW) / 2;
    int midY   = rc.top  + (rc.bottom - rc.top) / 2;

    // Draw label
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    oldFont = (HFONT)SelectObject(hdc, Theme::FontSmall());

    RECT labelRc = {
        startX,
        midY - labelSz.cy / 2,
        startX + labelSz.cx,
        midY + labelSz.cy / 2
    };
    DrawText(hdc, label, -1, &labelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Draw badge
    int badgeX = startX + labelSz.cx + gap;
    int badgeY = midY - badgeH / 2;
    RECT badgeRc = { badgeX, badgeY, badgeX + badgeW, badgeY + badgeH };

    COLORREF badgeBg  = active ? RGB(255, 255, 255) : Theme::BG_ELEVATED;
    COLORREF badgeFg  = active ? Theme::ACCENT       : Theme::TEXT_MUTED;

    HBRUSH badgeBr = CreateSolidBrush(badgeBg);
    FillRoundRect(hdc, badgeRc, badgeH, badgeBr);
    DeleteObject(badgeBr);

    SetTextColor(hdc, badgeFg);
    DrawText(hdc, countStr, -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);

    // Focus rect (accessibility)
    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(hdc, &rc);
}

// ─── Compute filter counts from current data ──────────────────────────────────
void TabDevices::UpdateFilterCounts() {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();

    _filterCounts[0] = (int)r.devices.size(); // All
    _filterCounts[1] = 0; // Online
    _filterCounts[2] = 0; // Unknown
    _filterCounts[3] = 0; // Watchlist
    _filterCounts[4] = 0; // Owned
    _filterCounts[5] = 0; // Changed

    for (auto& d : r.devices) {
        if (d.online)                  _filterCounts[1]++;
        if (d.trustState == L"unknown")   _filterCounts[2]++;
        if (d.trustState == L"watchlist") _filterCounts[3]++;
        if (d.trustState == L"owned")     _filterCounts[4]++;
        if (d.prevPorts != d.openPorts)   _filterCounts[5]++;
    }
}

// ─── OnCreate ─────────────────────────────────────────────────────────────────
LRESULT TabDevices::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    return 0;
}

// ─── CreateControls ───────────────────────────────────────────────────────────
void TabDevices::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // ── Search box ────────────────────────────────────────────────────────────
    _hSearch = CreateWindowEx(0, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        16, 12, 260, 28, hwnd, (HMENU)IDC_EDIT_SEARCH, hInst, nullptr);
    SendMessage(_hSearch, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hSearch, EM_SETCUEBANNER, FALSE, (LPARAM)L"  Search registry...");
    Theme::ApplyDarkEdit(_hSearch);

    // ── Filter pill buttons (owner-draw) ──────────────────────────────────────
    int btnX = 288;
    for (int i = 0; i < 6; i++) {
        _hFilterBtns[i] = CreateWindowEx(0, L"BUTTON", FILTER_LABELS[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            btnX, 10, 82, 28, hwnd,
            (HMENU)(IDC_BTN_FILTER_ALL + i), hInst, nullptr);
        SendMessage(_hFilterBtns[i], WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
        btnX += 86;
    }

    // ── List view ─────────────────────────────────────────────────────────────
    int listW = _detailVisible ? cx - DETAIL_WIDTH - 32 : cx - 32;
    _hList = CreateWindowEx(
        0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS |
        LVS_SINGLESEL | WS_VSCROLL | WS_HSCROLL | LVS_OWNERDATA,
        16, 52, listW, cy - 68,
        hwnd, (HMENU)IDC_LIST_DEVICES, hInst, nullptr);

    SendMessage(_hList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);

    Theme::ApplyDarkScrollbar(_hList);

    // Style the header
    HWND hHeader = ListView_GetHeader(_hList);
    if (hHeader) {
        SetWindowTheme(hHeader, L"DarkMode_ItemsView", nullptr);
    }

    // ── Columns ───────────────────────────────────────────────────────────────
    struct ColDef { const wchar_t* name; int width; int fmt; };
    static const ColDef COLS[] = {
        { L"",           18,  LVCFMT_CENTER }, // Status dot
        { L"Name",       170, LVCFMT_LEFT   },
        { L"IP Address", 120, LVCFMT_LEFT   },
        { L"MAC",        130, LVCFMT_LEFT   },
        { L"Vendor",     110, LVCFMT_LEFT   },
        { L"Type",       140, LVCFMT_LEFT   },
        { L"Trust",       80, LVCFMT_LEFT   },
        { L"Open Ports", 130, LVCFMT_LEFT   },
        { L"Seen",        50, LVCFMT_CENTER },
        { L"Last Seen",  110, LVCFMT_LEFT   },
    };

    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    for (int i = 0; i < (int)(sizeof(COLS)/sizeof(COLS[0])); i++) {
        col.cx     = COLS[i].width;
        col.pszText = (LPWSTR)COLS[i].name;
        col.fmt    = COLS[i].fmt;
        ListView_InsertColumn(_hList, i, &col);
    }

    // ── Detail panel (hidden by default) ──────────────────────────────────────
    _hDetailPanel = CreateWindowEx(0, L"STATIC", nullptr,
        WS_CHILD | SS_NOTIFY,
        cx - DETAIL_WIDTH - 16, 48, DETAIL_WIDTH, cy - 64,
        hwnd, nullptr, hInst, nullptr);

    // Subclass detail panel so its children's WM_DRAWITEM is handled
    SetWindowSubclass(_hDetailPanel, DetailPanelSubclassProc, 1,
                      (DWORD_PTR)this);

    // ── Detail controls inside panel ──────────────────────────────────────────
    auto makeLbl = [&](const wchar_t* text, int y, int h = 18) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            8, y, DETAIL_WIDTH - 16, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto makeSmallLbl = [&](const wchar_t* text, int y, int h = 16) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            8, y, DETAIL_WIDTH - 16, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
        return hw;
    };
    auto makeMonoLbl = [&](const wchar_t* text, int y, int h = 16) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            8, y, DETAIL_WIDTH - 16, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontMono(), TRUE);
        return hw;
    };

    int dy = 12;

    // Custom name label + edit
    makeSmallLbl(L"CUSTOM NAME", dy, 14); dy += 16;
    _hDetailCustomName = CreateWindowEx(0, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        8, dy, DETAIL_WIDTH - 16, 24, _hDetailPanel,
        (HMENU)IDC_EDIT_DEVICE_NAME, hInst, nullptr);
    SendMessage(_hDetailCustomName, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(_hDetailCustomName);
    dy += 30;

    // Display name (FontHeader)
    _hDetailName = CreateWindowEx(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        8, dy, DETAIL_WIDTH - 16, 28, _hDetailPanel, nullptr, hInst, nullptr);
    SendMessage(_hDetailName, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
    dy += 30;

    // Type + confidence
    _hDetailType = makeLbl(L"", dy, 16); dy += 18;

    // Confidence bar placeholder (drawn in WM_PAINT of detail panel, or via static with custom draw)
    // We use a plain static as a "slot" - actual bar is painted in OnPaintDetailPanel
    _hDetailConfBar = CreateWindowEx(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        8, dy, DETAIL_WIDTH - 16, 8, _hDetailPanel,
        (HMENU)IDC_CONF_BAR, hInst, nullptr);
    dy += 14;

    // Evidence
    _hDetailEvidence = makeSmallLbl(L"", dy, 28); dy += 30;

    // Alt types
    _hDetailAlt = makeSmallLbl(L"", dy, 28); dy += 30;

    // Separator area: Vendor section
    makeSmallLbl(L"VENDOR", dy, 12); dy += 14;
    _hDetailVendor = makeLbl(L"", dy, 16); dy += 20;

    // IP + MAC (mono)
    makeSmallLbl(L"IP / MAC", dy, 12); dy += 14;
    _hDetailMac = makeMonoLbl(L"", dy, 16); dy += 20;

    // Subnet / timestamps
    _hDetailSubnet    = makeSmallLbl(L"", dy, 14); dy += 16;
    _hDetailFirstSeen = makeSmallLbl(L"", dy, 14); dy += 16;
    _hDetailLastSeen  = makeSmallLbl(L"", dy, 14); dy += 16;
    _hDetailSightings = makeSmallLbl(L"", dy, 14); dy += 16;
    _hDetailIpHistory = makeSmallLbl(L"", dy, 14); dy += 18;

    // Ports section
    makeSmallLbl(L"OPEN PORTS", dy, 12); dy += 14;
    _hDetailPorts = makeLbl(L"", dy, 36); dy += 40;

    // mDNS
    _hDetailMdns = makeSmallLbl(L"", dy, 28); dy += 30;

    // IoT risk (hidden when not IoT)
    _hDetailIotRisk = CreateWindowEx(0, L"EDIT", nullptr,
        WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        8, dy, DETAIL_WIDTH - 16, 56, _hDetailPanel, nullptr, hInst, nullptr);
    SendMessage(_hDetailIotRisk, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
    Theme::ApplyDarkEdit(_hDetailIotRisk);
    dy += 64;

    // Anomalies
    _hDetailAnoms = makeSmallLbl(L"", dy, 50); dy += 54;

    // Notes section
    makeSmallLbl(L"NOTES", dy, 12); dy += 14;
    _hDetailNotes = CreateWindowEx(0, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        8, dy, DETAIL_WIDTH - 16, 54, _hDetailPanel,
        (HMENU)IDC_EDIT_DEVICE_NOTES, hInst, nullptr);
    SendMessage(_hDetailNotes, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(_hDetailNotes);
    dy += 60;

    // Trust section
    makeSmallLbl(L"TRUST", dy, 12); dy += 14;
    _hDetailTrust = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        8, dy, DETAIL_WIDTH - 16, 120, _hDetailPanel,
        (HMENU)IDC_COMBO_TRUST, hInst, nullptr);
    SendMessage(_hDetailTrust, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"unknown");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"owned");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"watchlist");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"guest");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"blocked");
    dy += 30;

    // Save + Close buttons
    _hDetailSave = CreateWindowEx(0, L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        8, dy, 80, 26, _hDetailPanel,
        (HMENU)IDC_BTN_DEVICE_SAVE, hInst, nullptr);
    SendMessage(_hDetailSave, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hDetailClose = CreateWindowEx(0, L"BUTTON", L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        DETAIL_WIDTH - 90, dy, 82, 26, _hDetailPanel,
        (HMENU)9500, hInst, nullptr);
    SendMessage(_hDetailClose, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
}

// ─── LayoutControls ───────────────────────────────────────────────────────────
void TabDevices::LayoutControls(int cx, int cy) {
    int listW = _detailVisible ? cx - DETAIL_WIDTH - 32 : cx - 32;
    if (_hList)
        SetWindowPos(_hList, nullptr, 16, 52, listW, cy - 68, SWP_NOZORDER);

    if (_hDetailPanel) {
        if (_detailVisible)
            SetWindowPos(_hDetailPanel, nullptr,
                cx - DETAIL_WIDTH - 16, 48, DETAIL_WIDTH, cy - 64,
                SWP_NOZORDER | SWP_SHOWWINDOW);
        else
            ShowWindow(_hDetailPanel, SW_HIDE);
    }

    // Reposition filter buttons
    int btnX = 288;
    for (int i = 0; i < 6; i++) {
        if (_hFilterBtns[i])
            SetWindowPos(_hFilterBtns[i], nullptr, btnX, 10, 82, 28, SWP_NOZORDER);
        btnX += 86;
    }
}

// ─── OnSize ───────────────────────────────────────────────────────────────────
LRESULT TabDevices::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

// ─── OnPaint (main window chrome) ─────────────────────────────────────────────
LRESULT TabDevices::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);

    // Background
    FillRect(hdc, &rc, Theme::BrushApp());

    // ── Search box container ──────────────────────────────────────────────────
    {
        RECT searchRc;
        if (_hSearch) {
            GetWindowRect(_hSearch, &searchRc);
            MapWindowPoints(HWND_DESKTOP, hwnd, (POINT*)&searchRc, 2);
        } else {
            searchRc = { 14, 10, 278, 40 };
        }
        // Expand by 2px for border
        RECT border = { searchRc.left - 2, searchRc.top - 2,
                        searchRc.right + 2, searchRc.bottom + 2 };

        HBRUSH inputBr = CreateSolidBrush(Theme::BG_INPUT);
        FillRect(hdc, &border, inputBr);
        DeleteObject(inputBr);

        HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, border.left, border.top, border.right, border.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(borderPen);

        // "Search:" prefix label in TEXT_MUTED
        HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontSmall());
        SetTextColor(hdc, Theme::TEXT_MUTED);
        SetBkMode(hdc, TRANSPARENT);
        RECT prefRc = { searchRc.left + 4, searchRc.top,
                        searchRc.left + 60, searchRc.bottom };
        DrawText(hdc, L"\U0001F50D", -1, &prefRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
    }

    // ── Thin separator line below toolbar ─────────────────────────────────────
    {
        HPEN sep = CreatePen(PS_SOLID, 1, Theme::BORDER);
        HPEN old = (HPEN)SelectObject(hdc, sep);
        MoveToEx(hdc, 0, 48, nullptr);
        LineTo(hdc, rc.right, 48);
        SelectObject(hdc, old);
        DeleteObject(sep);
    }

    // ── Detail panel background card ──────────────────────────────────────────
    if (_detailVisible && _hDetailPanel) {
        RECT panelRc;
        GetWindowRect(_hDetailPanel, &panelRc);
        MapWindowPoints(HWND_DESKTOP, hwnd, (POINT*)&panelRc, 2);

        // Card background
        FillRect(hdc, &panelRc, Theme::BrushCard());

        // Left border accent line
        HPEN accentPen = CreatePen(PS_SOLID, 1, Theme::BORDER);
        HPEN old = (HPEN)SelectObject(hdc, accentPen);
        MoveToEx(hdc, panelRc.left, panelRc.top, nullptr);
        LineTo(hdc, panelRc.left, panelRc.bottom);
        SelectObject(hdc, old);
        DeleteObject(accentPen);
    }

    EndPaint(hwnd, &ps);
    return 0;
}

// ─── OnCommand ────────────────────────────────────────────────────────────────
LRESULT TabDevices::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    if (id >= IDC_BTN_FILTER_ALL && id <= IDC_BTN_FILTER_CHANGED) {
        _filterMode = id - IDC_BTN_FILTER_ALL;
        // Redraw all filter buttons
        for (int i = 0; i < 6; i++)
            if (_hFilterBtns[i]) InvalidateRect(_hFilterBtns[i], nullptr, FALSE);
        ApplyFilter();
        return 0;
    }

    if (id == 9500) { // Close detail
        HideDetailPanel();
        return 0;
    }

    if (id == IDC_BTN_DEVICE_SAVE) {
        if (_mainWnd && !_detailDeviceIp.empty()) {
            wchar_t nameBuf[256]  = {};
            wchar_t notesBuf[1024] = {};
            if (_hDetailCustomName) GetWindowText(_hDetailCustomName, nameBuf,  256);
            if (_hDetailNotes)      GetWindowText(_hDetailNotes,      notesBuf, 1024);

            int trustSel = _hDetailTrust
                ? (int)SendMessage(_hDetailTrust, CB_GETCURSEL, 0, 0) : 0;
            static const wchar_t* trustOpts[] =
                { L"unknown", L"owned", L"watchlist", L"guest", L"blocked" };
            wstring trust = (trustSel >= 0 && trustSel < 5)
                ? trustOpts[trustSel] : L"unknown";

            Device* savedDevice = nullptr;
            {
                std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
                for (auto& d : _mainWnd->_lastResult.devices) {
                    if (d.ip == _detailDeviceIp) {
                        d.customName = nameBuf;
                        d.notes      = notesBuf;
                        d.trustState = trust;
                        savedDevice = &d;
                        break;
                    }
                }
            }

            // Save to Firebase if authenticated
            FirebaseClient& fb = GetFirebase();
            if (savedDevice) {
                if (fb.IsAuthenticated()) {
                    if (fb.SaveDevice(*savedDevice)) {
                        // Success - show brief confirmation
                        // (no popup needed, just a visual cue would be nice)
                    } else {
                        MessageBox(hwnd, (L"Cloud save failed: " + fb.GetLastError()).c_str(),
                            L"Firebase", MB_OK | MB_ICONWARNING);
                    }
                } else {
                    // Not logged in - inform user data is only saved locally
                    static bool shownOfflineWarning = false;
                    if (!shownOfflineWarning) {
                        MessageBox(hwnd,
                            L"You are not signed in.\n\n"
                            L"Your changes are saved locally but won't sync to the cloud.\n"
                            L"Sign in to sync your data across devices.",
                            L"Offline Mode", MB_OK | MB_ICONINFORMATION);
                        shownOfflineWarning = true;
                    }
                }
            }

            ApplyFilter();
            HideDetailPanel();
        }
        return 0;
    }

    if (id == IDC_EDIT_SEARCH && HIWORD(wp) == EN_CHANGE) {
        ApplyFilter();
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

// ─── OnNotify ─────────────────────────────────────────────────────────────────
LRESULT TabDevices::OnNotify(HWND hwnd, NMHDR* hdr) {
    if (!hdr) return 0;

    if (hdr->idFrom == IDC_LIST_DEVICES) {
        switch (hdr->code) {

        case NM_RCLICK: {
            NMITEMACTIVATE* nia = reinterpret_cast<NMITEMACTIVATE*>(hdr);
            if (nia->iItem >= 0 && nia->iItem < (int)_filteredIndices.size()) {
                POINT pt; GetCursorPos(&pt);
                ShowDeviceContextMenu(hwnd, pt.x, pt.y, _filteredIndices[nia->iItem]);
            }
            return 0;
        }

        case NM_CLICK:
        case NM_DBLCLK: {
            NMITEMACTIVATE* nm = (NMITEMACTIVATE*)hdr;
            if (nm->iItem >= 0 && nm->iItem < (int)_filteredIndices.size()) {
                _selectedDevice = _filteredIndices[nm->iItem];
                ShowDetailPanel(_selectedDevice);
            }
            break;
        }

        case LVN_COLUMNCLICK: {
            NMLISTVIEW* nm = (NMLISTVIEW*)hdr;
            if (nm->iSubItem == _sortCol) _sortAsc = !_sortAsc;
            else { _sortCol = nm->iSubItem; _sortAsc = true; }
            ApplyFilter();
            break;
        }

        // LVN_GETDISPINFO — feed text to virtual list
        case LVN_GETDISPINFO: {
            NMLVDISPINFO* pdi = (NMLVDISPINFO*)hdr;
            int row = pdi->item.iItem;
            int col = pdi->item.iSubItem;
            if (!(pdi->item.mask & LVIF_TEXT)) break;
            if (row < 0 || row >= (int)_filteredIndices.size()) break;

            if (!_mainWnd) break;
            ScanResult r = _mainWnd->GetLastResult();
            int idx = _filteredIndices[row];
            if (idx < 0 || idx >= (int)r.devices.size()) break;
            const Device& d = r.devices[idx];

            static wchar_t dispBuf[512];
            dispBuf[0] = L'\0';

            switch (col) {
            case 0:
                // Status column — text is empty; dot drawn in custom draw
                dispBuf[0] = L'\0';
                break;
            case 1: {
                wstring name = d.customName.empty() ? d.hostname : d.customName;
                if (name.empty()) name = d.ip;
                wcsncpy_s(dispBuf, name.c_str(), 511);
                break;
            }
            case 2: {
                wstring ip = d.ip;
                if (!d.ipv6Address.empty()) ip += L" [v6]";
                wcsncpy_s(dispBuf, ip.c_str(), 511);
                break;
            }
            case 3:
                wcsncpy_s(dispBuf, d.mac.c_str(), 511);
                break;
            case 4:
                wcsncpy_s(dispBuf, d.vendor.c_str(), 511);
                break;
            case 5: {
                wstring tc = d.deviceType + L" (" + std::to_wstring(d.confidence) + L"%)";
                wcsncpy_s(dispBuf, tc.c_str(), 511);
                break;
            }
            case 6:
                // Trust — text is empty; pill drawn in custom draw
                dispBuf[0] = L'\0';
                break;
            case 7: {
                wstring ports = GetPortSummary(d);
                wcsncpy_s(dispBuf, ports.c_str(), 511);
                break;
            }
            case 8: {
                wstring seen = std::to_wstring(d.sightingCount);
                wcsncpy_s(dispBuf, seen.c_str(), 511);
                break;
            }
            case 9:
                wcsncpy_s(dispBuf, d.lastSeen.c_str(), 511);
                break;
            }

            pdi->item.pszText = dispBuf;
            return 0;
        }

        case NM_CUSTOMDRAW: {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)hdr;
            return HandleListCustomDraw(cd);
        }

        } // switch hdr->code
    } // if IDC_LIST_DEVICES

    // Header custom draw
    HWND hHeader = _hList ? ListView_GetHeader(_hList) : nullptr;
    if (hHeader && hdr->hwndFrom == hHeader && hdr->code == NM_CUSTOMDRAW) {
        NMCUSTOMDRAW* hcd = (NMCUSTOMDRAW*)hdr;
        if (hcd->dwDrawStage == CDDS_PREPAINT)
            return CDRF_NOTIFYITEMDRAW;
        if (hcd->dwDrawStage == CDDS_ITEMPREPAINT) {
            HDC hdc = hcd->hdc;
            RECT rc  = hcd->rc;

            // Header cell background
            HBRUSH bg = CreateSolidBrush(Theme::BG_ELEVATED);
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            // Right-edge separator
            HPEN sep = CreatePen(PS_SOLID, 1, Theme::BORDER);
            HPEN old = (HPEN)SelectObject(hdc, sep);
            MoveToEx(hdc, rc.right - 1, rc.top + 3, nullptr);
            LineTo(hdc,   rc.right - 1, rc.bottom - 3);
            SelectObject(hdc, old);
            DeleteObject(sep);

            // Bottom line
            HPEN bot = CreatePen(PS_SOLID, 1, Theme::BORDER);
            old = (HPEN)SelectObject(hdc, bot);
            MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
            LineTo(hdc,   rc.right, rc.bottom - 1);
            SelectObject(hdc, old);
            DeleteObject(bot);

            // Header text
            wchar_t colText[128] = {};
            HDITEM hdi = {};
            hdi.mask      = HDI_TEXT;
            hdi.pszText   = colText;
            hdi.cchTextMax = 127;
            Header_GetItem(hHeader, (int)hcd->dwItemSpec, &hdi);

            SetTextColor(hdc, Theme::TEXT_MUTED);
            SetBkMode(hdc, TRANSPARENT);
            HFONT oldf = (HFONT)SelectObject(hdc, Theme::FontSmall());
            RECT textRc = { rc.left + 6, rc.top, rc.right - 2, rc.bottom };
            DrawText(hdc, colText, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldf);

            return CDRF_SKIPDEFAULT;
        }
        return CDRF_DODEFAULT;
    }

    return CDRF_DODEFAULT;
}

// ─── HandleListCustomDraw ─────────────────────────────────────────────────────
LRESULT TabDevices::HandleListCustomDraw(NMLVCUSTOMDRAW* cd) {
    switch (cd->nmcd.dwDrawStage) {

    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT: {
        int row = (int)cd->nmcd.dwItemSpec;
        bool sel = (ListView_GetItemState(_hList, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;

        COLORREF bg;
        if (sel)
            bg = Theme::BG_ROW_SEL;
        else if (row % 2 == 1)
            bg = Theme::BG_ROW_ALT;
        else
            bg = Theme::BG_APP;

        cd->clrTextBk = bg;
        cd->clrText   = Theme::TEXT_PRIMARY;
        return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
    }

    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        int row = (int)cd->nmcd.dwItemSpec;
        int col = cd->iSubItem;

        if (!_mainWnd || row >= (int)_filteredIndices.size())
            return CDRF_DODEFAULT;

        ScanResult r = _mainWnd->GetLastResult();
        int idx = _filteredIndices[row];
        if (idx < 0 || idx >= (int)r.devices.size())
            return CDRF_DODEFAULT;

        const Device& d = r.devices[idx];
        bool sel = (ListView_GetItemState(_hList, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;

        COLORREF rowBg;
        if (sel)
            rowBg = Theme::BG_ROW_SEL;
        else if (row % 2 == 1)
            rowBg = Theme::BG_ROW_ALT;
        else
            rowBg = Theme::BG_APP;

        HDC hdc  = cd->nmcd.hdc;
        RECT rc  = cd->nmcd.rc;

        // ── Col 0: Status dot ─────────────────────────────────────────────────
        if (col == 0) {
            HBRUSH bg = CreateSolidBrush(rowBg);
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            // Selected row: accent left border stripe
            if (sel) {
                HBRUSH stripe = CreateSolidBrush(Theme::ACCENT);
                RECT stripeRc = { rc.left, rc.top, rc.left + 3, rc.bottom };
                FillRect(hdc, &stripeRc, stripe);
                DeleteObject(stripe);
            }

            COLORREF dotColor = d.online ? Theme::SUCCESS : Theme::DANGER;
            DrawStatusDot(hdc, rc, dotColor);
            return CDRF_SKIPDEFAULT;
        }

        // ── Col 2: IP — monospace ─────────────────────────────────────────────
        if (col == 2) {
            HBRUSH bg = CreateSolidBrush(rowBg);
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            wstring ip = d.ip;
            if (!d.ipv6Address.empty()) ip += L" [v6]";

            SelectObject(hdc, Theme::FontMono());
            SetTextColor(hdc, Theme::ACCENT_BRIGHT);
            SetBkMode(hdc, TRANSPARENT);
            RECT textRc = { rc.left + 4, rc.top, rc.right - 2, rc.bottom };
            DrawText(hdc, ip.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            return CDRF_SKIPDEFAULT;
        }

        // ── Col 3: MAC — monospace ────────────────────────────────────────────
        if (col == 3) {
            HBRUSH bg = CreateSolidBrush(rowBg);
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            SelectObject(hdc, Theme::FontMono());
            SetTextColor(hdc, Theme::TEXT_SECONDARY);
            SetBkMode(hdc, TRANSPARENT);
            RECT textRc = { rc.left + 4, rc.top, rc.right - 2, rc.bottom };
            DrawText(hdc, d.mac.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            return CDRF_SKIPDEFAULT;
        }

        // ── Col 1: Name — bold name + small vendor hint ───────────────────────
        if (col == 1) {
            HBRUSH bg = CreateSolidBrush(rowBg);
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            wstring name = d.customName.empty() ? d.hostname : d.customName;
            if (name.empty()) name = d.ip;

            SetBkMode(hdc, TRANSPARENT);

            // Bold device name (top half)
            HFONT oldf = (HFONT)SelectObject(hdc, Theme::FontBold());
            SetTextColor(hdc, Theme::TEXT_PRIMARY);
            int midY = rc.top + (rc.bottom - rc.top) / 2;
            RECT nameRc = { rc.left + 4, rc.top + 1, rc.right - 2, midY };
            DrawText(hdc, name.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Small vendor (bottom half)
            if (!d.vendor.empty()) {
                SelectObject(hdc, Theme::FontSmall());
                SetTextColor(hdc, Theme::TEXT_MUTED);
                RECT vendRc = { rc.left + 4, midY, rc.right - 2, rc.bottom - 1 };
                DrawText(hdc, d.vendor.c_str(), -1, &vendRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            SelectObject(hdc, oldf);
            return CDRF_SKIPDEFAULT;
        }

        // ── Col 6: Trust — colored pill ───────────────────────────────────────
        if (col == 6) {
            HBRUSH bg = CreateSolidBrush(rowBg);
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            COLORREF pillBg, pillFg;
            GetTrustColors(d.trustState, pillBg, pillFg);
            DrawPill(hdc, rc, d.trustState.c_str(), pillBg, pillFg,
                     Theme::FontSmall(), false);
            return CDRF_SKIPDEFAULT;
        }

        // ── Col 7: Ports — with risky port highlighting ───────────────────────
        if (col == 7) {
            HBRUSH bg = CreateSolidBrush(rowBg);
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            SetBkMode(hdc, TRANSPARENT);
            HFONT oldf = (HFONT)SelectObject(hdc, Theme::FontSmall());

            if (d.openPorts.empty()) {
                SetTextColor(hdc, Theme::TEXT_MUTED);
                RECT textRc = { rc.left + 4, rc.top, rc.right, rc.bottom };
                DrawText(hdc, L"None", -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            } else {
                int x = rc.left + 4;
                int count = std::min((int)d.openPorts.size(), 4);
                for (int pi = 0; pi < count && x < rc.right - 20; pi++) {
                    int port = d.openPorts[pi];
                    wchar_t portStr[16];
                    wsprintf(portStr, L"%d", port);

                    SIZE sz = {};
                    GetTextExtentPoint32(hdc, portStr, (int)wcslen(portStr), &sz);

                    int badgeW = sz.cx + 8;
                    int badgeH = sz.cy + 2;
                    int by = rc.top + (rc.bottom - rc.top - badgeH) / 2;

                    RECT badgeRc = { x, by, x + badgeW, by + badgeH };

                    COLORREF badgeBg = IsRiskyPort(port) ? Theme::DANGER    : Theme::BG_ELEVATED;
                    COLORREF badgeFg = IsRiskyPort(port) ? RGB(255,255,255) : Theme::TEXT_SECONDARY;

                    HBRUSH bbr = CreateSolidBrush(badgeBg);
                    FillRoundRect(hdc, badgeRc, badgeH, bbr);
                    DeleteObject(bbr);

                    SetTextColor(hdc, badgeFg);
                    DrawText(hdc, portStr, -1, &badgeRc,
                             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    x += badgeW + 3;
                }
                if ((int)d.openPorts.size() > 4) {
                    wchar_t more[16];
                    wsprintf(more, L"+%d", (int)d.openPorts.size() - 4);
                    SetTextColor(hdc, Theme::TEXT_MUTED);
                    RECT moreRc = { x, rc.top, rc.right, rc.bottom };
                    DrawText(hdc, more, -1, &moreRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
            }

            SelectObject(hdc, oldf);
            return CDRF_SKIPDEFAULT;
        }

        // Default: use row background, primary text
        cd->clrTextBk = rowBg;
        cd->clrText   = Theme::TEXT_PRIMARY;
        return CDRF_NEWFONT;
    }

    default:
        return CDRF_DODEFAULT;
    }
}

// ─── OnScanComplete ───────────────────────────────────────────────────────────
LRESULT TabDevices::OnScanComplete(HWND hwnd) {
    ApplyFilter();
    return 0;
}

// ─── ApplyFilter ──────────────────────────────────────────────────────────────
void TabDevices::ApplyFilter() {
    if (!_mainWnd || !_hList) return;

    ScanResult r = _mainWnd->GetLastResult();

    wchar_t searchBuf[256] = {};
    if (_hSearch) GetWindowText(_hSearch, searchBuf, 256);
    wstring search = searchBuf;
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    _filteredIndices.clear();
    for (int i = 0; i < (int)r.devices.size(); i++) {
        const Device& d = r.devices[i];

        switch (_filterMode) {
        case 1: if (!d.online)                    continue; break;
        case 2: if (d.trustState != L"unknown")   continue; break;
        case 3: if (d.trustState != L"watchlist") continue; break;
        case 4: if (d.trustState != L"owned")     continue; break;
        case 5: if (d.prevPorts == d.openPorts)   continue; break;
        }

        if (!search.empty()) {
            wstring ip = d.ip, mac = d.mac, name = d.hostname, vendor = d.vendor;
            std::transform(ip.begin(),     ip.end(),     ip.begin(),     ::tolower);
            std::transform(mac.begin(),    mac.end(),    mac.begin(),    ::tolower);
            std::transform(name.begin(),   name.end(),   name.begin(),   ::tolower);
            std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);

            if (ip.find(search)     == wstring::npos &&
                mac.find(search)    == wstring::npos &&
                name.find(search)   == wstring::npos &&
                vendor.find(search) == wstring::npos)
                continue;
        }

        _filteredIndices.push_back(i);
    }

    UpdateFilterCounts();

    // Virtual list — just set count and redraw
    ListView_SetItemCountEx(_hList, (int)_filteredIndices.size(),
                            LVSICF_NOINVALIDATEALL);
    InvalidateRect(_hList, nullptr, FALSE);

    // Redraw filter buttons (counts changed)
    for (int i = 0; i < 6; i++)
        if (_hFilterBtns[i]) InvalidateRect(_hFilterBtns[i], nullptr, FALSE);
}

// ─── PopulateList (kept for compatibility — virtual list doesn't need this) ──
void TabDevices::PopulateList() {
    if (!_hList) return;
    UpdateFilterCounts();
    ListView_SetItemCountEx(_hList, (int)_filteredIndices.size(),
                            LVSICF_NOINVALIDATEALL);
    InvalidateRect(_hList, nullptr, FALSE);
}

// ─── GetPortSummary ───────────────────────────────────────────────────────────
wstring TabDevices::GetPortSummary(const Device& dev) {
    if (dev.openPorts.empty()) return L"None";

    wstring s;
    int count = std::min((int)dev.openPorts.size(), 4);
    for (int i = 0; i < count; i++) {
        int port = dev.openPorts[i];
        auto it = ScanEngine::PORT_NAMES.find(port);
        if (it != ScanEngine::PORT_NAMES.end())
            s += std::to_wstring(port) + L"(" + it->second + L") ";
        else
            s += std::to_wstring(port) + L" ";
    }
    if ((int)dev.openPorts.size() > 4)
        s += L"+" + std::to_wstring(dev.openPorts.size() - 4) + L" more";

    return s;
}

// ─── ShowDetailPanel ──────────────────────────────────────────────────────────
void TabDevices::ShowDetailPanel(int idx) {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();
    if (idx < 0 || idx >= (int)r.devices.size()) return;

    _detailVisible = true;
    UpdateDetailPanel(r.devices[idx]);

    RECT rc; GetClientRect(_hwnd, &rc);
    LayoutControls(rc.right, rc.bottom);
    ShowWindow(_hDetailPanel, SW_SHOW);
    InvalidateRect(_hwnd, nullptr, FALSE);
}

// ─── HideDetailPanel ─────────────────────────────────────────────────────────
void TabDevices::HideDetailPanel() {
    _detailVisible = false;
    ShowWindow(_hDetailPanel, SW_HIDE);

    RECT rc; GetClientRect(_hwnd, &rc);
    LayoutControls(rc.right, rc.bottom);
    InvalidateRect(_hwnd, nullptr, FALSE);
}

// ─── UpdateDetailPanel ───────────────────────────────────────────────────────
void TabDevices::UpdateDetailPanel(const Device& dev) {
    if (!_hDetailPanel) return;

    // Custom name
    if (_hDetailCustomName)
        SetWindowText(_hDetailCustomName, dev.customName.c_str());

    // Display name
    wstring displayName = dev.customName.empty() ? dev.hostname : dev.customName;
    if (displayName.empty()) displayName = dev.ip;
    wstring nameSrc;
    for (auto& e : dev.evidence) {
        if (e.field == L"hostname") { nameSrc = e.source; break; }
    }
    if (!nameSrc.empty() && dev.customName.empty())
        displayName += L"  [" + nameSrc + L"]";
    if (_hDetailName) SetWindowText(_hDetailName, displayName.c_str());

    // Type + confidence
    if (_hDetailType)
        SetWindowText(_hDetailType,
            (dev.deviceType + L"   " + std::to_wstring(dev.confidence) + L"% confidence").c_str());

    // Store confidence for confidence bar
    _detailConfidence = dev.confidence;
    if (_hDetailConfBar)
        InvalidateRect(_hDetailConfBar, nullptr, TRUE);

    // Classification evidence
    if (_hDetailEvidence) {
        wstring ev = dev.classificationReason;
        if (ev.empty()) ev = L"No evidence — run a deeper scan";
        SetWindowText(_hDetailEvidence, (L"Evidence: " + ev).c_str());
    }

    // Confidence alternatives
    wstring altStr;
    if (!dev.altType1.empty())
        altStr += dev.altType1 + L" (" + std::to_wstring(dev.altConf1) + L"%)";
    if (!dev.altType2.empty())
        altStr += L"  |  " + dev.altType2 + L" (" + std::to_wstring(dev.altConf2) + L"%)";
    if (altStr.empty()) altStr = L"No alternatives — run Deep scan";
    if (_hDetailAlt) SetWindowText(_hDetailAlt, (L"Alt: " + altStr).c_str());

    // Vendor
    if (_hDetailVendor) {
        wstring vendorStr = dev.vendor.empty() ? L"Unknown" : dev.vendor;
        for (auto& e : dev.evidence) {
            if (e.field == L"vendor") { vendorStr += L"  [" + e.source + L"]"; break; }
        }
        SetWindowText(_hDetailVendor, vendorStr.c_str());
    }

    // MAC + latency (mono)
    if (_hDetailMac) {
        wstring macLine = dev.ip + L"   " + dev.mac;
        if (dev.latencyMs >= 0)
            macLine += L"   " + std::to_wstring(dev.latencyMs) + L"ms";
        SetWindowText(_hDetailMac, macLine.c_str());
    }

    // Subnet
    if (_hDetailSubnet) {
        wstring sub = dev.subnet.empty() ? L"Subnet: unknown" : L"Subnet: " + dev.subnet;
        SetWindowText(_hDetailSubnet, sub.c_str());
    }

    // Timestamps
    if (_hDetailFirstSeen)
        SetWindowText(_hDetailFirstSeen,
            (L"First: " + (dev.firstSeen.empty() ? L"this scan" : dev.firstSeen)).c_str());
    if (_hDetailLastSeen)
        SetWindowText(_hDetailLastSeen, (L"Last:  " + dev.lastSeen).c_str());
    if (_hDetailSightings)
        SetWindowText(_hDetailSightings,
            (L"Sightings: " + std::to_wstring(dev.sightingCount) + L" scan(s)").c_str());

    // IP history
    if (_hDetailIpHistory) {
        if (dev.ipHistory.empty()) {
            SetWindowText(_hDetailIpHistory, L"No prior IPs");
        } else {
            wstring hist = L"Prior IPs: ";
            for (size_t i = 0; i < dev.ipHistory.size(); i++) {
                if (i > 0) hist += L", ";
                hist += dev.ipHistory[i];
            }
            SetWindowText(_hDetailIpHistory, hist.c_str());
        }
    }

    // Ports
    wstring portStr;
    if (dev.openPorts.empty()) portStr = L"No open ports";
    else for (int p : dev.openPorts) {
        auto it = ScanEngine::PORT_NAMES.find(p);
        if (IsRiskyPort(p)) portStr += L"\u26A0 ";
        portStr += std::to_wstring(p);
        if (it != ScanEngine::PORT_NAMES.end()) portStr += L"/" + it->second;
        portStr += L"  ";
    }
    if (_hDetailPorts) SetWindowText(_hDetailPorts, portStr.c_str());

    // mDNS
    wstring mdns;
    for (auto& s : dev.mdnsServices) mdns += s + L"  ";
    if (mdns.empty()) mdns = L"No mDNS services";
    else mdns = L"mDNS: " + mdns;
    if (_hDetailMdns) SetWindowText(_hDetailMdns, mdns.c_str());

    // IoT risk
    if (_hDetailIotRisk) {
        if (dev.iotRisk && !dev.iotRiskDetail.empty()) {
            SetWindowText(_hDetailIotRisk, dev.iotRiskDetail.c_str());
            ShowWindow(_hDetailIotRisk, SW_SHOW);
        } else {
            ShowWindow(_hDetailIotRisk, SW_HIDE);
        }
    }

    // Anomalies
    wstring anoms;
    if (_mainWnd) {
        ScanResult r = _mainWnd->GetLastResult();
        for (auto& a : r.anomalies) {
            if (a.deviceIp == dev.ip)
                anoms += L"[" + a.severity + L"] " + a.description + L"\r\n";
        }
    }
    if (anoms.empty()) anoms = L"No alerts for this device";
    if (_hDetailAnoms) SetWindowText(_hDetailAnoms, anoms.c_str());

    // Notes
    if (_hDetailNotes) SetWindowText(_hDetailNotes, dev.notes.c_str());

    // Trust dropdown
    if (_hDetailTrust) {
        static const wchar_t* trustOpts[] =
            { L"unknown", L"owned", L"watchlist", L"guest", L"blocked" };
        for (int i = 0; i < 5; i++) {
            if (dev.trustState == trustOpts[i]) {
                SendMessage(_hDetailTrust, CB_SETCURSEL, i, 0);
                break;
            }
        }
    }

    _detailDeviceIp = dev.ip;
}

// ─── RefreshList ─────────────────────────────────────────────────────────────
void TabDevices::RefreshList() {
    ApplyFilter();
}

// ─── ShowDeviceContextMenu ────────────────────────────────────────────────────
void TabDevices::ShowDeviceContextMenu(HWND hwnd, int x, int y, int deviceIdx) {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();
    if (deviceIdx < 0 || deviceIdx >= (int)r.devices.size()) return;
    const Device& dev = r.devices[deviceIdx];

    HMENU hMenu = CreatePopupMenu();

    AppendMenu(hMenu, MF_STRING,    12001, L"Ping Device");
    AppendMenu(hMenu, MF_STRING,    12002, L"Traceroute");
    AppendMenu(hMenu, MF_STRING,    12003, L"Port Scan");
    AppendMenu(hMenu, MF_STRING,    12004, L"Copy IP Address");
    AppendMenu(hMenu, MF_STRING,    12005, L"Copy Device Summary (JSON)");
    AppendMenu(hMenu, MF_SEPARATOR, 0,     nullptr);

    AppendMenu(hMenu, MF_STRING,    12010, L"Mark as Owned");
    AppendMenu(hMenu, MF_STRING,    12011, L"Mark as Guest");
    AppendMenu(hMenu, MF_STRING,    12012, L"Add to Watchlist");
    AppendMenu(hMenu, MF_STRING,    12013, L"Block Device");
    AppendMenu(hMenu, MF_SEPARATOR, 0,     nullptr);

    bool hasWeb = false, hasSsh = false, hasRdp = false, hasFtp = false, hasSamba = false;
    for (int p : dev.openPorts) {
        if (p == 80 || p == 443 || p == 8080 || p == 8443) hasWeb = true;
        if (p == 22)  hasSsh   = true;
        if (p == 3389) hasRdp  = true;
        if (p == 21)  hasFtp   = true;
        if (p == 445 || p == 139) hasSamba = true;
    }

    if (hasWeb)   AppendMenu(hMenu, MF_STRING, 12020, L"Open in Browser");
    if (hasSsh)   AppendMenu(hMenu, MF_STRING, 12021, L"Connect via SSH");
    if (hasRdp)   AppendMenu(hMenu, MF_STRING, 12022, L"Remote Desktop (RDP)");
    if (hasFtp)   AppendMenu(hMenu, MF_STRING, 12023, L"Open FTP Connection");
    if (hasSamba) AppendMenu(hMenu, MF_STRING, 12024, L"Browse Network Share");

    if (hasWeb || hasSsh || hasRdp || hasFtp || hasSamba)
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);

    if (!dev.online && !dev.mac.empty())
        AppendMenu(hMenu, MF_STRING, 12030, L"Wake-on-LAN");

    AppendMenu(hMenu, MF_STRING, 12031, L"Reverse DNS Lookup");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, 12040, L"View Details");

    _detailDeviceIp = dev.ip;

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 0) return;

    switch (cmd) {
    case 12001: { wstring c = L"cmd /c start cmd /k ping " + dev.ip;       _wsystem(c.c_str()); break; }
    case 12002: { wstring c = L"cmd /c start cmd /k tracert " + dev.ip;    _wsystem(c.c_str()); break; }
    case 12003: { if (_mainWnd) _mainWnd->SwitchTab(Tab::Tools); break; }
    case 12004: {
        if (!CopyToClipboard(hwnd, dev.ip))
            MessageBox(hwnd, L"Could not copy IP to clipboard.", L"Clipboard", MB_OK | MB_ICONWARNING);
        break;
    }
    case 12005: {
        if (!CopyToClipboard(hwnd, BuildDeviceSummaryJson(dev)))
            MessageBox(hwnd, L"Could not copy device summary to clipboard.", L"Clipboard", MB_OK | MB_ICONWARNING);
        break;
    }
    case 12010: {
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices)
            if (d.ip == dev.ip) { d.trustState = L"owned"; break; }
        ApplyFilter(); break;
    }
    case 12011: {
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices)
            if (d.ip == dev.ip) { d.trustState = L"guest"; break; }
        ApplyFilter(); break;
    }
    case 12012: {
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices)
            if (d.ip == dev.ip) { d.trustState = L"watchlist"; break; }
        ApplyFilter(); break;
    }
    case 12013: {
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices)
            if (d.ip == dev.ip) { d.trustState = L"blocked"; break; }
        ApplyFilter(); break;
    }
    case 12020: {
        wstring url = L"http://" + dev.ip;
        for (int p : dev.openPorts) {
            if (p == 443 || p == 8443) { url = L"https://" + dev.ip; break; }
            if (p == 8080) { url = L"http://" + dev.ip + L":8080"; break; }
        }
        ShellExecute(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);
        break;
    }
    case 12021: { wstring c = L"cmd /c start cmd /k ssh " + dev.ip; _wsystem(c.c_str()); break; }
    case 12022: { ShellExecute(nullptr, L"open", L"mstsc.exe", (L"/v:" + dev.ip).c_str(), nullptr, SW_SHOW); break; }
    case 12023: { ShellExecute(nullptr, L"open", (L"ftp://" + dev.ip).c_str(), nullptr, nullptr, SW_SHOW); break; }
    case 12024: { ShellExecute(nullptr, L"open", (L"\\\\" + dev.ip).c_str(), nullptr, nullptr, SW_SHOW); break; }
    case 12030: { MessageBox(hwnd, (L"Wake-on-LAN sent to " + dev.mac).c_str(), L"WOL", MB_OK | MB_ICONINFORMATION); break; }
    case 12031: { wstring c = L"cmd /c start cmd /k nslookup " + dev.ip; _wsystem(c.c_str()); break; }
    case 12040: {
        for (int i = 0; i < (int)_filteredIndices.size(); i++) {
            if (_filteredIndices[i] == deviceIdx) {
                ShowDetailPanel(i);
                break;
            }
        }
        break;
    }
    }
}
