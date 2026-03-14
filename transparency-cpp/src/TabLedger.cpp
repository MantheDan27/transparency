#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <sstream>
#include <mutex>
#include <map>

#include "TabLedger.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

const wchar_t* TabLedger::s_className = L"TransparencyTabLedger";

enum {
    ID_COMBO_SNAP1 = 9700,
    ID_COMBO_SNAP2 = 9701,
    ID_BTN_DIFF    = 9702,
    ID_DIFF_LIST   = 9703,
};

bool TabLedger::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
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

LRESULT CALLBACK TabLedger::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabLedger* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabLedger*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabLedger*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:     return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:       self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:      return self->OnPaint(hwnd);
    case WM_ERASEBKGND:{RECT rc;GetClientRect(hwnd,&rc);FillRect((HDC)wp,&rc,Theme::BrushSurface());return 1;}
    case WM_COMMAND:    return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY:     return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        SetTextColor((HDC)wp, Theme::TEXT_PRIMARY);
        SetBkColor((HDC)wp, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }
    case WM_SCAN_COMPLETE: return self->OnScanComplete(hwnd);
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabLedger::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    return 0;
}

void TabLedger::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    HWND hHdr = CreateWindowEx(0, L"STATIC",
        L"Data Ledger — transparent record of all network events",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 14, cx - 200, 22, hwnd, nullptr, hInst, nullptr);
    SendMessage(hHdr, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hBtnExport = CreateWindowEx(0, L"BUTTON", L"Export CSV",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cx - 188, 12, 88, 26, hwnd, (HMENU)IDC_BTN_EXPORT_LEDGER, hInst, nullptr);
    SendMessage(_hBtnExport, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    _hBtnClear = CreateWindowEx(0, L"BUTTON", L"Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cx - 96, 12, 80, 26, hwnd, (HMENU)9800, hInst, nullptr);
    SendMessage(_hBtnClear, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    _hEntryCount = CreateWindowEx(0, L"STATIC", L"0 entries",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 46, 200, 18, hwnd, nullptr, hInst, nullptr);
    SendMessage(_hEntryCount, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    // ── Ledger ListView ───────────────────────────────────────────────────────
    // Top half: ledger
    int ledgerH = (cy - 68 - 16) / 2 - 8;
    if (ledgerH < 80) ledgerH = 80;

    _hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | WS_VSCROLL,
        16, 68, cx - 32, ledgerH,
        hwnd, (HMENU)IDC_LIST_LEDGER, hInst, nullptr);
    SendMessage(_hList, WM_SETFONT, (WPARAM)Theme::FontMono(), TRUE);
    ListView_SetExtendedListViewStyle(_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hList);

    {
        LVCOLUMN col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
        col.cx = 150; col.pszText = (LPWSTR)L"Time";     ListView_InsertColumn(_hList, 0, &col);
        col.cx = 180; col.pszText = (LPWSTR)L"Action";   ListView_InsertColumn(_hList, 1, &col);
        col.cx = 600; col.pszText = (LPWSTR)L"Details";  ListView_InsertColumn(_hList, 2, &col);
    }

    // ── Snapshot Diff ─────────────────────────────────────────────────────────
    int diffTop = 68 + ledgerH + 12;

    HWND hDiffHdr = CreateWindowEx(0, L"STATIC", L"Snapshot Diff",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, diffTop, 140, 20, hwnd, nullptr, hInst, nullptr);
    SendMessage(hDiffHdr, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
    diffTop += 24;

    _hSnapLabel1 = CreateWindowEx(0, L"STATIC", L"Baseline:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, diffTop + 4, 64, 20, hwnd, nullptr, hInst, nullptr);
    SendMessage(_hSnapLabel1, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hComboSnap1 = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        84, diffTop, 180, 120, hwnd, (HMENU)(INT_PTR)ID_COMBO_SNAP1, hInst, nullptr);
    SendMessage(_hComboSnap1, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hSnapLabel2 = CreateWindowEx(0, L"STATIC", L"Compare:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        276, diffTop + 4, 64, 20, hwnd, nullptr, hInst, nullptr);
    SendMessage(_hSnapLabel2, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hComboSnap2 = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        344, diffTop, 180, 120, hwnd, (HMENU)(INT_PTR)ID_COMBO_SNAP2, hInst, nullptr);
    SendMessage(_hComboSnap2, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hBtnDiff = CreateWindowEx(0, L"BUTTON", L"Run Diff",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        534, diffTop, 80, 26, hwnd, (HMENU)(INT_PTR)ID_BTN_DIFF, hInst, nullptr);
    SendMessage(_hBtnDiff, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    diffTop += 32;

    int diffH = cy - diffTop - 16;
    if (diffH < 60) diffH = 60;

    _hDiffList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | WS_VSCROLL,
        16, diffTop, cx - 32, diffH,
        hwnd, (HMENU)(INT_PTR)ID_DIFF_LIST, hInst, nullptr);
    SendMessage(_hDiffList, WM_SETFONT, (WPARAM)Theme::FontMono(), TRUE);
    ListView_SetExtendedListViewStyle(_hDiffList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hDiffList);

    {
        LVCOLUMN col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
        col.cx = 80;  col.pszText = (LPWSTR)L"Change";   ListView_InsertColumn(_hDiffList, 0, &col);
        col.cx = 130; col.pszText = (LPWSTR)L"MAC";      ListView_InsertColumn(_hDiffList, 1, &col);
        col.cx = 130; col.pszText = (LPWSTR)L"IP";       ListView_InsertColumn(_hDiffList, 2, &col);
        col.cx = 160; col.pszText = (LPWSTR)L"Hostname"; ListView_InsertColumn(_hDiffList, 3, &col);
        col.cx = 300; col.pszText = (LPWSTR)L"Details";  ListView_InsertColumn(_hDiffList, 4, &col);
    }

    RefreshSnapshots();
}

void TabLedger::LayoutControls(int cx, int cy) {
    if (!_hList) return;
    int ledgerH = (cy - 68 - 16) / 2 - 8;
    if (ledgerH < 80) ledgerH = 80;

    SetWindowPos(_hList, nullptr, 16, 68, cx - 32, ledgerH, SWP_NOZORDER);
    if (_hBtnExport) SetWindowPos(_hBtnExport, nullptr, cx - 188, 12, 88, 26, SWP_NOZORDER);
    if (_hBtnClear)  SetWindowPos(_hBtnClear,  nullptr, cx - 96,  12, 80, 26, SWP_NOZORDER);

    int diffTop = 68 + ledgerH + 12 + 24 + 32;
    int diffH   = cy - diffTop - 16;
    if (diffH < 60) diffH = 60;
    if (_hDiffList)
        SetWindowPos(_hDiffList, nullptr, 16, diffTop, cx - 32, diffH, SWP_NOZORDER);
}

LRESULT TabLedger::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabLedger::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushSurface());

    // Separator
    RECT sep = { 16, 44, rc.right - 16, 45 };
    FillRect(hdc, &sep, Theme::BrushBorderSubtle());

    // Section label — caption style
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontCaption());
    RECT hdr = { 16, 54, 200, 66 };
    DrawText(hdc, L"EVENT LOG", -1, &hdr, DT_LEFT | DT_SINGLELINE);
    SelectObject(hdc, old);

    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT TabLedger::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    switch (id) {
    case IDC_BTN_EXPORT_LEDGER: {
        if (!_mainWnd) break;
        wchar_t path[MAX_PATH] = {};
        OPENFILENAME ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = hwnd;
        ofn.lpstrFile   = path;
        ofn.nMaxFile    = MAX_PATH;
        ofn.lpstrFilter = L"CSV Files\0*.csv\0All Files\0*.*\0";
        ofn.lpstrDefExt = L"csv";
        ofn.lpstrTitle  = L"Export Ledger";
        ofn.Flags       = OFN_OVERWRITEPROMPT;
        if (GetSaveFileName(&ofn)) {
            HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS, 0, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD w;
                WriteFile(hFile, "Timestamp,Action,Details\r\n", 26, &w, nullptr);
                std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
                for (auto& e : _mainWnd->_ledger) {
                    wstring line = L"\"" + e.timestamp + L"\",\"" +
                                   e.action + L"\",\"" + e.details + L"\"\r\n";
                    int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1,
                                                nullptr, 0, nullptr, nullptr);
                    if (n > 0) {
                        std::string narrow(n - 1, 0);
                        WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1,
                                            &narrow[0], n, nullptr, nullptr);
                        WriteFile(hFile, narrow.c_str(), (DWORD)narrow.size(), &w, nullptr);
                    }
                }
                CloseHandle(hFile);
                MessageBox(hwnd, L"Ledger exported.", L"Export",
                           MB_OK | MB_ICONINFORMATION);
            }
        }
        break;
    }

    case 9800: // Clear
        if (_mainWnd) {
            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            _mainWnd->_ledger.clear();
        }
        PopulateList();
        break;

    case ID_BTN_DIFF:
        RunDiff();
        break;
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabLedger::OnScanComplete(HWND hwnd) {
    PopulateList();
    // Auto-save snapshot
    if (_mainWnd) _mainWnd->SaveSnapshot();
    RefreshSnapshots();
    return 0;
}

LRESULT TabLedger::OnNotify(HWND hwnd, NMHDR* hdr) {
    if (!hdr) return 0;

    if (hdr->idFrom == IDC_LIST_LEDGER && hdr->code == NM_CUSTOMDRAW) {
        NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)hdr;
        switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:   return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
            int row = (int)cd->nmcd.dwItemSpec;
            cd->clrTextBk = (row % 2 == 0) ? Theme::BG_SURFACE : Theme::BG_ROW_ALT;
            cd->clrText   = Theme::TEXT_PRIMARY;
            return CDRF_NEWFONT;
        }
        }
    }

    // Color diff list rows by change type
    if (hdr->idFrom == ID_DIFF_LIST && hdr->code == NM_CUSTOMDRAW) {
        NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)hdr;
        switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:   return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
            // Read first subitem text to determine color
            wchar_t buf[32] = {};
            ListView_GetItemText(_hDiffList, (int)cd->nmcd.dwItemSpec, 0, buf, 32);
            if      (wcscmp(buf, L"Added")   == 0) cd->clrText = Theme::SUCCESS;
            else if (wcscmp(buf, L"Removed") == 0) cd->clrText = Theme::DANGER;
            else if (wcscmp(buf, L"Changed") == 0) cd->clrText = Theme::WARNING;
            else                                    cd->clrText = Theme::TEXT_MUTED;
            cd->clrTextBk = Theme::BG_SURFACE;
            return CDRF_NEWFONT;
        }
        }
    }

    return CDRF_DODEFAULT;
}

