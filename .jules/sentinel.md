## 2026-04-05 - Incomplete HTML Escaping in Custom Sanitizer
**Vulnerability:** XSS via unescaped single quotes in `escHtml`.
**Learning:** Custom HTML sanitization functions often forget to escape single quotes (`'`). When user input is interpolated into HTML attributes bounded by single quotes or used in inline event handlers, this leads to XSS.
**Prevention:** Always use robust, established sanitization libraries when possible. If a custom escaping function must be used, ensure it explicitly escapes `&`, `<`, `>`, `"`, and `'`.
