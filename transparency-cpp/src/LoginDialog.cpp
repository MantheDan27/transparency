#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "LoginDialog.h"
#include "FirebaseClient.h"
#include "Theme.h"
#include <commctrl.h>

// Static members
std::wstring LoginDialog::s_userEmail;
std::wstring LoginDialog::s_userId;
bool LoginDialog::s_loggedIn = false;
LoginResult LoginDialog::s_result = LoginResult::Cancelled;

// Control IDs
#define IDC_EMAIL       1001
#define IDC_PASSWORD    1002
#define IDC_BTN_LOGIN   1003
#define IDC_BTN_SIGNUP  1004
#define IDC_BTN_SKIP    1005
#define IDC_ERROR_LABEL 1006
#define IDC_MODE_TOGGLE 1007

static bool s_signUpMode = false;

LoginResult LoginDialog::Show(HWND parent) {
    s_result = LoginResult::Cancelled;
    s_loggedIn = false;
    s_userEmail.clear();
    s_userId.clear();
    s_signUpMode = false;

    // Register dialog class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefDlgProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hbrBackground = Theme::BrushCard();
    wc.lpszClassName = L"TransparencyLoginDlg";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    // Create dialog window
    int dlgW = 420, dlgH = 380;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int dlgX = (screenW - dlgW) / 2;
    int dlgY = (screenH - dlgH) / 2;

    HWND hwnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"TransparencyLoginDlg",
        L"Transparency - Sign In",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        dlgX, dlgY, dlgW, dlgH,
        parent, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!hwnd) return LoginResult::Cancelled;

    Theme::SetDarkTitlebar(hwnd);
    SetWindowLongPtr(hwnd, DWLP_DLGPROC, (LONG_PTR)DlgProc);
    OnInitDialog(hwnd);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Modal message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(hwnd)) break;
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return s_result;
}