void TabLedger::PopulateList() {
    if (!_hList || !_mainWnd) return;
    ListView_DeleteAllItems(_hList);

    std::vector<LedgerEntry> entries;
    {
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        entries = _mainWnd->_ledger;
    }

    int row = 0;
    for (int i = (int)entries.size() - 1; i >= 0; i--) {
        auto& e = entries[i];
        LVITEM item = {};
        item.mask    = LVIF_TEXT;
        item.iItem   = row;
        item.pszText = (LPWSTR)e.timestamp.c_str();
        ListView_InsertItem(_hList, &item);
        ListView_SetItemText(_hList, row, 1, (LPWSTR)e.action.c_str());
        ListView_SetItemText(_hList, row, 2, (LPWSTR)e.details.c_str());
        row++;
    }

    if (_hEntryCount) {
        wchar_t buf[64];
        swprintf_s(buf, L"%d entries", (int)entries.size());
        SetWindowText(_hEntryCount, buf);
    }
}

void TabLedger::RefreshSnapshots() {
    if (!_hComboSnap1 || !_hComboSnap2 || !_mainWnd) return;

    std::vector<ScanResult> snaps;
    {
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        snaps = _mainWnd->_snapshots;
    }

    SendMessage(_hComboSnap1, CB_RESETCONTENT, 0, 0);
    SendMessage(_hComboSnap2, CB_RESETCONTENT, 0, 0);

    for (size_t i = 0; i < snaps.size(); i++) {
        wstring label = L"[" + std::to_wstring(i + 1) + L"] " + snaps[i].scannedAt;
        if (label.empty()) label = L"Snapshot " + std::to_wstring(i + 1);
        SendMessage(_hComboSnap1, CB_ADDSTRING, 0, (LPARAM)label.c_str());
        SendMessage(_hComboSnap2, CB_ADDSTRING, 0, (LPARAM)label.c_str());
    }

    // Default: snap1 = oldest, snap2 = newest
    if (snaps.size() >= 2) {
        SendMessage(_hComboSnap1, CB_SETCURSEL, 0, 0);
        SendMessage(_hComboSnap2, CB_SETCURSEL, (WPARAM)(snaps.size() - 1), 0);
    } else if (snaps.size() == 1) {
        SendMessage(_hComboSnap1, CB_SETCURSEL, 0, 0);
        SendMessage(_hComboSnap2, CB_SETCURSEL, 0, 0);
    }
}

