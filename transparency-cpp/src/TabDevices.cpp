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

#include "TabDevices.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"
#include "Scanner.h"

using std::wstring;

const wchar_t* TabDevices::s_className = L"TransparencyTabDevices";

static const wchar_t* FILTER_LABELS[] = {
    L"All", L"Online", L"Unknown", L"Watchlist", L"Owned", L"Changed"
};

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
        WS_CHILD | WS_CLIPCHILDREN,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);

    return _hwnd != nullptr;
}

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
    case WM_CREATE:     return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:       self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:      return self->OnPaint(hwnd);
    case WM_ERASEBKGND: { RECT rc; GetClientRect(hwnd,&rc); FillRect((HDC)wp,&rc,Theme::BrushApp()); return 1; }
    case WM_COMMAND:    return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY:     return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_APP);
        return (LRESULT)Theme::BrushApp();
    }
    case WM_SCAN_COMPLETE: return self->OnScanComplete(hwnd);
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabDevices::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    return 0;
}

void TabDevices::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Search box
    _hSearch = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        16, 12, 260, 26, hwnd, (HMENU)IDC_EDIT_SEARCH, hInst, nullptr);
    SendMessage(_hSearch, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hSearch, EM_SETCUEBANNER, FALSE, (LPARAM)L"Search devices...");
    Theme::ApplyDarkEdit(_hSearch);

    // Filter buttons
    int btnX = 290;
    for (int i = 0; i < 6; i++) {
        _hFilterBtns[i] = CreateWindowEx(0, L"BUTTON", FILTER_LABELS[i],
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            btnX, 12, 72, 26, hwnd, (HMENU)(IDC_BTN_FILTER_ALL + i), hInst, nullptr);
        SendMessage(_hFilterBtns[i], WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
        btnX += 76;
    }

    // List view
    int listW = _detailVisible ? cx - DETAIL_WIDTH - 32 : cx - 32;
    _hList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS |
        LVS_SINGLESEL | LVS_NOSORTHEADER | WS_VSCROLL | WS_HSCROLL,
        16, 48, listW, cy - 64,
        hwnd, (HMENU)IDC_LIST_DEVICES, hInst, nullptr);

    SendMessage(_hList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);

    Theme::ApplyDarkScrollbar(_hList);

    // Columns
    struct ColDef { const wchar_t* name; int width; int fmt; };
    static const ColDef COLS[] = {
        { L"",          16,  LVCFMT_CENTER }, // Status dot
        { L"Name",      200, LVCFMT_LEFT   },
        { L"IP Address",140, LVCFMT_LEFT   },
        { L"MAC",       150, LVCFMT_LEFT   },
        { L"Vendor",    130, LVCFMT_LEFT   },
        { L"Type",      120, LVCFMT_LEFT   },
        { L"Trust",      90, LVCFMT_LEFT   },
        { L"Open Ports",140, LVCFMT_LEFT   },
        { L"Last Seen", 120, LVCFMT_LEFT   },
    };

    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    for (int i = 0; i < (int)(sizeof(COLS)/sizeof(COLS[0])); i++) {
        col.cx = COLS[i].width;
        col.pszText = (LPWSTR)COLS[i].name;
        col.fmt = COLS[i].fmt;
        ListView_InsertColumn(_hList, i, &col);
    }

    // Detail panel (hidden by default)
    _hDetailPanel = CreateWindowEx(WS_EX_STATICEDGE, L"STATIC", nullptr,
        WS_CHILD | SS_NOTIFY,
        cx - DETAIL_WIDTH - 16, 48, DETAIL_WIDTH, cy - 64,
        hwnd, nullptr, hInst, nullptr);

    // Detail controls inside panel
    auto makeLbl = [&](const wchar_t* text, int y, int h = 20) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            8, y, DETAIL_WIDTH - 16, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };

    _hDetailName    = makeLbl(L"", 8, 22);
    _hDetailType    = makeLbl(L"", 32, 18);
    _hDetailVendor  = makeLbl(L"", 52, 18);
    _hDetailMac     = makeLbl(L"", 72, 18);
    _hDetailPorts   = makeLbl(L"", 96, 36);
    _hDetailLastSeen= makeLbl(L"", 136, 18);
    _hDetailMdns    = makeLbl(L"", 158, 36);
    _hDetailAnoms   = makeLbl(L"", 198, 54);

    // Notes edit
    _hDetailNotes = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        8, 260, DETAIL_WIDTH - 16, 60, _hDetailPanel, (HMENU)IDC_EDIT_DEVICE_NOTES, hInst, nullptr);
    SendMessage(_hDetailNotes, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(_hDetailNotes);

    // Trust dropdown
    _hDetailTrust = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        8, 328, DETAIL_WIDTH - 16, 120, _hDetailPanel, (HMENU)IDC_COMBO_TRUST, hInst, nullptr);
    SendMessage(_hDetailTrust, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"unknown");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"owned");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"watchlist");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"guest");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"blocked");

    // Close button
    _hDetailClose = CreateWindowEx(0, L"BUTTON", L"Close Detail",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        8, DETAIL_WIDTH + 50, DETAIL_WIDTH - 16, 28,
        _hDetailPanel, (HMENU)9500, hInst, nullptr);
    SendMessage(_hDetailClose, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
}

