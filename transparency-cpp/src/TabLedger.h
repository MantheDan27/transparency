#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "Models.h"
#include "Resource.h"
#include "Theme.h"

class MainWindow;

class TabLedger {
public:
    TabLedger() = default;
    ~TabLedger() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

    void Refresh();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnScanComplete(HWND hwnd);
    LRESULT OnNotify(HWND hwnd, NMHDR* hdr);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void PopulateList();
    void RefreshSnapshots();
    void RunDiff();

    HWND _hwnd     = nullptr;
    MainWindow* _mainWnd = nullptr;

    // Ledger list
    HWND _hList       = nullptr;
    HWND _hBtnExport  = nullptr;
    HWND _hBtnClear   = nullptr;
    HWND _hEntryCount = nullptr;

    // Snapshot diff section
    HWND _hSnapLabel1  = nullptr;
    HWND _hComboSnap1  = nullptr;
    HWND _hSnapLabel2  = nullptr;
    HWND _hComboSnap2  = nullptr;
    HWND _hBtnDiff     = nullptr;
    HWND _hDiffList    = nullptr;

    static const wchar_t* s_className;
};
