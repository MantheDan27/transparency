## 2024-05-18 - Added global :focus-visible styles
**Learning:** Found that most interactive elements stripped default focus outlines via `outline: none` without providing an alternative, breaking keyboard navigation accessibility.
**Action:** Adding a global `*:focus-visible` outline mapped to the theme's `--accent` color ensures universal keyboard accessibility while preserving the intended mouse/touch design. Always implement `:focus-visible` before stripping `:focus` outlines.
## 2026-03-23 - Manual aria-labels for injected HTML
**Learning:** In vanilla JS applications using string interpolation for rendering (e.g., `.map().join('')`), standard accessibility linters may not catch missing ARIA labels. Always manually audit injected HTML templates to ensure icon-only buttons include descriptive `aria-label` and `title` attributes.
**Action:** Manually inspect and add `aria-label` to all icon-only buttons inside JS string templates during UI implementation.
