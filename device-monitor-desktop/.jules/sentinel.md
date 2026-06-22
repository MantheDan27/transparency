## 2024-06-22 - Fixed Timing Attack Vulnerability in API Key Authentication
**Vulnerability:** The local API authentication mechanism compared the user-provided API key directly with the stored API key using a standard JavaScript string equality check (`reqKey !== apiKeyStore.key`). This is vulnerable to timing attacks.
**Learning:** String comparisons in Node.js short-circuit and return early, taking more or less time depending on how many characters match before a mismatch is found. This can allow an attacker to progressively guess the API key by measuring response times.
**Prevention:** Always use `crypto.timingSafeEqual` for comparing secrets like API keys, tokens, or passwords to ensure constant-time comparison, even when they mismatch.
