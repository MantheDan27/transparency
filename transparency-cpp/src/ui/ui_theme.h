#pragma once

#include <cstdint>

// ─── Design System: Transparency ────────────────────────────────────────────
// Single source of truth for all design tokens. Platform backends convert
// these values to their native representations (COLORREF, GdkRGBA, etc.).
// Rule: NEVER hardcode colors, fonts, sizes, or spacing inline.

namespace theme {

// ── Backgrounds (4-layer depth system — never skip layers) ──
constexpr uint32_t bg_root      = 0xFF0A0C10;  // App background, sidebar
constexpr uint32_t bg_surface   = 0xFF12151C;  // Cards, panels, content area
constexpr uint32_t bg_elevated  = 0xFF1A1E28;  // Hover states, elevated cards
constexpr uint32_t bg_overlay   = 0xFF222838;  // Modals, dropdowns, popovers

// Derived background states
constexpr uint32_t bg_input     = 0xFF0A0C10;  // Inputs use root layer
constexpr uint32_t bg_row_alt   = 0xFF0E1116;  // Subtle alternation on root
constexpr uint32_t bg_row_hov   = 0xFF1A1E28;  // Hover = elevated layer
constexpr uint32_t bg_row_sel   = 0xFF18253E;  // accent_blue @ 15% on surface
constexpr uint32_t bg_nav_active= 0xFF121D34;  // accent_blue @ 15% on root

// ── Borders ──
constexpr uint32_t border_default = 0xFF2A3040;  // Card borders, dividers
constexpr uint32_t border_subtle  = 0xFF1E2330;  // Subtle separators
constexpr uint32_t border_focus   = 0xFF3D7FFF;  // Focus rings, active borders

// ── Text ──
constexpr uint32_t text_primary   = 0xFFE8ECF4;  // Headings, primary content
constexpr uint32_t text_secondary = 0xFF8892A8;  // Body text, descriptions
constexpr uint32_t text_tertiary  = 0xFF555E72;  // Disabled, hints, timestamps

// ── Accents (each has ONE semantic role — do not mix) ──
constexpr uint32_t accent_blue   = 0xFF3D7FFF;  // Primary actions, links, focus
constexpr uint32_t accent_cyan   = 0xFF00E5FF;  // Confidence scores, power user
constexpr uint32_t accent_green  = 0xFF00E57A;  // Trusted, healthy, success
constexpr uint32_t accent_amber  = 0xFFFFC832;  // Warning, caution, unknown
constexpr uint32_t accent_red    = 0xFFFF4060;  // Critical, blocked, destructive
constexpr uint32_t accent_purple = 0xFFA855F7;  // Premium, rare, watchlist

// ── Fonts ──
constexpr const char* font_sans = "Geist";       // Fallback: "Inter", "Segoe UI"
constexpr const char* font_mono = "Geist Mono";  // Fallback: "JetBrains Mono", "Consolas"

// ── Radii (in pixels) ──
constexpr int radius_sm   = 6;    // Badges, chips, small buttons
constexpr int radius_md   = 10;   // Cards, inputs, dropdowns
constexpr int radius_lg   = 14;   // Modals, panels
constexpr int radius_xl   = 20;   // Feature sections
constexpr int radius_full = 9999; // Pills, avatars, toggles

// ── Spacing (base-4 system — all multiples of 4) ──
constexpr int sp1  = 4;
constexpr int sp2  = 8;
constexpr int sp3  = 12;
constexpr int sp4  = 16;
constexpr int sp5  = 20;
constexpr int sp6  = 24;
constexpr int sp8  = 32;
constexpr int sp10 = 40;
constexpr int sp12 = 48;
constexpr int sp16 = 64;

// ── Layout Constants ──
constexpr int sidebar_w       = 260;   // Sidebar fixed width
constexpr int content_max_w   = 1200;  // Content area max width
constexpr int card_padding    = 20;    // Card inner padding
constexpr int grid_gap        = 16;    // Grid gap between cards
constexpr int page_padding    = 32;    // Page-level padding
constexpr int modal_max_w     = 520;   // Modal max width
constexpr int min_target      = 44;    // Minimum interactive target size

// ── Shadow definitions ──
struct Shadow {
    int offset_x;
    int offset_y;
    int blur;
    float alpha;  // 0.0-1.0
};

constexpr Shadow shadow_subtle = { 0, 1, 2,  0.30f };  // Cards at rest
constexpr Shadow shadow_medium = { 0, 4, 12, 0.40f };  // Hover / elevated
constexpr Shadow shadow_heavy  = { 0, 8, 32, 0.60f };  // Modals / overlays

// ── Type Scale ──
struct TypeStyle {
    int size_px;
    int weight;      // 400=Regular, 500=Medium, 600=SemiBold, 700=Bold
    float line_height; // multiplier
    float letter_spacing; // em
    bool mono;
};

constexpr TypeStyle type_display  = { 48, 700, 1.1f, -0.03f, false };
constexpr TypeStyle type_h1       = { 32, 700, 1.2f, -0.02f, false };
constexpr TypeStyle type_h2       = { 24, 600, 1.3f, -0.015f, false };
constexpr TypeStyle type_h3       = { 18, 600, 1.4f, -0.01f, false };
constexpr TypeStyle type_body     = { 15, 400, 1.6f,  0.0f,  false };
constexpr TypeStyle type_body_sm  = { 13, 400, 1.5f,  0.01f, false };
constexpr TypeStyle type_caption  = { 11, 500, 1.4f,  0.04f, false };
constexpr TypeStyle type_mono     = { 13, 400, 1.5f,  0.02f, true  };

// ── Helper: Alpha-blended accent color ──
inline uint32_t accent_with_alpha(uint32_t color, float alpha) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// ── Helper: Extract RGB channels ──
inline void extract_rgb(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
}

} // namespace theme