void TabDevices::LayoutControls(int cx, int cy) {
    int listW = _detailVisible ? cx - DETAIL_WIDTH - 32 : cx - 32;
    if (_hList) SetWindowPos(_hList, nullptr, 16, 48, listW, cy - 64, SWP_NOZORDER);

    if (_hDetailPanel) {
        if (_detailVisible)
            SetWindowPos(_hDetailPanel, nullptr, cx - DETAIL_WIDTH - 16, 48, DETAIL_WIDTH, cy - 64, SWP_NOZORDER | SWP_SHOWWINDOW);
        else
            ShowWindow(_hDetailPanel, SW_HIDE);
    }

    // Reposition filter buttons
    int btnX = 290;
    for (int i = 0; i < 6; i++) {
        if (_hFilterBtns[i]) SetWindowPos(_hFilterBtns[i], nullptr, btnX, 12, 72, 26, SWP_NOZORDER);
        btnX += 76;
    }
}

LRESULT TabDevices::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabDevices::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushApp());
    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT TabDevices::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    if (id >= IDC_BTN_FILTER_ALL && id <= IDC_BTN_FILTER_CHANGED) {
        _filterMode = id - IDC_BTN_FILTER_ALL;
        ApplyFilter();
        return 0;
    }

    if (id == 9500) { // Close detail
        HideDetailPanel();
        return 0;
    }

    if (id == IDC_EDIT_SEARCH && HIWORD(wp) == EN_CHANGE) {
        ApplyFilter();
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabDevices::OnNotify(HWND hwnd, NMHDR* hdr) {
    if (!hdr) return 0;

    if (hdr->idFrom == IDC_LIST_DEVICES) {
        switch (hdr->code) {
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
        case NM_CUSTOMDRAW: {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)hdr;
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: {
                int row = (int)cd->nmcd.dwItemSpec;
                bool sel = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
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
                // Status dot column (col 0)
                if (cd->iSubItem == 0) {
                    int row = (int)cd->nmcd.dwItemSpec;
                    bool online = true;
                    if (row < (int)_filteredIndices.size() && _mainWnd) {
                        ScanResult r = _mainWnd->GetLastResult();
                        int idx = _filteredIndices[row];
                        if (idx < (int)r.devices.size())
                            online = r.devices[idx].online;
                    }
                    cd->clrText = online ? Theme::SUCCESS : Theme::TEXT_MUTED;
                    return CDRF_NEWFONT;
                }
                return CDRF_DODEFAULT;
            }
            default:
                return CDRF_DODEFAULT;
            }
        }
        }
    }

    return CDRF_DODEFAULT;
}

LRESULT TabDevices::OnScanComplete(HWND hwnd) {
    ApplyFilter();
    return 0;
}

void TabDevices::ApplyFilter() {
    if (!_mainWnd || !_hList) return;

    ScanResult r = _mainWnd->GetLastResult();

    // Get search text
    wchar_t searchBuf[256] = {};
    if (_hSearch) GetWindowText(_hSearch, searchBuf, 256);
    wstring search = searchBuf;
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    _filteredIndices.clear();
    for (int i = 0; i < (int)r.devices.size(); i++) {
        const Device& d = r.devices[i];

        // Apply filter mode
        switch (_filterMode) {
        case 1: if (!d.online) continue; break;
        case 2: if (d.trustState != L"unknown") continue; break;
        case 3: if (d.trustState != L"watchlist") continue; break;
        case 4: if (d.trustState != L"owned") continue; break;
        case 5: if (d.prevPorts == d.openPorts) continue; break;
        }

        // Apply search
        if (!search.empty()) {
            wstring ip = d.ip, mac = d.mac, name = d.hostname, vendor = d.vendor;
            std::transform(ip.begin(), ip.end(), ip.begin(), ::tolower);
            std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);

            if (ip.find(search) == wstring::npos &&
                mac.find(search) == wstring::npos &&
                name.find(search) == wstring::npos &&
                vendor.find(search) == wstring::npos)
                continue;
        }

        _filteredIndices.push_back(i);
    }

    PopulateList();
}

