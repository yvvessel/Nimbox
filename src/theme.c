#include "theme.h"

static const ThemePalette s_palette_dark = {
    .type = THEME_DARK,
    .name = L"Escuro",

    // Fundo escuro moderno (#18181B / #222228 / #1E1E24)
    .bg_window      = RGB(24, 24, 27),
    .bg_surface     = RGB(34, 34, 40),
    .bg_surface_alt = RGB(30, 30, 36),
    .bg_hover       = RGB(48, 48, 56),
    .bg_active      = RGB(60, 60, 70),

    // Bordas suaves
    .border_normal  = RGB(54, 54, 62),
    .border_hover   = RGB(78, 78, 90),
    .border_focus   = RGB(59, 130, 246),

    // Tipografia de alto contraste suave (#E4E4E7 / #A1A1AA)
    .text_primary   = RGB(228, 228, 231),
    .text_secondary = RGB(161, 161, 170),
    .text_muted     = RGB(113, 113, 122),

    // Azul de destaque (#3B82F6)
    .accent         = RGB(59, 130, 246),
    .accent_hover   = RGB(37, 99, 235),
    .accent_active  = RGB(29, 78, 216),
    .accent_text    = RGB(255, 255, 255),

    // Badge Ativo (Verde esmeralda escuro)
    .status_active_bg     = RGB(16, 48, 36),
    .status_active_fg     = RGB(74, 222, 128),
    .status_active_border = RGB(22, 101, 52),

    // Badge Pausado (Vermelho suave)
    .status_paused_bg        = RGB(64, 24, 28),
    .status_paused_hover_bg  = RGB(82, 30, 35),
    .status_paused_active_bg = RGB(98, 36, 42),
    .status_paused_fg        = RGB(248, 113, 113),
    .status_paused_border    = RGB(153, 27, 27),

    // Acoes perigosas
    .danger_fg           = RGB(248, 113, 113),
    .danger_hover_bg     = RGB(68, 28, 33),
    .danger_active_bg    = RGB(88, 34, 40),
    .danger_hover_border = RGB(220, 68, 68),

    // Tabela de snippets (Tema Escuro confortavel)
    .table_bg                  = RGB(24, 24, 27),
    .table_header_bg           = RGB(30, 30, 35),
    .table_row                 = RGB(32, 32, 36),
    .table_row_alt             = RGB(28, 28, 32),
    .table_row_hover           = RGB(41, 41, 50),
    .table_row_selected        = RGB(30, 48, 78),
    .table_row_selected_border = RGB(59, 130, 246),
    .table_separator           = RGB(48, 48, 54),
    .table_border              = RGB(48, 48, 54),
    .badge_bg                  = RGB(39, 39, 46),
    .badge_border              = RGB(58, 58, 68),
    .badge_fg                  = RGB(147, 197, 253),
    .badge_selected_bg         = RGB(37, 61, 100),
    .badge_selected_border     = RGB(71, 114, 180),
    .scrollbar_thumb           = RGB(63, 63, 74),
    .scrollbar_thumb_hover     = RGB(94, 94, 108),

    .is_dark = TRUE
};

