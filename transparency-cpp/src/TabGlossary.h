#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include "Resource.h"
#include "Theme.h"

class MainWindow;

class TabGlossary {
public:
    TabGlossary() = default;
    ~TabGlossary() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnNotify(HWND hwnd, NMHDR* hdr);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void PopulateList();

    HWND _hwnd    = nullptr;
    HWND _hList   = nullptr;
    HWND _hDetail = nullptr;

    static const wchar_t* s_className;
};