void LoginDialog::OnInitDialog(HWND hwnd) {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int cx = rc.right, cy = rc.bottom;

    int centerX = cx / 2;
    int y = 20;

    // Brand title
    HWND hBrand = CreateWindowEx(0, L"STATIC", L"Transparency",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, y, cx, 32, hwnd, nullptr, hInst, nullptr);
    SendMessage(hBrand, WM_SETFONT, (WPARAM)Theme::FontBrand(), TRUE);
    y += 36;

    // Tagline
    HWND hTag = CreateWindowEx(0, L"STATIC", L"Network Intelligence Platform",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, y, cx, 18, hwnd, nullptr, hInst, nullptr);
    SendMessage(hTag, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
    y += 36;

    // Mode toggle (Login / Sign Up)
    HWND hMode = CreateWindowEx(0, L"STATIC", L"Sign in to sync your data across devices",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, y, cx - 40, 20, hwnd, (HMENU)IDC_MODE_TOGGLE, hInst, nullptr);
    SendMessage(hMode, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    y += 32;

    // Email label
    HWND hEmailLbl = CreateWindowEx(0, L"STATIC", L"Email",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, y, cx - 80, 16, hwnd, nullptr, hInst, nullptr);
    SendMessage(hEmailLbl, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
    y += 18;

    // Email input
    HWND hEmail = CreateWindowEx(0, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        40, y, cx - 80, 28, hwnd, (HMENU)IDC_EMAIL, hInst, nullptr);
    SendMessage(hEmail, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(hEmail);
    y += 38;

    // Password label
    HWND hPwdLbl = CreateWindowEx(0, L"STATIC", L"Password",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        40, y, cx - 80, 16, hwnd, nullptr, hInst, nullptr);
    SendMessage(hPwdLbl, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
    y += 18;

    // Password input
    HWND hPwd = CreateWindowEx(0, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
        40, y, cx - 80, 28, hwnd, (HMENU)IDC_PASSWORD, hInst, nullptr);
    SendMessage(hPwd, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(hPwd);
    y += 42;

    // Error label (hidden by default)
    HWND hError = CreateWindowEx(0, L"STATIC", L"",
        WS_CHILD | SS_CENTER,
        40, y, cx - 80, 20, hwnd, (HMENU)IDC_ERROR_LABEL, hInst, nullptr);
    SendMessage(hError, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
    y += 26;

    // Login button
    HWND hLogin = CreateWindowEx(0, L"BUTTON", L"Sign In",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        40, y, cx - 80, 36, hwnd, (HMENU)IDC_BTN_LOGIN, hInst, nullptr);
    SendMessage(hLogin, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
    y += 44;

    // Sign up toggle
    HWND hSignUp = CreateWindowEx(0, L"BUTTON", L"Create Account",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        40, y, (cx - 90) / 2, 28, hwnd, (HMENU)IDC_BTN_SIGNUP, hInst, nullptr);
    SendMessage(hSignUp, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    // Skip button
    HWND hSkip = CreateWindowEx(0, L"BUTTON", L"Continue Offline",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        40 + (cx - 90) / 2 + 10, y, (cx - 90) / 2, 28, hwnd, (HMENU)IDC_BTN_SKIP, hInst, nullptr);
    SendMessage(hSkip, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    SetFocus(hEmail);
}

INT_PTR CALLBACK LoginDialog::DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case IDC_BTN_LOGIN:
            if (s_signUpMode)
                OnSignUp(hwnd);
            else
                OnLogin(hwnd);
            return TRUE;

        case IDC_BTN_SIGNUP:
            // Toggle between login and signup mode
            s_signUpMode = !s_signUpMode;
            {
                HWND hMode = GetDlgItem(hwnd, IDC_MODE_TOGGLE);
                HWND hBtn = GetDlgItem(hwnd, IDC_BTN_LOGIN);
                HWND hToggle = GetDlgItem(hwnd, IDC_BTN_SIGNUP);
                if (s_signUpMode) {
                    SetWindowText(hMode, L"Create a new account");
                    SetWindowText(hBtn, L"Create Account");
                    SetWindowText(hToggle, L"Back to Sign In");
                } else {
                    SetWindowText(hMode, L"Sign in to sync your data across devices");
                    SetWindowText(hBtn, L"Sign In");
                    SetWindowText(hToggle, L"Create Account");
                }
            }
            ClearError(hwnd);
            return TRUE;

        case IDC_BTN_SKIP:
            OnSkip(hwnd);
            return TRUE;

        case IDCANCEL:
            s_result = LoginResult::Cancelled;
            DestroyWindow(hwnd);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        HWND hCtrl = (HWND)lp;
        int id = GetDlgCtrlID(hCtrl);

        SetBkMode(hdc, TRANSPARENT);
        if (id == IDC_ERROR_LABEL) {
            SetTextColor(hdc, Theme::DANGER);
        } else if (id == IDC_MODE_TOGGLE) {
            SetTextColor(hdc, Theme::TEXT_SECONDARY);
        } else {
            SetTextColor(hdc, Theme::TEXT_PRIMARY);
        }
        SetBkColor(hdc, Theme::BG_CARD);
        return (INT_PTR)Theme::BrushCard();
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_INPUT);
        static HBRUSH inputBr = CreateSolidBrush(Theme::BG_INPUT);
        return (INT_PTR)inputBr;
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_CARD);
        return (INT_PTR)Theme::BrushCard();
    }

    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, Theme::BrushCard());
        return 1;
    }

    case WM_CLOSE:
        s_result = LoginResult::Cancelled;
        DestroyWindow(hwnd);
        return TRUE;

    case WM_DESTROY:
        PostQuitMessage(0);
        return TRUE;
    }

    return FALSE;
}

void LoginDialog::OnLogin(HWND hwnd) {
    ClearError(hwnd);

    wchar_t email[256] = {};
    wchar_t password[256] = {};
    GetDlgItemText(hwnd, IDC_EMAIL, email, 256);
    GetDlgItemText(hwnd, IDC_PASSWORD, password, 256);

    if (wcslen(email) == 0) {
        ShowError(hwnd, L"Please enter your email address.");
        return;
    }
    if (wcslen(password) == 0) {
        ShowError(hwnd, L"Please enter your password.");
        return;
    }

    SetLoading(hwnd, true);

    FirebaseClient& fb = GetFirebase();
    bool success = fb.SignInWithEmail(email, password);

    SetLoading(hwnd, false);

    if (success) {
        s_loggedIn = true;
        s_userEmail = email;
        s_userId = fb.GetUserId();
        s_result = LoginResult::LoggedIn;
        DestroyWindow(hwnd);
    } else {
        std::wstring err = fb.GetLastError();
        if (err.empty()) err = L"Sign in failed. Please check your credentials.";
        ShowError(hwnd, err.c_str());
    }
}

void LoginDialog::OnSignUp(HWND hwnd) {
    ClearError(hwnd);

    wchar_t email[256] = {};
    wchar_t password[256] = {};
    GetDlgItemText(hwnd, IDC_EMAIL, email, 256);
    GetDlgItemText(hwnd, IDC_PASSWORD, password, 256);

    if (wcslen(email) == 0) {
        ShowError(hwnd, L"Please enter your email address.");
        return;
    }
    if (wcslen(password) < 6) {
        ShowError(hwnd, L"Password must be at least 6 characters.");
        return;
    }

    SetLoading(hwnd, true);

    FirebaseClient& fb = GetFirebase();
    bool success = fb.SignUp(email, password);

    SetLoading(hwnd, false);

    if (success) {
        s_loggedIn = true;
        s_userEmail = email;
        s_userId = fb.GetUserId();
        s_result = LoginResult::LoggedIn;
        DestroyWindow(hwnd);
    } else {
        std::wstring err = fb.GetLastError();
        if (err.empty()) err = L"Account creation failed. Please try again.";
        ShowError(hwnd, err.c_str());
    }
}

void LoginDialog::OnSkip(HWND hwnd) {
    int result = MessageBox(hwnd,
        L"Without signing in:\n\n"
        L"  - Device settings will not be saved to the cloud\n"
        L"  - Data won't sync across devices\n"
        L"  - You may lose your data if you reinstall\n\n"
        L"You can sign in later from the Settings.\n\n"
        L"Continue without signing in?",
        L"Continue Offline",
        MB_YESNO | MB_ICONWARNING);

    if (result == IDYES) {
        s_loggedIn = false;
        s_result = LoginResult::Skipped;
        DestroyWindow(hwnd);
    }
}

void LoginDialog::ShowError(HWND hwnd, const wchar_t* msg) {
    HWND hError = GetDlgItem(hwnd, IDC_ERROR_LABEL);
    if (hError) {
        SetWindowText(hError, msg);
        ShowWindow(hError, SW_SHOW);
    }
}

void LoginDialog::ClearError(HWND hwnd) {
    HWND hError = GetDlgItem(hwnd, IDC_ERROR_LABEL);
    if (hError) {
        SetWindowText(hError, L"");
        ShowWindow(hError, SW_HIDE);
    }
}

void LoginDialog::SetLoading(HWND hwnd, bool loading) {
    HWND hLogin = GetDlgItem(hwnd, IDC_BTN_LOGIN);
    HWND hSignUp = GetDlgItem(hwnd, IDC_BTN_SIGNUP);
    HWND hSkip = GetDlgItem(hwnd, IDC_BTN_SKIP);
    HWND hEmail = GetDlgItem(hwnd, IDC_EMAIL);
    HWND hPwd = GetDlgItem(hwnd, IDC_PASSWORD);

    EnableWindow(hLogin, !loading);
    EnableWindow(hSignUp, !loading);
    EnableWindow(hSkip, !loading);
    EnableWindow(hEmail, !loading);
    EnableWindow(hPwd, !loading);

    if (loading) {
        SetWindowText(hLogin, s_signUpMode ? L"Creating..." : L"Signing in...");
    } else {
        SetWindowText(hLogin, s_signUpMode ? L"Create Account" : L"Sign In");
    }
}