void TabDevices::PopulateList() {
    if (!_hList || !_mainWnd) return;

    ScanResult r = _mainWnd->GetLastResult();

    ListView_DeleteAllItems(_hList);

    for (int row = 0; row < (int)_filteredIndices.size(); row++) {
        int idx = _filteredIndices[row];
        if (idx >= (int)r.devices.size()) continue;
        const Device& d = r.devices[idx];

        LVITEM item = {};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)(d.online ? L"\u25CF" : L"\u25CB"); // filled/empty circle
        ListView_InsertItem(_hList, &item);

        // Name
        wstring name = d.customName.empty() ? d.hostname : d.customName;
        if (name.empty()) name = d.ip;
        ListView_SetItemText(_hList, row, 1, (LPWSTR)name.c_str());

        // IP (+ IPv6 badge)
        wstring ip = d.ip;
        if (!d.ipv6Address.empty()) ip += L" [v6]";
        ListView_SetItemText(_hList, row, 2, (LPWSTR)ip.c_str());

        ListView_SetItemText(_hList, row, 3, (LPWSTR)d.mac.c_str());
        ListView_SetItemText(_hList, row, 4, (LPWSTR)d.vendor.c_str());
        ListView_SetItemText(_hList, row, 5, (LPWSTR)d.deviceType.c_str());
        ListView_SetItemText(_hList, row, 6, (LPWSTR)d.trustState.c_str());

        // Ports
        wstring ports = GetPortSummary(d);
        ListView_SetItemText(_hList, row, 7, (LPWSTR)ports.c_str());
        ListView_SetItemText(_hList, row, 8, (LPWSTR)d.lastSeen.c_str());
    }
}

wstring TabDevices::GetPortSummary(const Device& dev) {
    if (dev.openPorts.empty()) return L"None";

    wstring s;
    int count = min((int)dev.openPorts.size(), 4);
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

void TabDevices::HideDetailPanel() {
    _detailVisible = false;
    ShowWindow(_hDetailPanel, SW_HIDE);

    RECT rc; GetClientRect(_hwnd, &rc);
    LayoutControls(rc.right, rc.bottom);
    InvalidateRect(_hwnd, nullptr, FALSE);
}

void TabDevices::UpdateDetailPanel(const Device& dev) {
    if (!_hDetailPanel) return;

    wstring name = dev.customName.empty() ? dev.hostname : dev.customName;
    if (name.empty()) name = dev.ip;

    if (_hDetailName)  SetWindowText(_hDetailName, name.c_str());
    if (_hDetailType)  SetWindowText(_hDetailType,
        (L"Type: " + dev.deviceType + L"  Confidence: " + std::to_wstring(dev.confidence) + L"%").c_str());
    if (_hDetailVendor)SetWindowText(_hDetailVendor,
        (L"Vendor: " + (dev.vendor.empty() ? L"Unknown" : dev.vendor)).c_str());
    if (_hDetailMac)   SetWindowText(_hDetailMac,
        (L"MAC: " + dev.mac + (dev.latencyMs >= 0 ? L"  Latency: " + std::to_wstring(dev.latencyMs) + L"ms" : L"")).c_str());

    // Ports
    wstring portStr = L"Ports: ";
    if (dev.openPorts.empty()) portStr += L"None";
    else for (int p : dev.openPorts) {
        auto it = ScanEngine::PORT_NAMES.find(p);
        portStr += std::to_wstring(p);
        if (it != ScanEngine::PORT_NAMES.end()) portStr += L"(" + it->second + L")";
        portStr += L" ";
    }
    if (_hDetailPorts) SetWindowText(_hDetailPorts, portStr.c_str());

    if (_hDetailLastSeen) SetWindowText(_hDetailLastSeen,
        (L"Last seen: " + dev.lastSeen).c_str());

    // mDNS
    wstring mdns = L"mDNS: ";
    for (auto& s : dev.mdnsServices) mdns += s + L" ";
    if (dev.mdnsServices.empty()) mdns += L"None";
    if (_hDetailMdns) SetWindowText(_hDetailMdns, mdns.c_str());

    // Anomalies
    wstring anoms;
    if (_mainWnd) {
        ScanResult r = _mainWnd->GetLastResult();
        for (auto& a : r.anomalies) {
            if (a.deviceIp == dev.ip)
                anoms += L"[" + a.severity + L"] " + a.description + L"\n";
        }
    }
    if (anoms.empty()) anoms = L"No anomalies";
    if (_hDetailAnoms) SetWindowText(_hDetailAnoms, anoms.c_str());

    // Notes
    if (_hDetailNotes) SetWindowText(_hDetailNotes, dev.notes.c_str());

    // Trust
    if (_hDetailTrust) {
        static const wchar_t* trustOpts[] = { L"unknown", L"owned", L"watchlist", L"guest", L"blocked" };
        for (int i = 0; i < 5; i++) {
            if (dev.trustState == trustOpts[i]) {
                SendMessage(_hDetailTrust, CB_SETCURSEL, i, 0);
                break;
            }
        }
    }
}

void TabDevices::RefreshList() {
    ApplyFilter();
}
