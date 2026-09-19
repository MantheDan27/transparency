## 2026-09-19 - Tooltips on dynamically disabled buttons
**Learning:** Native `title` tooltips intended for disabled HTML elements must be applied and removed dynamically in JavaScript alongside the `disabled` state itself. If hardcoded into the static HTML, they will misleadingly persist when the element becomes enabled and active.
**Action:** When adding UX tooltips to elements controlled by JS logic (like "Select an item to enable"), update the corresponding JS event listeners to toggle the `title` attribute dynamically to match the element's interactive state.
