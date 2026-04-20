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

class TabHistory {
public:
    TabHistory() = default;
    ~TabHistory() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

    void Refresh();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnCustomDraw(NMHDR* hdr);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void PopulateList();
    void UpdateFilterButtons();

    HWND _hwnd    = nullptr;
    MainWindow* _mainWnd = nullptr;

    HWND _hList       = nullptr;
    HWND _hBtnAll     = nullptr;
    HWND _hBtnJoined  = nullptr;
    HWND _hBtnLeft    = nullptr;
    HWND _hBtnRecon   = nullptr;
    HWND _hBtnClear   = nullptr;
    HWND _hCountLabel = nullptr;

    int _filter = 0; // 0=all, 1=joined, 2=left, 3=reconnected

    static const wchar_t* s_className;
};
