I plan to add `aria-label` attributes to the buttons in `transparency-web/public/js/dashboard.js` that are missing them, which are the dynamic ones injected via JavaScript map strings (namely the "View Devices" and "Remove" buttons on the network cards).

The specific buttons are:

```html
      <button class="btn-sm btn-view" data-id="${id}" aria-label="View Devices for ${escHtml(net.name)}">View Devices</button>
      <button class="btn-sm btn-delete" data-id="${id}" aria-label="Remove network ${escHtml(net.name)}">Remove</button>
```

The `aria-label` helps screen readers read what exactly is being removed, because "Remove" isn't clear enough out of context, and same for "View Devices".

Since this aligns perfectly with Palette's persona (Add ARIA labels to buttons, keep changes under 50 lines), this feels like a perfect micro-UX enhancement!

I will also do a quick look at `index.html` to see if there are any other easy wins but I'm primarily focusing on the dynamic buttons.
