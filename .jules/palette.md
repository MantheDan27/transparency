## 2024-05-24 - Implicit Form Labels in Vanilla HTML/JS

**Learning:** When dealing with standard HTML/JS implementations without React or framework JSX, raw `<label>` elements frequently lack the explicit `for` attribute and instead rely on visual proximity to their input fields. Screen readers and automated accessibility tools cannot properly associate the input without this `for` attribute referencing the target `id`.

**Action:** Always manually audit and inject `for="targetId"` into `<label>` elements linked to inputs, avoiding linking labels to non-labelable elements like `<div>` buttons, to ensure strict compliance with Web Content Accessibility Guidelines (WCAG) and full screen reader operability.
