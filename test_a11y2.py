import re

with open("transparency-web/public/index.html", "r") as f:
    html = f.read()

# Check for icon-only buttons in sidebar
# Specifically <button class="nav-item"...>
nav_buttons = re.findall(r'<button class="nav-item"[^>]*>.*?</button>', html, re.DOTALL)
for btn in nav_buttons:
    print(btn)
