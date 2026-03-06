## 2024-03-06 - Unquoted Path and Command Injection via CreateProcess

**Vulnerability:**
Passing unquoted strings containing spaces to `CreateProcess` (with `lpApplicationName` as `nullptr`) creates an "Unquoted Service Path" vulnerability, potentially leading to the execution of unintended binaries (e.g., `C:\Program.exe` instead of `C:\Program Files\...`). Additionally, if the path string is controllable by the user and unquoted, they can append arguments for command injection.

**Learning:**
`CreateProcess` requires the executable path to be explicitly wrapped in double quotes when passed as the command line argument if the path contains or might contain spaces. It is also critical to reject or sanitize strings that already contain double quotes to prevent an attacker from prematurely closing the quote and injecting trailing arguments.

**Prevention:**
Always wrap dynamically provided executable paths in double quotes: `std::wstring cmd = L"\"" + path + L"\"";`. Ensure the input does not already contain double quotes: `if (path.find(L"\"") != std::wstring::npos) return;`. Alternatively, if no arguments are needed, pass the exact path via the `lpApplicationName` argument instead of `lpCommandLine`.
