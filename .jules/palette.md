## 2024-05-18 - Added global :focus-visible styles
**Learning:** Found that most interactive elements stripped default focus outlines via `outline: none` without providing an alternative, breaking keyboard navigation accessibility.
**Action:** Adding a global `*:focus-visible` outline mapped to the theme's `--accent` color ensures universal keyboard accessibility while preserving the intended mouse/touch design. Always implement `:focus-visible` before stripping `:focus` outlines.
