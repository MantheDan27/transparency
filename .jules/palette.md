## 2024-05-24 - Implicit Form Labels in Vanilla HTML/JS

**Learning:** When dealing with standard HTML/JS implementations without React or framework JSX, raw `<label>` elements frequently lack the explicit `for` attribute and instead rely on visual proximity to their input fields. Screen readers and automated accessibility tools cannot properly associate the input without this `for` attribute referencing the target `id`.

**Action:** Always manually audit and inject `for="targetId"` into `<label>` elements linked to inputs, avoiding linking labels to non-labelable elements like `<div>` buttons, to ensure strict compliance with Web Content Accessibility Guidelines (WCAG) and full screen reader operability.
## 2024-04-05 - Accessibility in Implicitly Labeled Forms
**Learning:** In vanilla JS apps, form structures often use generic `<div>` tags mapped with CSS classes (like `.setting-label`) alongside `aria-label` attributes to visually label form inputs without explicitly associating them via a `<label for="...">` element. While `aria-label` makes the input accessible to screen readers, missing an explicit `<label for="[id]">` prevents users from clicking the text label to focus the input, negatively impacting usability for mouse or touch users.
**Action:** Always ensure `<label>` tags explicitly declare the `for` attribute (linked to their target `<input>` ID) to support screen-reader functionality and increase the interactive clickable area for users.

## 2026-04-06 - Missing ARIA Labels on Interpolated JS Buttons
**Learning:** When vanilla JS apps interpolate HTML strings for UI components (e.g. `map().join('')`), it's common for icon-only buttons (like toolbars or quick actions) to lack `aria-label` attributes, since linters often cannot inspect dynamic string content as strictly as JSX.
**Action:** Always manually audit JS string templates injecting icon-only buttons to ensure they have descriptive `aria-label` attributes for screen readers.

## 2026-04-22 - Missing ARIA Labels on Custom Toggle Switches
**Learning:** In vanilla HTML implementations where custom toggle switches are built using a visually hidden `<input type="checkbox">` wrapped inside a `<label>`, the input itself is often missing an `aria-label`. Without a `for` attribute on the label explicitly tying it to the input ID, screen readers will announce these interactive checkbox inputs as "unlabeled checkbox".
**Action:** Always ensure that visually hidden inputs inside custom UI wrappers (like toggle switches) receive an explicit `aria-label` matching the visual text of the toggle, ensuring proper screen reader announcements when users tab to them.
