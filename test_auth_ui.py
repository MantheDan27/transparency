from playwright.sync_api import sync_playwright
import time

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        context = browser.new_context()
        page = context.new_page()

        print("Navigating to file...")
        page.goto("http://localhost:8080")
        time.sleep(2)

        page.screenshot(path="auth_form.png")

        print("Clicking Sign Up...")
        page.locator("#signup-toggle").click()
        time.sleep(1)

        page.screenshot(path="signup_form.png")

        print("Done")
        browser.close()

if __name__ == "__main__":
    run()
