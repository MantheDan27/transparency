#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <commctrl.h>
#include <objbase.h>
#include <dwmapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")

#include "ui/win32/main_window.h"

// Manifest for Common Controls v6 (required for visual styles and modern list views)
#pragma comment(linker, "/manifestdependency:\"type='win32' \
    name='Microsoft.Windows.Common-Controls' \
    version='6.0.0.0' \
    processorArchitecture='*' \
    publicKeyToken='6595b64144ccf1df' \
    language='*'\"")

#ifdef _MSC_VER
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/,
    LPWSTR    /*lpCmdLine*/,
    int       /*nCmdShow*/)
#else
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/,
    LPSTR     /*lpCmdLine*/,
    int       /*nCmdShow*/)
#endif
{
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        MessageBox(nullptr, L"Failed to initialize COM.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBox(nullptr, L"Failed to initialize Winsock.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES
               | ICC_LISTVIEW_CLASSES
               | ICC_BAR_CLASSES
               | ICC_PROGRESS_CLASS
               | ICC_STANDARD_CLASSES
               | ICC_LINK_CLASS;
    if (!InitCommonControlsEx(&icc)) {
        MessageBox(nullptr, L"Failed to initialize Common Controls.", L"Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        CoUninitialize();
        return 1;
    }

    // Create main window
    if (!MainWindow::Create(hInstance)) {
        MessageBox(nullptr, L"Failed to create main window.", L"Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        CoUninitialize();
        return 1;
    }

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    WSACleanup();
    CoUninitialize();

    return (int)msg.wParam;
}
