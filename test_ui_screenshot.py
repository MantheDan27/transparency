from playwright.sync_api import sync_playwright

def test():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()
        page.goto("file:///app/transparency-web/public/index.html")

        # Verify elements have the new aria labels
        network_filter = page.locator("#network-filter")
        assert network_filter.get_attribute("aria-label") == "Filter by network", "network-filter missing aria-label"

        device_search = page.locator("#device-search")
        assert device_search.get_attribute("aria-label") == "Search devices", "device-search missing aria-label"

        api_endpoint = page.locator("#setting-api-endpoint")
        assert api_endpoint.get_attribute("aria-label") == "API Endpoint", "setting-api-endpoint missing aria-label"

        # Screenshot the whole page without interacting with JS (since JS requires firebase modules not loading in file://)
        page.screenshot(path="screenshot.png")

        print("Tests passed!")
        browser.close()

if __name__ == "__main__":
    test()
