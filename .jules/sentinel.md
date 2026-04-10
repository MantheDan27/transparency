## 2024-05-24 - Command Injection in Script Hooks

**Vulnerability:**
The `runScriptHooks` function in `device-monitor-desktop/main.js` was vulnerable to command injection. It concatenated unvalidated user input (`h.cmd`) and JSON stringified payloads directly into a string executed by `child_process.exec()`.

**Learning:**
String interpolation and shell execution via `child_process.exec()` must be strictly avoided when user-provided arguments are involved. Using regex parsing for basic CLI inputs is lightweight but handling sub-processes in Node.js always requires direct binary execution.

**Prevention:**
Use `child_process.execFile()` with an array of arguments, completely bypassing the system shell. Any data payloads should be piped to the child process via standard input (`stdin`) to decouple command logic from data completely.
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
## 2026-03-11 - Command Injection in IP Diagnostics Options

**Vulnerability:**
The `ShowDeviceContextMenu` command handlers in `transparency-cpp/src/TabDevices.cpp` for ping, traceroute, SSH, and reverse DNS directly append user-controlled data (`dev.ip`) into a command string passed to `_wsystem`. A spoofed IP address or manipulated state could contain shell characters (e.g., `&`, `|`, `"`), leading to arbitrary command execution on the host machine.

**Learning:**
Even if data originates from network scanning contexts (like an IP address variable), it should not be implicitly trusted or directly interpolated into system shell calls without strict validation or sanitation. `_wsystem` inherently invokes `cmd.exe`, which processes all shell meta-characters.

**Prevention:**
Always strictly validate data against an allowlist pattern before using it in a shell command string. For IP addresses, verify they contain only alphanumeric characters, dots, colons, and hyphens (as implemented via the `IsValidIP` helper). When possible, use `CreateProcess` or similar non-shell APIs with properly quoted arguments.
## 2024-05-18 - Prevent Command Injection via IP Parameters
**Vulnerability:** Weak IP validation (`IsValidIP`) used before calling `_wsystem` and `ShellExecute` allowed potential command injection if malicious payloads were supplied (e.g., via malformed network traffic).
**Learning:** Always use strict networking primitives like `InetPtonW` to validate IP addresses before passing them to the shell, rather than relying on weak regular expressions or simple character matching.
**Prevention:** Use `ScanEngine::IsSafeIP` uniformly for all IP-based system shell executions.

## 2024-05-28 - Incomplete HTML Escaping via DOM textContent

**Vulnerability:**
The `escHtml` function in `transparency-web/public/js/dashboard.js` relied on assigning text to a detached DOM element (`div.textContent = str;`) and then reading `div.innerHTML`. This approach correctly escapes `<`, `>`, and `&`, but fails to escape single (`'`) and double (`"`) quotes. Because `escHtml` is used extensively to interpolate user-controlled data (like device names or MAC addresses) into HTML attributes, an attacker could supply a payload containing quotes to break out of the attribute and inject arbitrary scripts (XSS).

**Learning:**
DOM-based text insertion (`textContent`) is designed for rendering safe text nodes, not for securely escaping strings meant to be embedded within HTML attributes. It lacks the context of HTML attributes, so it does not escape quotes, risking Cross-Site Scripting (XSS).

**Prevention:**
When manually escaping HTML for use in vanilla JS template literals (e.g., `.map().join('')`), always explicitly escape `&`, `<`, `>`, `"`, and `'` using strict regex string replacements or robust allowlists, rather than relying on `textContent`.
