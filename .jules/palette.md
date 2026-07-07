## 2024-05-24 - Implicit Form Labels in Vanilla HTML/JS

**Learning:** When dealing with standard HTML/JS implementations without React or framework JSX, raw `<label>` elements frequently lack the explicit `for` attribute and instead rely on visual proximity to their input fields. Screen readers and automated accessibility tools cannot properly associate the input without this `for` attribute referencing the target `id`.

**Action:** Always manually audit and inject `for="targetId"` into `<label>` elements linked to inputs, avoiding linking labels to non-labelable elements like `<div>` buttons, to ensure strict compliance with Web Content Accessibility Guidelines (WCAG) and full screen reader operability.
## 2024-04-05 - Accessibility in Implicitly Labeled Forms
**Learning:** In vanilla JS apps, form structures often use generic `<div>` tags mapped with CSS classes (like `.setting-label`) alongside `aria-label` attributes to visually label form inputs without explicitly associating them via a `<label for="...">` element. While `aria-label` makes the input accessible to screen readers, missing an explicit `<label for="[id]">` prevents users from clicking the text label to focus the input, negatively impacting usability for mouse or touch users.
**Action:** Always ensure `<label>` tags explicitly declare the `for` attribute (linked to their target `<input>` ID) to support screen-reader functionality and increase the interactive clickable area for users.

## 2026-04-06 - Missing ARIA Labels on Interpolated JS Buttons
**Learning:** When vanilla JS apps interpolate HTML strings for UI components (e.g. `map().join('')`), it's common for icon-only buttons (like toolbars or quick actions) to lack `aria-label` attributes, since linters often cannot inspect dynamic string content as strictly as JSX.
**Action:** Always manually audit JS string templates injecting icon-only buttons to ensure they have descriptive `aria-label` attributes for screen readers.

## 2024-05-25 - Accessibility on Wrapper-Label Toggle Switches

**Learning:** When creating custom toggle switches in vanilla HTML/JS, developers often wrap the input element inside a `<label>` without a `for` attribute, using an adjacent `<span>` for visual text and knobs. While visually pleasing, this pattern fails screen readers because the implicit label relationship is broken or poorly announced when interacting with the hidden checkbox. Screen readers need an explicit `aria-label` directly on the `<input>` or a strict `<label for="id">` to properly voice the toggle's function and its checked/unchecked state.

**Action:** Always inject `aria-label` directly onto visually-hidden inputs embedded in toggle components (e.g. `<input type="checkbox" aria-label="Toggle feature">`), or refactor the wrapper to use an explicit `for` attribute matching the input's ID.
## 2026-07-07 - Handling Asynchronous UI States in Vanilla JS

**Learning:** When adding transient UI states (like loading spinners, changing button text, or disabling buttons) during asynchronous operations (e.g., login or signup submissions) in vanilla JavaScript without a reactive framework like React, it's critical to ensure these states are properly reset. If the operation fails (e.g., an API error or simple client-side validation failure), the UI can become permanently stuck in the "loading" or disabled state if the reset logic isn't robust.

**Action:** Always wrap the asynchronous logic in `try...catch...finally` blocks to guarantee the UI elements (like `submitBtn`) are re-enabled and restored to their original visual state (e.g., clearing inline styles like `opacity` and `cursor`, and reverting `textContent`) inside the `finally` block, ensuring a reliable user experience regardless of the operation's outcome.
