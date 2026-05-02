#include "TabTopology.h"
#include "MainWindow.h"
#include "Scanner.h"
#include <windowsx.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const wchar_t* TabTopology::s_className = L"TransparenciiTabTopology";

// ─── Create ──────────────────────────────────────────────────────────────────

bool TabTopology::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
    _mainWnd = mainWnd;

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hbrBackground = Theme::BrushSurface();
    wc.lpszClassName = s_className;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(0, s_className, nullptr,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, w, h,
        parent, (HMENU)(UINT_PTR)IDC_PANEL_TOPOLOGY,
        GetModuleHandle(nullptr), this);
    return hwnd != nullptr;
}

// ─── WndProc ─────────────────────────────────────────────────────────────────

LRESULT CALLBACK TabTopology::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabTopology* self = nullptr;
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<LPCREATESTRUCT>(lp);
        self = reinterpret_cast<TabTopology*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabTopology*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:        return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:          return self->OnSize(hwnd, LOWORD(lp), HIWORD(lp));
    case WM_PAINT:         return self->OnPaint(hwnd);
    case WM_ERASEBKGND:    return 1;
    case WM_MOUSEMOVE:     return self->OnMouseMove(hwnd, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
    case WM_LBUTTONDOWN:   return self->OnLButtonDown(hwnd, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
    case WM_COMMAND:       return self->OnCommand(hwnd, wp);
    case WM_SCAN_COMPLETE: return self->OnScanComplete(hwnd);
    case WM_MONITOR_TICK:  return self->OnMonitorTick(hwnd);
    case WM_TIMER:         return self->OnTimer(hwnd);
    case WM_SETCURSOR:     return 0; // let OnMouseMove set cursor
    default:               return DefWindowProc(hwnd, msg, wp, lp);
    }
}

// ─── Message handlers ────────────────────────────────────────────────────────

LRESULT TabTopology::OnCreate(HWND hwnd, LPCREATESTRUCT) {
    CreateWindowEx(0, L"BUTTON", L"Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        8, 6, 76, 26,
        hwnd, (HMENU)(UINT_PTR)BTN_REFRESH,
        GetModuleHandle(nullptr), nullptr);

    SetTimer(hwnd, 1, 120, nullptr);
    return 0;
}

LRESULT TabTopology::OnSize(HWND hwnd, int cx, int cy) {
    _totalW = cx;
    _totalH = cy;
    SetWindowPos(GetDlgItem(hwnd, BTN_REFRESH), nullptr, 8, 6, 76, 26, SWP_NOZORDER);
    BuildLayout(cx, cy);
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
}

LRESULT TabTopology::OnTimer(HWND hwnd) {
    _animTick = (_animTick + 1) % 16;
    bool monActive = _mainWnd && _mainWnd->_monitorActive;
    if (monActive) InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
}

LRESULT TabTopology::OnScanComplete(HWND hwnd) {
    BuildLayout(_totalW, _totalH);
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
}

LRESULT TabTopology::OnMonitorTick(HWND hwnd) {
    BuildLayout(_totalW, _totalH);
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
}

