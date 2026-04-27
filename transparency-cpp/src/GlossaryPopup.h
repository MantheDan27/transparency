#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>

// Call once at application startup (before any tab is shown).
void RegisterGlossaryPopupClass(HINSTANCE hInst);

// Show a small themed explanation popup near the cursor.
// Destroys any previously open popup first.
void ShowGlossaryPopup(HWND rootWnd, const std::wstring& title, const std::wstring& body);

// Look up a term/acronym in the built-in glossary.
// Returns nullptr if the key is not in the database.
const wchar_t* LookupAcronym(const std::wstring& key);

// Escape & < > in text before embedding in a WC_LINK (SysLink) control.
std::wstring EscapeForLink(const std::wstring& s);

// Apply background-fill subclass to a WC_LINK control so it blends with
// the panel surface colour instead of showing the system default white.
void SubclassLinkCtrl(HWND hwnd);