void TabLedger::RunDiff() {
    if (!_hDiffList || !_mainWnd) return;

    std::vector<ScanResult> snaps;
    {
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        snaps = _mainWnd->_snapshots;
    }

    if (snaps.size() < 2) {
        MessageBox(_hwnd,
            L"At least two snapshots are required to diff.\n"
            L"Run two or more scans first.",
            L"Diff", MB_OK | MB_ICONINFORMATION);
        return;
    }

    int idx1 = (int)SendMessage(_hComboSnap1, CB_GETCURSEL, 0, 0);
    int idx2 = (int)SendMessage(_hComboSnap2, CB_GETCURSEL, 0, 0);
    if (idx1 < 0) idx1 = 0;
    if (idx2 < 0) idx2 = (int)snaps.size() - 1;
    if (idx1 >= (int)snaps.size()) idx1 = 0;
    if (idx2 >= (int)snaps.size()) idx2 = (int)snaps.size() - 1;

    const ScanResult& base    = snaps[idx1];
    const ScanResult& compare = snaps[idx2];

    // Build MAC-keyed maps
    std::map<wstring, const Device*> baseMap, cmpMap;
    for (auto& d : base.devices)    baseMap[d.mac] = &d;
    for (auto& d : compare.devices) cmpMap[d.mac]  = &d;

    ListView_DeleteAllItems(_hDiffList);
    int row = 0;

    auto addRow = [&](const wchar_t* change, const wstring& mac,
                      const wstring& ip, const wstring& host,
                      const wstring& details) {
        LVITEM item = {}; item.mask = LVIF_TEXT; item.iItem = row;
        item.pszText = (LPWSTR)change;
        ListView_InsertItem(_hDiffList, &item);
        ListView_SetItemText(_hDiffList, row, 1, (LPWSTR)mac.c_str());
        ListView_SetItemText(_hDiffList, row, 2, (LPWSTR)ip.c_str());
        ListView_SetItemText(_hDiffList, row, 3, (LPWSTR)host.c_str());
        ListView_SetItemText(_hDiffList, row, 4, (LPWSTR)details.c_str());
        row++;
    };

    // Added (in compare, not in base)
    for (auto& [mac, d] : cmpMap) {
        if (baseMap.find(mac) == baseMap.end())
            addRow(L"Added", mac, d->ip, d->hostname, L"New device appeared");
    }

    // Removed (in base, not in compare)
    for (auto& [mac, d] : baseMap) {
        if (cmpMap.find(mac) == cmpMap.end())
            addRow(L"Removed", mac, d->ip, d->hostname, L"Device disappeared");
    }

    // Changed (in both — check IP, ports, hostname, trust)
    for (auto& [mac, bd] : baseMap) {
        auto it = cmpMap.find(mac);
        if (it == cmpMap.end()) continue;
        const Device* cd = it->second;

        wstring details;
        if (bd->ip != cd->ip)
            details += L"IP: " + bd->ip + L"→" + cd->ip + L"  ";
        if (bd->hostname != cd->hostname)
            details += L"Name: " + bd->hostname + L"→" + cd->hostname + L"  ";
        if (bd->trustState != cd->trustState)
            details += L"Trust: " + bd->trustState + L"→" + cd->trustState + L"  ";
        if (bd->openPorts != cd->openPorts) {
            details += L"Ports changed  ";
        }
        if (bd->latencyMs != cd->latencyMs && cd->latencyMs >= 0) {
            details += L"Latency: " + std::to_wstring(bd->latencyMs) +
                       L"ms→" + std::to_wstring(cd->latencyMs) + L"ms  ";
        }

        if (!details.empty())
            addRow(L"Changed", mac, cd->ip, cd->hostname, details);
        else
            addRow(L"Same", mac, cd->ip, cd->hostname, L"No changes");
    }

    // Summary
    wchar_t summary[128];
    swprintf_s(summary, L"Diff: %d row(s) between snapshot %d and %d",
               row, idx1 + 1, idx2 + 1);
    if (_hEntryCount) SetWindowText(_hEntryCount, summary);
}

void TabLedger::Refresh() {
    PopulateList();
    RefreshSnapshots();
}