LRESULT TabTopology::OnCommand(HWND hwnd, WPARAM wp) {
    if (LOWORD(wp) == BTN_REFRESH) {
        BuildLayout(_totalW, _totalH);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
}

// ─── Layout ──────────────────────────────────────────────────────────────────

void TabTopology::BuildLayout(int totalW, int totalH) {
    _nodes.clear();
    _gatewayIdx = -1;
    if (!_mainWnd) return;

    ScanResult r = _mainWnd->GetLastResult();
    if (r.devices.empty()) return;

    // Resolve gateway IP from ranked NICs
    std::wstring gatewayIp;
    {
        auto nets = ScanEngine::RankNetworkInterfaces();
        if (!nets.empty()) gatewayIp = nets[0].gateway;
    }

    // Canvas extents (below toolbar, left of detail panel)
    int canvasW = totalW - DETAIL_W;
    int canvasH = totalH - TOOLBAR_H;
    if (canvasW < 80 || canvasH < 80) return;

    // Find gateway device in result
    int gwDevIdx = -1;
    for (int i = 0; i < (int)r.devices.size(); i++) {
        if (!gatewayIp.empty() && r.devices[i].ip == gatewayIp) { gwDevIdx = i; break; }
    }
    if (gwDevIdx < 0) {
        for (int i = 0; i < (int)r.devices.size(); i++) {
            const auto& dt = r.devices[i].deviceType;
            if (dt.find(L"Router") != std::wstring::npos ||
                dt.find(L"Gateway") != std::wstring::npos) { gwDevIdx = i; break; }
        }
    }

    // Separate gateway from peers
    std::vector<Device> others;
    Device gwDev;
    bool hasGw = false;
    for (int i = 0; i < (int)r.devices.size(); i++) {
        if (i == gwDevIdx) { gwDev = r.devices[i]; hasGw = true; }
        else               { others.push_back(r.devices[i]); }
    }

    // Sort: online first, then by trust priority
    auto trustRank = [](const std::wstring& ts) -> int {
        if (ts == L"owned")     return 0;
        if (ts == L"known")     return 1;
        if (ts == L"guest")     return 2;
        if (ts == L"watchlist") return 3;
        if (ts == L"blocked")   return 4;
        return 2; // unknown
    };
    std::stable_sort(others.begin(), others.end(), [&](const Device& a, const Device& b) {
        if (a.online != b.online) return a.online > b.online;
        return trustRank(a.trustState) < trustRank(b.trustState);
    });

    int N = (int)others.size();

    // Node radius scales down for crowded maps
    int nodeR = (N > 20) ? 10 : (N > 12) ? 12 : 14;

    // Compute ring radius — enough arc spacing between nodes
    double minSpacing = nodeR * 2.0 + 18.0;
    double minFromSpacing = (N > 0) ? (N * minSpacing) / (2.0 * M_PI) : 80.0;
    double maxFromCanvas  = (std::min(canvasW, canvasH) * 0.5 - nodeR - 70.0);
    double ringR = std::max(minFromSpacing, 80.0);
    if (maxFromCanvas > 80.0) ringR = std::min(ringR, maxFromCanvas);

    // Two concentric rings if one ring can't fit all nodes comfortably
    bool   twoRings   = false;
    int    innerCount = N;
    double innerR     = ringR;
    double outerR     = ringR;

    if (N > 1) {
        double maxInner = (2.0 * M_PI * ringR * 0.55) / minSpacing;
        if ((int)maxInner < N) {
            twoRings   = true;
            innerCount = std::max(1, (int)maxInner);
            innerR     = ringR * 0.55;
            outerR     = ringR;
        }
    }

    // Center of canvas (adjusted down slightly so labels clear the toolbar)
    int cx = canvasW / 2;
    int cy = TOOLBAR_H + canvasH / 2;

    // Gateway node at center
    NodeInfo gwNode;
    gwNode.cx        = cx;
    gwNode.cy        = cy;
    gwNode.radius    = 20;
    gwNode.isGateway = true;
    if (hasGw) {
        gwNode.dev = gwDev;
    } else {
        gwNode.dev.ip         = gatewayIp.empty() ? L"Router" : gatewayIp;
        gwNode.dev.deviceType = L"Router";
        gwNode.dev.trustState = L"owned";
        gwNode.dev.hostname   = L"Gateway";
        gwNode.dev.online     = true;
    }
    _nodes.push_back(gwNode);
    _gatewayIdx = 0;

    // Place other devices
    if (!twoRings) {
        for (int i = 0; i < N; i++) {
            double angle = -M_PI / 2.0 + (2.0 * M_PI * i) / std::max(N, 1);
            NodeInfo ni;
            ni.cx        = cx + (int)(ringR * std::cos(angle));
            ni.cy        = cy + (int)(ringR * std::sin(angle));
            ni.radius    = nodeR;
            ni.isGateway = false;
            ni.dev       = others[i];
            _nodes.push_back(ni);
        }
    } else {
        // Inner ring
        for (int i = 0; i < innerCount; i++) {
            double angle = -M_PI / 2.0 + (2.0 * M_PI * i) / innerCount;
            NodeInfo ni;
            ni.cx        = cx + (int)(innerR * std::cos(angle));
            ni.cy        = cy + (int)(innerR * std::sin(angle));
            ni.radius    = nodeR;
            ni.isGateway = false;
            ni.dev       = others[i];
            _nodes.push_back(ni);
        }
        // Outer ring
        int outerCount = N - innerCount;
        for (int i = 0; i < outerCount; i++) {
            double angle = -M_PI / 2.0 + (2.0 * M_PI * i) / outerCount;
            NodeInfo ni;
            ni.cx        = cx + (int)(outerR * std::cos(angle));
            ni.cy        = cy + (int)(outerR * std::sin(angle));
            ni.radius    = nodeR;
            ni.isGateway = false;
            ni.dev       = others[innerCount + i];
            _nodes.push_back(ni);
        }
    }

    // Clamp all nodes to canvas bounds
    for (auto& n : _nodes) {
        int margin = n.radius + 2;
        n.cx = std::max(margin, std::min(canvasW - margin, n.cx));
        n.cy = std::max(TOOLBAR_H + margin, std::min(totalH - margin - 20, n.cy));
    }

    // Clear stale selection/hover
    if (_selectedNode >= (int)_nodes.size()) _selectedNode = -1;
    if (_hoveredNode  >= (int)_nodes.size()) _hoveredNode  = -1;
}

// ─── Painting ────────────────────────────────────────────────────────────────

LRESULT TabTopology::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdcScreen = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int cxW = rc.right, cyH = rc.bottom;

    // Double-buffer
    HDC     hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp   = CreateCompatibleBitmap(hdcScreen, cxW, cyH);
    HBITMAP hOld   = (HBITMAP)SelectObject(hdcMem, hBmp);

    // Background
    FillRect(hdcMem, &rc, Theme::BrushSurface());

    // Toolbar strip
    RECT tbRc = { 0, 0, cxW, TOOLBAR_H };
    FillRect(hdcMem, &tbRc, Theme::BrushElevated());
    DrawToolbar(hdcMem, tbRc);

    // Toolbar / canvas separator
    {
        HPEN p = CreatePen(PS_SOLID, 1, Theme::BORDER_SUBTLE);
        HPEN o = (HPEN)SelectObject(hdcMem, p);
        MoveToEx(hdcMem, 0, TOOLBAR_H, nullptr);
        LineTo(hdcMem, cxW, TOOLBAR_H);
        SelectObject(hdcMem, o);
        DeleteObject(p);
    }

    // Canvas (left of detail panel)
    int canvasW = cxW - DETAIL_W;
    RECT canvasRc = { 0, TOOLBAR_H, canvasW, cyH };
    DrawCanvas(hdcMem, canvasRc);

    // Canvas / detail panel separator
    {
        HPEN p = CreatePen(PS_SOLID, 1, Theme::BORDER_SUBTLE);
        HPEN o = (HPEN)SelectObject(hdcMem, p);
        MoveToEx(hdcMem, canvasW, TOOLBAR_H, nullptr);
        LineTo(hdcMem, canvasW, cyH);
        SelectObject(hdcMem, o);
        DeleteObject(p);
    }

    // Detail panel
    RECT detailRc = { canvasW, TOOLBAR_H, cxW, cyH };
    FillRect(hdcMem, &detailRc, Theme::BrushElevated());
    DrawDetailPanel(hdcMem, detailRc);

    BitBlt(hdcScreen, 0, 0, cxW, cyH, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    EndPaint(hwnd, &ps);
    return 0;
}

// ─── Toolbar ─────────────────────────────────────────────────────────────────

void TabTopology::DrawToolbar(HDC hdc, const RECT& rc) {
    SetBkMode(hdc, TRANSPARENT);
    HFONT hSaved = (HFONT)SelectObject(hdc, Theme::FontCaption());

    // Legend items
    struct { COLORREF color; const wchar_t* label; BYTE alpha; } items[] = {
        { Theme::ACCENT_BLUE,   L"Gateway",   255 },
        { Theme::ACCENT_GREEN,  L"Trusted",   255 },
        { Theme::ACCENT_AMBER,  L"Unknown",   255 },
        { Theme::ACCENT_RED,    L"Blocked",   255 },
        { Theme::ACCENT_PURPLE, L"Watchlist", 255 },
        { Theme::TEXT_TERTIARY, L"Offline",   140 },
    };

    int lx = 96; // leave room for Refresh button
    for (auto& item : items) {
        // Dot
        {
            Gdiplus::Graphics g(hdc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::SolidBrush b(Theme::GdipColor(item.color, item.alpha));
            g.FillEllipse(&b, (Gdiplus::REAL)lx, (Gdiplus::REAL)(rc.top + 14), 9.0f, 9.0f);
        }
        // Label
        SetTextColor(hdc, item.color);
        RECT tr = { lx + 13, rc.top + 11, lx + 90, rc.bottom - 6 };
        DrawText(hdc, item.label, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SIZE sz; GetTextExtentPoint32(hdc, item.label, (int)wcslen(item.label), &sz);
        lx += 13 + sz.cx + 14;
    }

    // Monitor pulse indicator (top-right)
    bool monActive = _mainWnd && _mainWnd->_monitorActive;
    if (monActive) {
        COLORREF pulse = (_animTick % 4 < 2) ? Theme::ACCENT_GREEN :
            Theme::AlphaBlend(Theme::ACCENT_GREEN, Theme::BG_ELEVATED, 55);
        {
            Gdiplus::Graphics g(hdc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::SolidBrush b(Theme::GdipColor(pulse));
            g.FillEllipse(&b, (Gdiplus::REAL)(rc.right - 118), (Gdiplus::REAL)(rc.top + 14), 9.0f, 9.0f);
        }
        SetTextColor(hdc, Theme::ACCENT_GREEN);
        RECT mr = { rc.right - 105, rc.top + 11, rc.right - 4, rc.bottom - 6 };
        DrawText(hdc, L"Live", -1, &mr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Last-scan timestamp (right-aligned, before monitor pill)
    if (_mainWnd) {
        ScanResult r = _mainWnd->GetLastResult();
        if (!r.scannedAt.empty()) {
            std::wstring ts = L"Scanned: " + r.scannedAt;
            SetTextColor(hdc, Theme::TEXT_TERTIARY);
            int rightEdge = monActive ? rc.right - 130 : rc.right - 8;
            RECT tr2 = { rc.left, rc.top, rightEdge, rc.bottom };
            DrawText(hdc, ts.c_str(), -1, &tr2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    SelectObject(hdc, hSaved);
}

// ─── Canvas ──────────────────────────────────────────────────────────────────

void TabTopology::DrawCanvas(HDC hdc, const RECT& rc) {
    if (_nodes.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        HFONT hSaved = (HFONT)SelectObject(hdc, Theme::FontH3());
        DrawText(hdc, L"No scan data \x2014 run a scan to map your network",
            -1, const_cast<RECT*>(&rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, hSaved);
        return;
    }

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

    bool monActive = _mainWnd && _mainWnd->_monitorActive;

    // 1. Draw links (edges) from gateway to each device
    if (_gatewayIdx >= 0) {
        const NodeInfo& gw = _nodes[_gatewayIdx];
        for (int i = 0; i < (int)_nodes.size(); i++) {
            if (i == _gatewayIdx) continue;
            DrawLink(g, gw, _nodes[i]);
        }

        // 2. Animated traffic-flow pulses when monitor is active
        if (monActive) {
            for (int i = 1; i < (int)_nodes.size(); i++) {
                const NodeInfo& n = _nodes[i];
                if (!n.dev.online) continue;
                // Stagger pulse phase per node index
                float phase = (float)((_animTick + i * 3) % 16) / 16.0f;
                float px = (float)gw.cx + ((float)n.cx - gw.cx) * phase;
                float py = (float)gw.cy + ((float)n.cy - gw.cy) * phase;
                Gdiplus::SolidBrush pBrush(Theme::GdipColor(NodeColor(n.dev), 210));
                g.FillEllipse(&pBrush, px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            }
        }
    }

    // 3. Draw nodes (gateway on top: draw others first, then gateway)
    for (int i = 1; i < (int)_nodes.size(); i++) {
        DrawNode(g, hdc, _nodes[i], i == _hoveredNode, i == _selectedNode);
    }
    if (_gatewayIdx >= 0) {
        DrawNode(g, hdc, _nodes[_gatewayIdx],
            _gatewayIdx == _hoveredNode, _gatewayIdx == _selectedNode);
    }

    // 4. Device count watermark (bottom-left of canvas)
    {
        SetBkMode(hdc, TRANSPARENT);
        HFONT hSaved = (HFONT)SelectObject(hdc, Theme::FontCaption());
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        int onlineCount = 0;
        for (auto& n : _nodes) if (n.dev.online && !n.isGateway) onlineCount++;
        std::wstring countStr = std::to_wstring((int)_nodes.size() - 1) + L" devices  \x2022  "
            + std::to_wstring(onlineCount) + L" online";
        RECT cr = { rc.left + 10, rc.bottom - 20, rc.right - 4, rc.bottom - 4 };
        DrawText(hdc, countStr.c_str(), -1, &cr, DT_LEFT | DT_SINGLELINE);
        SelectObject(hdc, hSaved);
    }
}

// ─── Link ────────────────────────────────────────────────────────────────────

void TabTopology::DrawLink(Gdiplus::Graphics& g, const NodeInfo& from, const NodeInfo& to) {
    BYTE   alpha = to.dev.online ? 75 : 28;
    COLORREF col = to.dev.online ? Theme::ACCENT_BLUE : Theme::TEXT_TERTIARY;
    float  lineW = to.dev.online ? 1.4f : 0.7f;

    Gdiplus::Pen pen(Theme::GdipColor(col, alpha), lineW);
    // Dashed line for offline devices
    if (!to.dev.online) {
        Gdiplus::REAL dash[] = { 4.0f, 4.0f };
        pen.SetDashPattern(dash, 2);
    }
    g.DrawLine(&pen, (float)from.cx, (float)from.cy, (float)to.cx, (float)to.cy);
}

// ─── Node ────────────────────────────────────────────────────────────────────

void TabTopology::DrawNode(Gdiplus::Graphics& g, HDC hdc,
                           const NodeInfo& n, bool hover, bool sel) {
    int    r   = n.radius;
    float  fx  = (float)(n.cx - r);
    float  fy  = (float)(n.cy - r);
    float  fd  = (float)(r * 2);
    COLORREF col = NodeColor(n.dev);
    BYTE fillAlpha = n.dev.online ? 255 : 90;

    // Drop shadow (online nodes only)
    if (n.dev.online) {
        Gdiplus::SolidBrush shadow(Gdiplus::Color(22, 0, 42, 108));
        g.FillEllipse(&shadow, fx + 2.0f, fy + 3.0f, fd, fd);
    }

    // Selection / hover ring
    if (sel) {
        Gdiplus::Pen selPen(Theme::GdipColor(col, 200), 2.5f);
        g.DrawEllipse(&selPen, fx - 5.0f, fy - 5.0f, fd + 10.0f, fd + 10.0f);
    } else if (hover) {
        Gdiplus::Pen hovPen(Theme::GdipColor(col, 120), 1.8f);
        g.DrawEllipse(&hovPen, fx - 4.0f, fy - 4.0f, fd + 8.0f, fd + 8.0f);
    }

    // Node fill
    if (n.isGateway) {
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::PointF(fx, fy), Gdiplus::PointF(fx + fd, fy + fd),
            Theme::GdipColor(Theme::ACCENT_BLUE, fillAlpha),
            Theme::GdipColor(Theme::ACCENT_CYAN,  fillAlpha));
        g.FillEllipse(&grad, fx, fy, fd, fd);
        Gdiplus::Pen border(Theme::GdipColor(Theme::ACCENT_BLUE, 200), 1.5f);
        g.DrawEllipse(&border, fx, fy, fd, fd);
    } else {
        Gdiplus::SolidBrush fill(Theme::GdipColor(col, fillAlpha));
        g.FillEllipse(&fill, fx, fy, fd, fd);
        // Darker border derived from fill color
        COLORREF borderCol = Theme::AlphaBlend(col, RGB(0, 0, 0), 75);
        Gdiplus::Pen border(Theme::GdipColor(borderCol, 180), 1.0f);
        g.DrawEllipse(&border, fx, fy, fd, fd);
    }

    // Icon letter inside node
    SetBkMode(hdc, TRANSPARENT);
    HFONT hSaved = (HFONT)SelectObject(hdc, n.isGateway ? Theme::FontBody() : Theme::FontCaption());
    // Gateway: white on blue; others: surface color (or muted for offline)
    SetTextColor(hdc,
        n.isGateway  ? RGB(255, 255, 255) :
        n.dev.online ? Theme::BG_ROOT :
                       Theme::TEXT_TERTIARY);
    RECT iconRc = { n.cx - r, n.cy - r, n.cx + r, n.cy + r };
    DrawText(hdc, DeviceIcon(n.dev), -1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hSaved);

    // Label below node (IP / name)
    std::wstring label = DeviceLabel(n.dev);
    HFONT hLabel = (HFONT)SelectObject(hdc, Theme::FontCaption());
    SetTextColor(hdc, n.dev.online ? Theme::TEXT_PRIMARY : Theme::TEXT_TERTIARY);
    RECT lbRc = { n.cx - 52, n.cy + r + 2, n.cx + 52, n.cy + r + 18 };
    DrawText(hdc, label.c_str(), -1, &lbRc,
        DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, hLabel);

    // Offline indicator: small X drawn over node
    if (!n.dev.online && !n.isGateway) {
        float xr = r * 0.38f;
        Gdiplus::Pen xp(Theme::GdipColor(Theme::TEXT_TERTIARY, 170), 1.5f);
        g.DrawLine(&xp, (float)n.cx - xr, (float)n.cy - xr,
                        (float)n.cx + xr, (float)n.cy + xr);
        g.DrawLine(&xp, (float)n.cx + xr, (float)n.cy - xr,
                        (float)n.cx - xr, (float)n.cy + xr);
    }
}

// ─── Detail panel ────────────────────────────────────────────────────────────

void TabTopology::DrawDetailPanel(HDC hdc, const RECT& rc) {
    SetBkMode(hdc, TRANSPARENT);

    if (_selectedNode < 0 || _selectedNode >= (int)_nodes.size()) {
        // Empty state
        HFONT hSaved = (HFONT)SelectObject(hdc, Theme::FontBodySm());
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        DrawText(hdc, L"Click a device\nto view details",
            -1, const_cast<RECT*>(&rc), DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        SelectObject(hdc, hSaved);
        return;
    }

    const Device& d = _nodes[_selectedNode].dev;
    int x  = rc.left + 14;
    int y  = rc.top  + 14;
    int rw = rc.right - rc.left - 28; // usable width

    // ── Device name ──────────────────────────────────────────────────────────
    HFONT hSaved = (HFONT)SelectObject(hdc, Theme::FontH3());
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    std::wstring name = !d.customName.empty() ? d.customName :
                        !d.hostname.empty()   ? d.hostname   : d.ip;
    RECT nameRc = { x, y, rc.right - 14, y + 24 };
    DrawText(hdc, name.c_str(), -1, &nameRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 28;

    // ── Online / trust pill ──────────────────────────────────────────────────
    COLORREF trustCol = NodeColor(d);
    const wchar_t* trustLabel = d.trustState.empty() ? L"Unknown" : d.trustState.c_str();
    Theme::DrawPillBadge(hdc, x, y, 80, 20, trustCol, trustLabel, Theme::FontCaption());
    if (!d.online) {
        Theme::DrawPillBadge(hdc, x + 86, y, 58, 20,
            Theme::TEXT_TERTIARY, L"Offline", Theme::FontCaption());
    }
    y += 30;

    // ── Separator ────────────────────────────────────────────────────────────
    {
        HPEN p = CreatePen(PS_SOLID, 1, Theme::BORDER_SUBTLE);
        HPEN o = (HPEN)SelectObject(hdc, p);
        MoveToEx(hdc, x, y, nullptr); LineTo(hdc, rc.right - 14, y);
        SelectObject(hdc, o); DeleteObject(p);
    }
    y += 10;

    // ── Field rows ───────────────────────────────────────────────────────────
    SelectObject(hdc, Theme::FontCaption());

    struct Row { const wchar_t* label; std::wstring value; };
    std::vector<Row> rows;
    rows.push_back({ L"IP",       d.ip });
    if (!d.mac.empty())        rows.push_back({ L"MAC",      d.mac });
    if (!d.vendor.empty())     rows.push_back({ L"Vendor",   d.vendor });
    if (!d.deviceType.empty()) rows.push_back({ L"Type",     d.deviceType });
    if (!d.subnet.empty())     rows.push_back({ L"Subnet",   d.subnet });
    if (d.latencyMs >= 0)      rows.push_back({ L"Latency",
                                    std::to_wstring(d.latencyMs) + L" ms" });
    if (!d.firstSeen.empty())  rows.push_back({ L"First seen", d.firstSeen });
    if (!d.lastSeen.empty())   rows.push_back({ L"Last seen",  d.lastSeen });
    rows.push_back({ L"Sightings", std::to_wstring(d.sightingCount) });

    for (auto& row : rows) {
        if (y + 16 > rc.bottom - 12) break;
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        RECT lbRc = { x, y, x + 68, y + 16 };
        DrawText(hdc, row.label, -1, &lbRc, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        RECT valRc = { x + 70, y, rc.right - 14, y + 16 };
        DrawText(hdc, row.value.c_str(), -1, &valRc,
            DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += 18;
    }

    // ── Open ports ───────────────────────────────────────────────────────────
    if (!d.openPorts.empty() && y + 24 < rc.bottom - 12) {
        y += 6;
        {
            HPEN p = CreatePen(PS_SOLID, 1, Theme::BORDER_SUBTLE);
            HPEN o = (HPEN)SelectObject(hdc, p);
            MoveToEx(hdc, x, y, nullptr); LineTo(hdc, rc.right - 14, y);
            SelectObject(hdc, o); DeleteObject(p);
        }
        y += 8;

        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        RECT phRc = { x, y, rc.right - 14, y + 16 };
        DrawText(hdc, L"Open Ports", -1, &phRc, DT_LEFT | DT_SINGLELINE);
        y += 20;

        int px = x;
        for (int port : d.openPorts) {
            if (y + 18 > rc.bottom - 12) break;
            std::wstring ps = std::to_wstring(port);
            SIZE sz;
            GetTextExtentPoint32(hdc, ps.c_str(), (int)ps.size(), &sz);
            int pw = sz.cx + 10;
            if (px + pw > rc.right - 14) { px = x; y += 20; }
            RECT pRc = { px, y, px + pw, y + 16 };
            Theme::DrawRoundedCard(hdc, pRc, 4,
                Theme::BG_SURFACE, Theme::BORDER_DEFAULT, 1);
            SetTextColor(hdc, Theme::ACCENT_CYAN);
            DrawText(hdc, ps.c_str(), -1, &pRc, DT_CENTER | DT_SINGLELINE);
            px += pw + 4;
        }
    }

    // ── Classification reason (evidence summary) ──────────────────────────────
    if (!d.classificationReason.empty() && y + 28 < rc.bottom - 12) {
        y += (d.openPorts.empty() ? 10 : 26);
        if (y + 28 < rc.bottom - 12) {
            HPEN p = CreatePen(PS_SOLID, 1, Theme::BORDER_SUBTLE);
            HPEN o = (HPEN)SelectObject(hdc, p);
            MoveToEx(hdc, x, y, nullptr); LineTo(hdc, rc.right - 14, y);
            SelectObject(hdc, o); DeleteObject(p);
            y += 8;
            SetTextColor(hdc, Theme::TEXT_TERTIARY);
            RECT erRc = { x, y, rc.right - 14, rc.bottom - 12 };
            DrawText(hdc, d.classificationReason.c_str(), -1, &erRc,
                DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
        }
    }

    SelectObject(hdc, hSaved);
}

// ─── Interaction ─────────────────────────────────────────────────────────────

LRESULT TabTopology::OnMouseMove(HWND hwnd, int x, int y) {
    int prev = _hoveredNode;
    _hoveredNode = HitTest(x, y);
    SetCursor(LoadCursor(nullptr, _hoveredNode >= 0 ? IDC_HAND : IDC_ARROW));
    if (_hoveredNode != prev) InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
}

LRESULT TabTopology::OnLButtonDown(HWND hwnd, int x, int y) {
    int prev = _selectedNode;
    _selectedNode = HitTest(x, y);
    if (_selectedNode != prev) InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
}

int TabTopology::HitTest(int x, int y) const {
    // Check non-gateway nodes first (drawn on top of links but below gateway)
    for (int i = (int)_nodes.size() - 1; i >= 0; i--) {
        const auto& n = _nodes[i];
        int dx = x - n.cx, dy = y - n.cy;
        int hr = n.radius + 6; // slightly enlarged hit area
        if (dx * dx + dy * dy <= hr * hr) return i;
    }
    return -1;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

COLORREF TabTopology::NodeColor(const Device& d) {
    if (!d.online) return Theme::TEXT_TERTIARY;
    const auto& ts = d.trustState;
    if (ts == L"owned" || ts == L"known") return Theme::ACCENT_GREEN;
    if (ts == L"guest")                   return Theme::ACCENT_CYAN;
    if (ts == L"blocked")                 return Theme::ACCENT_RED;
    if (ts == L"watchlist")               return Theme::ACCENT_PURPLE;
    return Theme::ACCENT_AMBER; // unknown
}

std::wstring TabTopology::DeviceLabel(const Device& d) {
    if (!d.customName.empty()) return d.customName;
    if (!d.hostname.empty())   return d.hostname;
    return d.ip;
}

const wchar_t* TabTopology::DeviceIcon(const Device& d) {
    if (d.isIPv6) return L"6";
    const auto& dt = d.deviceType;
    if (dt.find(L"Router")  != std::wstring::npos ||
        dt.find(L"Gateway") != std::wstring::npos) return L"R";
    if (dt.find(L"Phone")   != std::wstring::npos ||
        dt.find(L"Mobile")  != std::wstring::npos) return L"M";
    if (dt.find(L"Laptop")  != std::wstring::npos) return L"L";
    if (dt.find(L"Desktop") != std::wstring::npos ||
        dt.find(L"PC")      != std::wstring::npos) return L"C";
    if (dt.find(L"TV")      != std::wstring::npos ||
        dt.find(L"Smart")   != std::wstring::npos) return L"S";
    if (dt.find(L"Camera")  != std::wstring::npos) return L"V";
    if (dt.find(L"Printer") != std::wstring::npos) return L"P";
    if (dt.find(L"NAS")     != std::wstring::npos ||
        dt.find(L"Server")  != std::wstring::npos) return L"N";
    if (dt.find(L"Switch")  != std::wstring::npos ||
        dt.find(L"Hub")     != std::wstring::npos) return L"H";
    if (dt.find(L"Speaker") != std::wstring::npos ||
        dt.find(L"Audio")   != std::wstring::npos) return L"A";
    return L"D"; // generic device
}