static const ThemePalette s_palette_light = {
    .type = THEME_LIGHT,
    .name = L"Claro",

    // Fundo claro moderno (#F4F6F9 / #FFFFFF / #F8FAFC)
    .bg_window      = RGB(244, 246, 249),
    .bg_surface     = RGB(255, 255, 255),
    .bg_surface_alt = RGB(248, 250, 252),
    .bg_hover       = RGB(238, 242, 246),
    .bg_active      = RGB(226, 232, 240),

    // Bordas suaves
    .border_normal  = RGB(214, 221, 230),
    .border_hover   = RGB(168, 179, 194),
    .border_focus   = RGB(37, 99, 235),

    // Tipografia escura de excelente legibilidade
    .text_primary   = RGB(24, 33, 47),
    .text_secondary = RGB(100, 116, 139),
    .text_muted     = RGB(148, 163, 184),

    // Azul de destaque (#2563EB)
    .accent         = RGB(37, 99, 235),
    .accent_hover   = RGB(29, 78, 216),
    .accent_active  = RGB(30, 64, 175),
    .accent_text    = RGB(255, 255, 255),

    // Badge Ativo (Verde claro suave)
    .status_active_bg     = RGB(220, 252, 231),
    .status_active_fg     = RGB(21, 128, 61),
    .status_active_border = RGB(134, 239, 172),

    // Badge Pausado (Vermelho claro suave)
    .status_paused_bg        = RGB(254, 226, 226),
    .status_paused_hover_bg  = RGB(254, 208, 208),
    .status_paused_active_bg = RGB(252, 185, 185),
    .status_paused_fg        = RGB(185, 28, 28),
    .status_paused_border    = RGB(252, 165, 165),

    // Acoes perigosas
    .danger_fg           = RGB(220, 38, 38),
    .danger_hover_bg     = RGB(254, 226, 226),
    .danger_active_bg    = RGB(254, 202, 202),
    .danger_hover_border = RGB(239, 68, 68),

    // Tabela de snippets (Tema Claro moderno)
    .table_bg                  = RGB(255, 255, 255),
    .table_header_bg           = RGB(248, 250, 252),
    .table_row                 = RGB(255, 255, 255),
    .table_row_alt             = RGB(250, 251, 253),
    .table_row_hover           = RGB(241, 245, 249),
    .table_row_selected        = RGB(232, 241, 255),
    .table_row_selected_border = RGB(37, 99, 235),
    .table_separator           = RGB(232, 237, 243),
    .table_border              = RGB(218, 225, 234),
    .badge_bg                  = RGB(241, 245, 249),
    .badge_border              = RGB(218, 225, 234),
    .badge_fg                  = RGB(29, 78, 216),
    .badge_selected_bg         = RGB(214, 229, 255),
    .badge_selected_border     = RGB(162, 196, 250),
    .scrollbar_thumb           = RGB(203, 213, 225),
    .scrollbar_thumb_hover     = RGB(148, 163, 184),

    .is_dark = FALSE
};

static AppTheme s_current_theme = THEME_DARK;
static HBRUSH s_hbrWindow = NULL;
static HBRUSH s_hbrSurface = NULL;

static void recreate_brushes(void) {
    if (s_hbrWindow) {
        DeleteObject(s_hbrWindow);
        s_hbrWindow = NULL;
    }
    if (s_hbrSurface) {
        DeleteObject(s_hbrSurface);
        s_hbrSurface = NULL;
    }

    const ThemePalette *p = theme_get_current();
    s_hbrWindow = CreateSolidBrush(p->bg_window);
    s_hbrSurface = CreateSolidBrush(p->bg_surface);
}

void theme_init(AppTheme initial_theme) {
    s_current_theme = initial_theme;
    recreate_brushes();
}

AppTheme theme_get_current_type(void) {
    return s_current_theme;
}

const ThemePalette *theme_get(AppTheme theme) {
    return (theme == THEME_LIGHT) ? &s_palette_light : &s_palette_dark;
}

const ThemePalette *theme_get_current(void) {
    return theme_get(s_current_theme);
}

void theme_set(AppTheme new_theme) {
    s_current_theme = new_theme;
    recreate_brushes();
}

AppTheme theme_toggle(void) {
    AppTheme next = (s_current_theme == THEME_DARK) ? THEME_LIGHT : THEME_DARK;
    theme_set(next);
    return next;
}

HBRUSH theme_get_window_brush(void) {
    return s_hbrWindow;
}

HBRUSH theme_get_surface_brush(void) {
    return s_hbrSurface;
}

void theme_cleanup(void) {
    if (s_hbrWindow) {
        DeleteObject(s_hbrWindow);
        s_hbrWindow = NULL;
    }
    if (s_hbrSurface) {
        DeleteObject(s_hbrSurface);
        s_hbrSurface = NULL;
    }
}
