#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>

#include "TabGlossary.h"
#include "MainWindow.h"
#include "GlossaryPopup.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

const wchar_t* TabGlossary::s_className = L"TabGlossaryWnd";

// Terms to display — mDNS and UPnP first per user request, then alphabetical.
static const wchar_t* TERMS[] = {
    L"mDNS", L"UPnP",
    L"APIPA", L"ARP",  L"BSSID", L"CIDR",  L"DEEP",
    L"DHCP",  L"DNS",  L"GATEWAY", L"GENTLE", L"IANA",
    L"ICMP",  L"IP",   L"IPv6",  L"ISP",   L"MAC",
    L"MONITOR", L"NAT", L"NDP",  L"NetBIOS", L"NIC",
    L"OUI",   L"PING", L"QUICK", L"RFC",   L"SSID",
    L"SSDP",  L"STANDARD", L"SUBNET", L"TCP", L"UDP", L"WOL",
};
static const int TERM_COUNT = (int)(sizeof(TERMS) / sizeof(TERMS[0]));

// ── Create ────────────────────────────────────────────────────────────────────

bool TabGlossary::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
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

// ── WndProc ───────────────────────────────────────────────────────────────────

LRESULT CALLBACK TabGlossary::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabGlossary* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabGlossary*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabGlossary*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:     return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:       self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:      return self->OnPaint(hwnd);
    case WM_ERASEBKGND: return 1;
    case WM_NOTIFY:     return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        SetTextColor((HDC)wp, Theme::TEXT_PRIMARY);
        SetBkColor((HDC)wp, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

// ── OnCreate / OnSize / OnPaint ───────────────────────────────────────────────

LRESULT TabGlossary::OnCreate(HWND hwnd, LPCREATESTRUCT) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    return 0;
}

LRESULT TabGlossary::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabGlossary::OnPaint(HWND hwnd) {
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

    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontH2());
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    RECT titleRc = { 16, 12, cx - 16, 38 };
    DrawText(hdc, L"Glossary", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, Theme::FontBodySm());
    SetTextColor(hdc, Theme::TEXT_SECONDARY);
    RECT subRc = { 16, 38, cx - 16, 54 };
    DrawText(hdc, L"Network terminology reference", -1, &subRc,
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

// ── Controls ──────────────────────────────────────────────────────────────────

static const int LIST_TOP    = 66;
static const int DETAIL_H    = 110;
static const int DETAIL_GAP  = 8;

void TabGlossary::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    int listH   = cy - LIST_TOP - DETAIL_GAP - DETAIL_H - 8;
    if (listH < 40) listH = 40;
    int detailY = LIST_TOP + listH + DETAIL_GAP;

    _hList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL |
        LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        16, LIST_TOP, cx - 32, listH,
        hwnd, (HMENU)(INT_PTR)IDC_LIST_GLOSSARY, hInst, nullptr);
    SendMessage(_hList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    Theme::ApplyDarkScrollbar(_hList);

    LVCOLUMN col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
    col.cx = 110; col.pszText = (LPWSTR)L"Term";
    ListView_InsertColumn(_hList, 0, &col);
    col.cx = cx - 32 - 110 - 4; col.pszText = (LPWSTR)L"Definition";
    ListView_InsertColumn(_hList, 1, &col);

    _hDetail = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
        16, detailY, cx - 32, DETAIL_H,
        hwnd, nullptr, hInst, nullptr);
    SendMessage(_hDetail, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(_hDetail);
    SetWindowText(_hDetail, L"Select a term above to read its full definition.");

    PopulateList();
}

void TabGlossary::LayoutControls(int cx, int cy) {
    if (!_hList) return;
    int listH   = cy - LIST_TOP - DETAIL_GAP - DETAIL_H - 8;
    if (listH < 40) listH = 40;
    int detailY = LIST_TOP + listH + DETAIL_GAP;

    SetWindowPos(_hList,   nullptr, 16, LIST_TOP, cx - 32, listH,   SWP_NOZORDER);
    SetWindowPos(_hDetail, nullptr, 16, detailY,  cx - 32, DETAIL_H, SWP_NOZORDER);

    // Resize "Definition" column to fill available width
    ListView_SetColumnWidth(_hList, 1, cx - 32 - 110 - 4);
}

// ── PopulateList ──────────────────────────────────────────────────────────────

void TabGlossary::PopulateList() {
    if (!_hList) return;
    ListView_DeleteAllItems(_hList);

    for (int i = 0; i < TERM_COUNT; i++) {
        const wchar_t* key = TERMS[i];
        const wchar_t* def = LookupAcronym(key);
        if (!def) continue;

        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = i;
        lvi.pszText = (LPWSTR)key;
        ListView_InsertItem(_hList, &lvi);
        ListView_SetItemText(_hList, i, 1, (LPWSTR)def);
    }
}

// ── Notify — row selection → show full definition ─────────────────────────────

LRESULT TabGlossary::OnNotify(HWND hwnd, NMHDR* hdr) {
    if (!hdr) return 0;

    if (hdr->idFrom == IDC_LIST_GLOSSARY) {
        if (hdr->code == NM_CLICK || hdr->code == NM_DBLCLK) {
            auto* nm = reinterpret_cast<NMITEMACTIVATE*>(hdr);
            if (nm->iItem >= 0 && nm->iItem < TERM_COUNT) {
                const wchar_t* def = LookupAcronym(TERMS[nm->iItem]);
                if (_hDetail && def)
                    SetWindowText(_hDetail, def);
            }
            return 0;
        }

        if (hdr->code == NM_CUSTOMDRAW) {
            auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(hdr);
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: {
                bool sel = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
                cd->clrTextBk = sel ? Theme::BG_ROW_SEL
                              : ((int)cd->nmcd.dwItemSpec % 2 == 1) ? Theme::BG_ROW_ALT
                              : Theme::BG_SURFACE;
                cd->clrText = Theme::TEXT_PRIMARY;
                return CDRF_NEWFONT;
            }
            default:
                return CDRF_DODEFAULT;
            }
        }
    }

    return CDRF_DODEFAULT;
}
