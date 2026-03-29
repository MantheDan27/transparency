## 2024-05-18 - Added global :focus-visible styles
**Learning:** Found that most interactive elements stripped default focus outlines via `outline: none` without providing an alternative, breaking keyboard navigation accessibility.
**Action:** Adding a global `*:focus-visible` outline mapped to the theme's `--accent` color ensures universal keyboard accessibility while preserving the intended mouse/touch design. Always implement `:focus-visible` before stripping `:focus` outlines.

## 2024-05-18 - Missing ARIA labels in dynamic HTML templates
**Learning:** In vanilla JS applications using string interpolation for rendering (e.g., `.map().join('')`), standard accessibility linters or static analysis might not catch missing ARIA labels on dynamic elements like checkboxes or icon-only buttons (like tag removal 'x' buttons).
**Action:** Always manually audit injected HTML templates, especially for interactive elements (inputs, icon-only buttons) to ensure they include descriptive `aria-label` and `title` attributes.
