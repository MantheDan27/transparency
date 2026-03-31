## 2024-05-14 - Add ARIA labels to tool inputs
**Learning:** Diagnostic tool inputs relied solely on placeholder text which is an accessibility anti-pattern for screen readers.
**Action:** Always ensure input fields without explicit `<label>` tags have descriptive `aria-label` attributes to improve screen reader accessibility.
