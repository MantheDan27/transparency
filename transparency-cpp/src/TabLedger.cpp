#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <sstream>
#include <mutex>

#include "TabLedger.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

const wchar_t* TabLedger::s_className = L"TransparencyTabLedger";

bool TabLedger::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
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

LRESULT TabLedger::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    return 0;
}

void TabLedger::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Header / toolbar
    HWND hHdr = CreateWindowEx(0, L"STATIC", L"Data Ledger - Transparent record of all network events",
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

    // ListView
    _hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | WS_VSCROLL,
        16, 68, cx - 32, cy - 84,
        hwnd, (HMENU)IDC_LIST_LEDGER, hInst, nullptr);

    SendMessage(_hList, WM_SETFONT, (WPARAM)Theme::FontMono(), TRUE);
    ListView_SetExtendedListViewStyle(_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hList);

    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt  = LVCFMT_LEFT;

    col.cx = 150; col.pszText = (LPWSTR)L"Time";
    ListView_InsertColumn(_hList, 0, &col);

    col.cx = 180; col.pszText = (LPWSTR)L"Action";
    ListView_InsertColumn(_hList, 1, &col);

    col.cx = 600; col.pszText = (LPWSTR)L"Details";
    ListView_InsertColumn(_hList, 2, &col);
}

void TabLedger::LayoutControls(int cx, int cy) {
    if (_hList) SetWindowPos(_hList, nullptr, 16, 68, cx - 32, cy - 84, SWP_NOZORDER);
    if (_hBtnExport) SetWindowPos(_hBtnExport, nullptr, cx - 188, 12, 88, 26, SWP_NOZORDER);
    if (_hBtnClear)  SetWindowPos(_hBtnClear,  nullptr, cx - 96, 12, 80, 26, SWP_NOZORDER);
}

LRESULT TabLedger::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabLedger::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushApp());

    // Separator under header
    RECT sep = { 16, 44, rc.right - 16, 45 };
    FillRect(hdc, &sep, Theme::BrushBorder());

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
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"CSV Files\0*.csv\0All Files\0*.*\0";
        ofn.lpstrDefExt = L"csv";
        ofn.lpstrTitle = L"Export Ledger";
        ofn.Flags = OFN_OVERWRITEPROMPT;

        if (GetSaveFileName(&ofn)) {
            HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                std::string header = "Timestamp,Action,Details\r\n";
                DWORD w;
                WriteFile(hFile, header.c_str(), (DWORD)header.size(), &w, nullptr);

                std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
                for (auto& e : _mainWnd->_ledger) {
                    std::wstring line = L"\"" + e.timestamp + L"\",\"" + e.action + L"\",\"" + e.details + L"\"\r\n";
                    std::string narrow;
                    int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (n > 0) {
                        narrow.resize(n - 1);
                        WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, &narrow[0], n, nullptr, nullptr);
                        WriteFile(hFile, narrow.c_str(), (DWORD)narrow.size(), &w, nullptr);
                    }
                }
                CloseHandle(hFile);
                MessageBox(hwnd, L"Ledger exported.", L"Export", MB_OK | MB_ICONINFORMATION);
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
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabLedger::OnScanComplete(HWND hwnd) {
    PopulateList();
    return 0;
}

LRESULT TabLedger::OnNotify(HWND hwnd, NMHDR* hdr) {
    if (!hdr) return 0;

    if (hdr->idFrom == IDC_LIST_LEDGER && hdr->code == NM_CUSTOMDRAW) {
        NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)hdr;
        switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
            int row = (int)cd->nmcd.dwItemSpec;
            cd->clrTextBk = (row % 2 == 0) ? Theme::BG_APP : Theme::BG_ROW_ALT;
            cd->clrText   = Theme::TEXT_PRIMARY;
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

    // Show in reverse order (newest first)
    int row = 0;
    for (int i = (int)entries.size() - 1; i >= 0; i--) {
        auto& e = entries[i];
        LVITEM item = {};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
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

void TabLedger::Refresh() {
    PopulateList();
}
