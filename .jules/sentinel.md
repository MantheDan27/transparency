## 2024-03-06 - Unquoted Path and Command Injection via CreateProcess

**Vulnerability:**
Passing unquoted strings containing spaces to `CreateProcess` (with `lpApplicationName` as `nullptr`) creates an "Unquoted Service Path" vulnerability, potentially leading to the execution of unintended binaries (e.g., `C:\Program.exe` instead of `C:\Program Files\...`). Additionally, if the path string is controllable by the user and unquoted, they can append arguments for command injection.

**Learning:**
`CreateProcess` requires the executable path to be explicitly wrapped in double quotes when passed as the command line argument if the path contains or might contain spaces. It is also critical to reject or sanitize strings that already contain double quotes to prevent an attacker from prematurely closing the quote and injecting trailing arguments.

**Prevention:**
Always wrap dynamically provided executable paths in double quotes: `std::wstring cmd = L"\"" + path + L"\"";`. Ensure the input does not already contain double quotes: `if (path.find(L"\"") != std::wstring::npos) return;`. Alternatively, if no arguments are needed, pass the exact path via the `lpApplicationName` argument instead of `lpCommandLine`.
## 2025-03-06 - Command Injection via Diagnostic Tools
**Vulnerability:** The `ping-host` and `traceroute-host` IPC handlers in `main.js`, and `pingHost`/`getMac` in `scanner.js` pass user-controlled input (`host`/`ip` parameters) directly into a shell command using `exec` without validation. For example: `ping -c 4 ${host}`. A user could enter `127.0.0.1; rm -rf /` or similar to execute arbitrary commands.
**Learning:** Even internal diagnostic tools must sanitize input. When `exec` or `execPromise` is used to invoke a system command, variables interpolated into the command string must be strictly validated to prevent shell injection, as these functions invoke a subshell which processes shell metacharacters.
**Prevention:** Use a whitelist-based validation approach (e.g., `^[a-zA-Z0-9.:-]+$`) to ensure only valid hostnames or IP addresses are passed into the command. Alternatively, use `child_process.execFile` or `child_process.spawn` which do not run a subshell and pass arguments directly, making them immune to shell injection.
