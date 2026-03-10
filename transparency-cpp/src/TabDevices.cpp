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
#include <shellapi.h>
#include <ws2tcpip.h>

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
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
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
    auto makeLbl = [&](const wchar_t* text, int y, int h = 18) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            8, y, DETAIL_WIDTH - 16, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto makeMonoLbl = [&](const wchar_t* text, int y, int h = 16) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            8, y, DETAIL_WIDTH - 16, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontMono(), TRUE);
        return hw;
    };

    int dy = 8;
    // Custom name edit
    makeLbl(L"Custom Name:", dy, 14); dy += 14;
    _hDetailCustomName = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        8, dy, DETAIL_WIDTH - 16, 22, _hDetailPanel, (HMENU)IDC_EDIT_DEVICE_NAME, hInst, nullptr);
    SendMessage(_hDetailCustomName, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(_hDetailCustomName);
    dy += 28;

    _hDetailName     = makeLbl(L"", dy, 18); dy += 20;
    _hDetailType     = makeLbl(L"", dy, 16); dy += 18;
    _hDetailAlt      = makeLbl(L"", dy, 30); dy += 32;  // confidence alternatives (2 lines)
    _hDetailVendor   = makeLbl(L"", dy, 16); dy += 18;
    _hDetailMac      = makeMonoLbl(L"", dy, 16); dy += 18;
    _hDetailLastSeen = makeLbl(L"", dy, 16); dy += 20;
    _hDetailPorts    = makeLbl(L"", dy, 36); dy += 38;
    _hDetailMdns     = makeLbl(L"", dy, 30); dy += 32;

    // IoT risk box (hidden when not IoT)
    _hDetailIotRisk = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        8, dy, DETAIL_WIDTH - 16, 60, _hDetailPanel, nullptr, hInst, nullptr);
    SendMessage(_hDetailIotRisk, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
    Theme::ApplyDarkEdit(_hDetailIotRisk);
    dy += 68;

    _hDetailAnoms = makeLbl(L"", dy, 50); dy += 54;

    // Notes edit
    makeLbl(L"Notes:", dy, 14); dy += 14;
    _hDetailNotes = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        8, dy, DETAIL_WIDTH - 16, 54, _hDetailPanel, (HMENU)IDC_EDIT_DEVICE_NOTES, hInst, nullptr);
    SendMessage(_hDetailNotes, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(_hDetailNotes);
    dy += 60;

    // Trust dropdown
    makeLbl(L"Trust:", dy, 14); dy += 14;
    _hDetailTrust = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        8, dy, DETAIL_WIDTH - 16, 120, _hDetailPanel, (HMENU)IDC_COMBO_TRUST, hInst, nullptr);
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
        8, dy, 80, 26, _hDetailPanel, (HMENU)IDC_BTN_DEVICE_SAVE, hInst, nullptr);
    SendMessage(_hDetailSave, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hDetailClose = CreateWindowEx(0, L"BUTTON", L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        DETAIL_WIDTH - 90, dy, 82, 26, _hDetailPanel, (HMENU)9500, hInst, nullptr);
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

    if (id == IDC_BTN_DEVICE_SAVE) { // Save name/notes/trust back to device
        if (_mainWnd && !_detailDeviceIp.empty()) {
            wchar_t nameBuf[256] = {}, notesBuf[1024] = {};
            if (_hDetailCustomName) GetWindowText(_hDetailCustomName, nameBuf, 256);
            if (_hDetailNotes) GetWindowText(_hDetailNotes, notesBuf, 1024);

            int trustSel = _hDetailTrust ? (int)SendMessage(_hDetailTrust, CB_GETCURSEL, 0, 0) : 0;
            static const wchar_t* trustOpts[] = { L"unknown", L"owned", L"watchlist", L"guest", L"blocked" };
            wstring trust = (trustSel >= 0 && trustSel < 5) ? trustOpts[trustSel] : L"unknown";

            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            for (auto& d : _mainWnd->_lastResult.devices) {
                if (d.ip == _detailDeviceIp) {
                    d.customName = nameBuf;
                    d.notes = notesBuf;
                    d.trustState = trust;
                    break;
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

LRESULT TabDevices::OnNotify(HWND hwnd, NMHDR* hdr) {
    if (!hdr) return 0;

    if (hdr->idFrom == IDC_LIST_DEVICES) {
        switch (hdr->code) {
        case NM_RCLICK: {
            NMITEMACTIVATE* nia = reinterpret_cast<NMITEMACTIVATE*>(hdr);
            if (nia->iItem >= 0 && nia->iItem < (int)_filteredIndices.size()) {
                POINT pt;
                GetCursorPos(&pt);
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

    // Custom name field
    if (_hDetailCustomName) SetWindowText(_hDetailCustomName, dev.customName.c_str());

    // Display name (hostname or IP)
    wstring displayName = dev.customName.empty() ? dev.hostname : dev.customName;
    if (displayName.empty()) displayName = dev.ip;
    if (_hDetailName) SetWindowText(_hDetailName, displayName.c_str());

    // Type + confidence
    if (_hDetailType) SetWindowText(_hDetailType,
        (dev.deviceType + L"  (" + std::to_wstring(dev.confidence) + L"% confidence)").c_str());

    // Confidence alternatives
    wstring altStr;
    if (!dev.altType1.empty())
        altStr += L"Alt 1: " + dev.altType1 + L" (" + std::to_wstring(dev.altConf1) + L"%)\r\n";
    if (!dev.altType2.empty())
        altStr += L"Alt 2: " + dev.altType2 + L" (" + std::to_wstring(dev.altConf2) + L"%)";
    if (altStr.empty()) altStr = L"No alternatives — run a Deep scan for better confidence";
    if (_hDetailAlt) SetWindowText(_hDetailAlt, altStr.c_str());

    if (_hDetailVendor) SetWindowText(_hDetailVendor,
        (L"Vendor: " + (dev.vendor.empty() ? L"Unknown" : dev.vendor)).c_str());
    if (_hDetailMac) SetWindowText(_hDetailMac,
        (dev.mac + (dev.latencyMs >= 0 ? L"   " + std::to_wstring(dev.latencyMs) + L"ms" : L"")).c_str());
    if (_hDetailLastSeen) SetWindowText(_hDetailLastSeen,
        (L"Last seen: " + dev.lastSeen).c_str());

    // Ports
    wstring portStr;
    if (dev.openPorts.empty()) portStr = L"No open ports";
    else for (int p : dev.openPorts) {
        auto it = ScanEngine::PORT_NAMES.find(p);
        portStr += std::to_wstring(p);
        if (it != ScanEngine::PORT_NAMES.end()) portStr += L"/" + it->second;
        portStr += L"  ";
    }
    if (_hDetailPorts) SetWindowText(_hDetailPorts, portStr.c_str());

    // mDNS
    wstring mdns;
    for (auto& s : dev.mdnsServices) mdns += s + L"  ";
    if (mdns.empty()) mdns = L"No mDNS services";
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

    // Anomalies for this device
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

    _detailDeviceIp = dev.ip;
}

void TabDevices::RefreshList() {
    ApplyFilter();
}

void TabDevices::ShowDeviceContextMenu(HWND hwnd, int x, int y, int deviceIdx) {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();
    if (deviceIdx < 0 || deviceIdx >= (int)r.devices.size()) return;
    const Device& dev = r.devices[deviceIdx];

    HMENU hMenu = CreatePopupMenu();

    // Basic actions always available
    AppendMenu(hMenu, MF_STRING, 12001, L"Ping Device");
    AppendMenu(hMenu, MF_STRING, 12002, L"Traceroute");
    AppendMenu(hMenu, MF_STRING, 12003, L"Port Scan");
    AppendMenu(hMenu, MF_STRING, 12004, L"Copy IP Address");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);

    // Trust state options
    AppendMenu(hMenu, MF_STRING, 12010, L"Mark as Owned");
    AppendMenu(hMenu, MF_STRING, 12011, L"Mark as Guest");
    AppendMenu(hMenu, MF_STRING, 12012, L"Add to Watchlist");
    AppendMenu(hMenu, MF_STRING, 12013, L"Block Device");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);

    // Connection-based options
    bool hasWeb = false, hasSsh = false, hasRdp = false, hasFtp = false, hasSamba = false;
    for (int p : dev.openPorts) {
        if (p == 80 || p == 443 || p == 8080 || p == 8443) hasWeb = true;
        if (p == 22) hasSsh = true;
        if (p == 3389) hasRdp = true;
        if (p == 21) hasFtp = true;
        if (p == 445 || p == 139) hasSamba = true;
    }

    if (hasWeb)   AppendMenu(hMenu, MF_STRING, 12020, L"Open in Browser");
    if (hasSsh)   AppendMenu(hMenu, MF_STRING, 12021, L"Connect via SSH");
    if (hasRdp)   AppendMenu(hMenu, MF_STRING, 12022, L"Remote Desktop (RDP)");
    if (hasFtp)   AppendMenu(hMenu, MF_STRING, 12023, L"Open FTP Connection");
    if (hasSamba) AppendMenu(hMenu, MF_STRING, 12024, L"Browse Network Share");

    if (hasWeb || hasSsh || hasRdp || hasFtp || hasSamba)
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);

    // Wake-on-LAN (if offline and has MAC)
    if (!dev.online && !dev.mac.empty())
        AppendMenu(hMenu, MF_STRING, 12030, L"Wake-on-LAN");

    // DNS lookup
    AppendMenu(hMenu, MF_STRING, 12031, L"Reverse DNS Lookup");

    // Detail panel
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, 12040, L"View Details");

    // Store device IP for command handler
    _detailDeviceIp = dev.ip;

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 0) return;

    auto isSafeIp = [](const wstring& ip) {
        if (ip.empty() || ip.length() > 255) return false;
        for (wchar_t c : ip) {
            if (!iswalnum(c) && c != L'.' && c != L':' && c != L'-') return false;
        }
        return true;
    };

    switch (cmd) {
    case 12001: { // Ping
        if (!isSafeIp(dev.ip)) break;
        wstring cmdLine = L"cmd /c start cmd /k ping " + dev.ip;
        _wsystem(cmdLine.c_str());
        break;
    }
    case 12002: { // Traceroute
        if (!isSafeIp(dev.ip)) break;
        wstring cmdLine = L"cmd /c start cmd /k tracert " + dev.ip;
        _wsystem(cmdLine.c_str());
        break;
    }
    case 12003: { // Port Scan — switch to tools tab
        if (_mainWnd) _mainWnd->SwitchTab(Tab::Tools);
        break;
    }
    case 12004: { // Copy IP
        if (OpenClipboard(hwnd)) {
            EmptyClipboard();
            size_t len = (dev.ip.size() + 1) * sizeof(wchar_t);
            HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len);
            if (hg) {
                memcpy(GlobalLock(hg), dev.ip.c_str(), len);
                GlobalUnlock(hg);
                SetClipboardData(CF_UNICODETEXT, hg);
            }
            CloseClipboard();
        }
        break;
    }
    case 12010: { // Mark Owned
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices) {
            if (d.ip == dev.ip) { d.trustState = L"owned"; break; }
        }
        ApplyFilter();
        break;
    }
    case 12011: { // Mark Guest
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices) {
            if (d.ip == dev.ip) { d.trustState = L"guest"; break; }
        }
        ApplyFilter();
        break;
    }
    case 12012: { // Watchlist
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices) {
            if (d.ip == dev.ip) { d.trustState = L"watchlist"; break; }
        }
        ApplyFilter();
        break;
    }
    case 12013: { // Block
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices) {
            if (d.ip == dev.ip) { d.trustState = L"blocked"; break; }
        }
        ApplyFilter();
        break;
    }
    case 12020: { // Open in Browser
        wstring url = L"http://" + dev.ip;
        for (int p : dev.openPorts) {
            if (p == 443 || p == 8443) { url = L"https://" + dev.ip; break; }
            if (p == 8080) { url = L"http://" + dev.ip + L":8080"; break; }
        }
        ShellExecute(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);
        break;
    }
    case 12021: { // SSH
        if (!isSafeIp(dev.ip)) break;
        wstring cmdLine = L"cmd /c start cmd /k ssh " + dev.ip;
        _wsystem(cmdLine.c_str());
        break;
    }
    case 12022: { // RDP
        ShellExecute(nullptr, L"open", L"mstsc.exe", (L"/v:" + dev.ip).c_str(), nullptr, SW_SHOW);
        break;
    }
    case 12023: { // FTP
        wstring url = L"ftp://" + dev.ip;
        ShellExecute(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);
        break;
    }
    case 12024: { // Network share
        wstring path = L"\\\\" + dev.ip;
        ShellExecute(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOW);
        break;
    }
    case 12030: { // Wake-on-LAN
        MessageBox(hwnd, (L"Wake-on-LAN sent to " + dev.mac).c_str(), L"WOL", MB_OK | MB_ICONINFORMATION);
        break;
    }
    case 12031: { // Reverse DNS
        if (!isSafeIp(dev.ip)) break;
        wstring cmdLine = L"cmd /c start cmd /k nslookup " + dev.ip;
        _wsystem(cmdLine.c_str());
        break;
    }
    case 12040: { // View Details
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
