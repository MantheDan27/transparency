#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>

// Login dialog result
enum class LoginResult {
    LoggedIn,       // User successfully logged in
    Skipped,        // User chose to continue without login
    Cancelled       // User closed the dialog
};

class LoginDialog {
public:
    // Show the login dialog. Returns the result.
    static LoginResult Show(HWND parent);

    // Get the logged-in user's email (empty if skipped/cancelled)
    static std::wstring GetUserEmail() { return s_userEmail; }
    static std::wstring GetUserId() { return s_userId; }
    static bool IsLoggedIn() { return s_loggedIn; }

private:
    static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static void OnInitDialog(HWND hwnd);
    static void OnLogin(HWND hwnd);
    static void OnSignUp(HWND hwnd);
    static void OnSkip(HWND hwnd);
    static void ShowError(HWND hwnd, const wchar_t* msg);
    static void ClearError(HWND hwnd);
    static void SetLoading(HWND hwnd, bool loading);

    static std::wstring s_userEmail;
    static std::wstring s_userId;
    static bool s_loggedIn;
    static LoginResult s_result;
};
