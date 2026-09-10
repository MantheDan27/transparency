## 2026-09-10 - Device Block Confirmation Dialog

**Learning:** Destructive actions (like blocking a network device) without explicit confirmation checks represent a significant UX/accessibility hazard. Users might accidentally trigger state-changing actions directly from action toolbars (`detail-actions`), leading to unexpected data loss or disconnection. Adding a native browser `confirm()` dialog provides a straightforward, robust, and accessible way to intercede before execution without requiring custom UI components or CSS overrides.

**Action:** Whenever introducing or reviewing destructive state-changing actions (e.g., Block, Delete, Reset), always verify that a confirmation mechanism (like a dialog or two-step flow) exists before the action is executed. Use native `confirm()` prompts when complex modal infrastructure is unavailable or outside scope boundaries.
