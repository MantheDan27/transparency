# Transparency
A native Windows desktop application for monitoring devices on your local network. Built with C++ and Win32 APIs — no runtime or installer required.

## Features

- **Multi-pass device discovery**: ARP sweep, ICMP ping, TCP probes, mDNS, SSDP, NetBIOS, IPv6 NDP
- **Multi-subnet scanning**: discovers devices across all local network interfaces
- **Device fingerprinting**: confidence scoring with explainability, OUI vendor lookup
- **Trust states**: Owned / Known / Guest / Unknown / Blocked / Watchlist
- **Custom labels**: names, tags, and notes per device
- **Scan modes**: Quick (~10s), Standard (~30s), Deep (~2 min) with optional gentle mode
- **Device history**: first/last seen, IP history, port history, latency sparklines, uptime estimate
- **Continuous monitoring**: configurable interval, quiet hours, internet outage detection
- **Alert rules**: IF/THEN conditions, webhook support, debounce, per-rule quiet hours
- **Event types**: new device, risky port, port changed, device offline, IP changed, internet outage, gateway MAC changed, DNS change, high latency
- **Diagnostic tools**: ping, traceroute, DNS lookup, TCP connect, HTTP test, guided troubleshooting
- **Privacy center**: data stats and retention controls, data wipe, export report
- **Wi-Fi info**: SSID, BSSID, channel, band, security type

## Download

Pre-built releases are available on the [Releases](../../releases) page. Download `Transparency.exe` and run it — no installation needed.

**Requirements**: Windows 10 or later (x64). Run as administrator for full ARP/ping functionality.

## Building from Source 

### Prerequisites

- Windows 10/11 (x64)
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with the **Desktop development with C++** workload
- [CMake](https://cmake.org/) 3.20 or later (included with Visual Studio)

### Build

```bat
git clone https://github.com/MantheDan27/transparency.git
cd transparency
cmake -B build -S transparency-cpp -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The executable is output to `build\Release\Transparency.exe`.

### Dependencies

All dependencies are Windows system libraries — no third-party packages required:

| Library | Purpose |
|---------|---------|
| `ws2_32` | Winsock networking |
| `iphlpapi` | ARP table, network interfaces |
| `comctl32` | Common controls (list view, etc.) |
| `uxtheme` | Visual styles |
| `dwmapi` | DWM window composition |
| `dnsapi` | DNS resolution |
| `gdi32` | GDI drawing |
| `wlanapi` | Wi-Fi information |
| `winmm` | Timer resolution |

## CI / Releases

GitHub Actions builds `Transparency.exe` on every push to `main` and on every tag push. Tagged releases (e.g. `v2.1.0`) are published automatically to GitHub Releases.

To create a release:

```bash
git tag v2.1.0
git push origin v2.1.0
```

## License

MIT
