## 2024-05-24 - Missing ARIA Labels on Dynamically Generated UI Elements
**Learning:** Dynamically injected icon-only buttons (like `.tag-rm` generated via `.map().join('')` in `renderer.js`) often miss accessibility tags because they bypass standard templating linting. Screen readers read "multiply" for "×" which provides no context.
**Action:** Always audit inline string templates for interactive elements to ensure they include descriptive `aria-label` or `title` attributes (e.g. `aria-label="Remove tag ${t}"`).
