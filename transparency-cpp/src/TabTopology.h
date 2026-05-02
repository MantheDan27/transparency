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
#include <gdiplus.h>

class MainWindow;

class TabTopology {
public:
    TabTopology() = default;
    ~TabTopology() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnMouseMove(HWND hwnd, int x, int y);
    LRESULT OnLButtonDown(HWND hwnd, int x, int y);
    LRESULT OnScanComplete(HWND hwnd);
    LRESULT OnMonitorTick(HWND hwnd);
    LRESULT OnTimer(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp);

    struct NodeInfo {
        int cx = 0, cy = 0;
        int radius = 0;
        bool isGateway = false;
        Device dev;
    };

    void BuildLayout(int totalW, int totalH);
    void DrawCanvas(HDC hdc, const RECT& rc);
    void DrawLink(Gdiplus::Graphics& g, const NodeInfo& from, const NodeInfo& to);
    void DrawNode(Gdiplus::Graphics& g, HDC hdc, const NodeInfo& n, bool hover, bool sel);
    void DrawDetailPanel(HDC hdc, const RECT& rc);
    void DrawToolbar(HDC hdc, const RECT& rc);
    int  HitTest(int x, int y) const;
    static COLORREF NodeColor(const Device& d);
    static std::wstring DeviceLabel(const Device& d);
    static const wchar_t* DeviceIcon(const Device& d);

    HWND        _hwnd    = nullptr;
    MainWindow* _mainWnd = nullptr;

    std::vector<NodeInfo> _nodes;
    int _gatewayIdx   = -1;
    int _hoveredNode  = -1;
    int _selectedNode = -1;

    int _totalW   = 0;
    int _totalH   = 0;
    int _animTick = 0;

    static const int DETAIL_W  = 260;
    static const int TOOLBAR_H = 40;
    static const int BTN_REFRESH = 14001;

    static const wchar_t* s_className;
};
