## 2024-05-09 - Loading States for Async Actions
**Learning:** In vanilla JS without frameworks, handling loading states correctly is crucial for UX. Storing original text before updating it to "Loading..." and ensuring restoration happens in a `finally` block provides robust feedback and prevents UI from getting stuck if an API call fails.
**Action:** Always wrap async API calls in `try...finally` to ensure visual reset of buttons (disabling and text updates), regardless of success or failure.
