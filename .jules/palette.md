## $(date +%Y-%m-%d) - Adding ARIA labels to map controls
**Learning:** Found that map controls (zoom in, zoom out, reset view) had `title` attributes but were missing explicit `aria-label`s for screen readers, a common a11y pattern that needs to be addressed for icon-only buttons.
**Action:** Ensure all icon-only buttons have explicit `aria-label` attributes for screen readers, rather than relying solely on `title` attributes.
