import re

with open("transparency-web/public/index.html", "r") as f:
    html = f.read()

# Check for buttons without aria-labels or title where they should have it
buttons = re.findall(r'<button[^>]*>.*?</button>', html, re.DOTALL)
for btn in buttons:
    if 'aria-label' not in btn and 'title' not in btn and not re.search(r'>\s*[a-zA-Z0-9]', btn):
        print("Potentially inaccessible button:", btn)

# Check for inputs without labels
inputs = re.findall(r'<input[^>]*>', html)
for inp in inputs:
    if 'id=' in inp:
        id_match = re.search(r'id="([^"]+)"', inp)
        if id_match:
            id_val = id_match.group(1)
            # check if there is a corresponding label with for="id_val"
            if f'for="{id_val}"' not in html and 'aria-label' not in inp:
                print("Input missing explicit label:", inp)
