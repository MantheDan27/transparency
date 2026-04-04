## 2024-05-18 - Added global :focus-visible styles
**Learning:** Found that most interactive elements stripped default focus outlines via `outline: none` without providing an alternative, breaking keyboard navigation accessibility.
**Action:** Adding a global `*:focus-visible` outline mapped to the theme's `--accent` color ensures universal keyboard accessibility while preserving the intended mouse/touch design. Always implement `:focus-visible` before stripping `:focus` outlines.
## 2024-04-02 - Testing Local HTML Files with Playwright
**Learning:** When visually verifying frontend changes in static web projects (like `device-monitor-desktop` or `transparency-web`) using Playwright, static HTML modifications that don't require an active build step can be tested quickly by navigating directly to the local file (e.g., `page.goto("file:///app/transparency-web/public/index.html")`).
**Action:** Use `file:///app/...` in Playwright verification scripts to quickly verify HTML structural changes without setting up a local dev server, saving time on micro-UX enhancements.
