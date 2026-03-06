## 2024-05-24 - Command Injection in Script Hooks

**Vulnerability:**
The `runScriptHooks` function in `device-monitor-desktop/main.js` was vulnerable to command injection. It concatenated unvalidated user input (`h.cmd`) and JSON stringified payloads directly into a string executed by `child_process.exec()`.

**Learning:**
String interpolation and shell execution via `child_process.exec()` must be strictly avoided when user-provided arguments are involved. Using regex parsing for basic CLI inputs is lightweight but handling sub-processes in Node.js always requires direct binary execution.

**Prevention:**
Use `child_process.execFile()` with an array of arguments, completely bypassing the system shell. Any data payloads should be piped to the child process via standard input (`stdin`) to decouple command logic from data completely.
