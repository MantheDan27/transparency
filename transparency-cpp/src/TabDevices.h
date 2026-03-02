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

class TabDevices {
public:
    TabDevices() = default;
    ~TabDevices() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

    void RefreshList();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnNotify(HWND hwnd, NMHDR* hdr);
    LRESULT OnScanComplete(HWND hwnd);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void PopulateList();
    void ShowDetailPanel(int idx);
    void HideDetailPanel();
    void UpdateDetailPanel(const Device& dev);
    void ApplyFilter();

    wstring GetPortSummary(const Device& dev);

    HWND _hwnd = nullptr;
    MainWindow* _mainWnd = nullptr;

    // Toolbar
    HWND _hSearch = nullptr;
    HWND _hFilterBtns[6] = {};

    // List view
    HWND _hList = nullptr;

    // Detail panel
    HWND _hDetailPanel = nullptr;
    HWND _hDetailName  = nullptr;
    HWND _hDetailType  = nullptr;
    HWND _hDetailVendor = nullptr;
    HWND _hDetailMac   = nullptr;
    HWND _hDetailPorts = nullptr;
    HWND _hDetailLastSeen = nullptr;
    HWND _hDetailNotes = nullptr;
    HWND _hDetailTrust = nullptr;
    HWND _hDetailMdns  = nullptr;
    HWND _hDetailAnoms = nullptr;
    HWND _hDetailClose = nullptr;

    // Sort state
    int _sortCol = 0;
    bool _sortAsc = true;

    // Filter
    int _filterMode = 0; // 0=All, 1=Online, 2=Unknown, 3=Watchlist, 4=Owned, 5=Changed

    // Filtered device indices
    std::vector<int> _filteredIndices;

    bool _detailVisible = false;
    int _selectedDevice = -1;

    static const wchar_t* s_className;
    static const int DETAIL_WIDTH = 340;
};
