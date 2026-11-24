#ifndef THEME_H
#define THEME_H

#include "settings.h"
#include <windows.h>
#include <stdbool.h>

typedef struct {
    AppTheme type;
    const wchar_t *name;

    // Cores de fundo (Window, Superficie, Superficie alternada)
    COLORREF bg_window;
    COLORREF bg_surface;
    COLORREF bg_surface_alt;
    COLORREF bg_hover;
    COLORREF bg_active;

    // Cores de bordas
    COLORREF border_normal;
    COLORREF border_hover;
    COLORREF border_focus;

    // Cores de texto
    COLORREF text_primary;
    COLORREF text_secondary;
    COLORREF text_muted;

    // Destaque primario (Acoes principais / Selecao)
    COLORREF accent;
    COLORREF accent_hover;
    COLORREF accent_active;
    COLORREF accent_text;

    // Status da Deteccao (Ativo / Pausado)
    COLORREF status_active_bg;
    COLORREF status_active_fg;
    COLORREF status_active_border;

    COLORREF status_paused_bg;
    COLORREF status_paused_hover_bg;
    COLORREF status_paused_active_bg;
    COLORREF status_paused_fg;
    COLORREF status_paused_border;

    // Cores de perigo / Exclusao
    COLORREF danger_fg;
    COLORREF danger_hover_bg;
    COLORREF danger_active_bg;
    COLORREF danger_hover_border;

    // Cores dedicadas da tabela de snippets e scrollbar
    COLORREF table_bg;
    COLORREF table_header_bg;
    COLORREF table_row;
    COLORREF table_row_alt;
    COLORREF table_row_hover;
    COLORREF table_row_selected;
    COLORREF table_row_selected_border;
    COLORREF table_separator;
    COLORREF table_border;
    COLORREF badge_bg;
    COLORREF badge_border;
    COLORREF badge_fg;
    COLORREF badge_selected_bg;
    COLORREF badge_selected_border;
    COLORREF scrollbar_thumb;
    COLORREF scrollbar_thumb_hover;

    BOOL is_dark;
} ThemePalette;

// Inicializa o gerenciador de temas com o tema informado
void theme_init(AppTheme initial_theme);

// Retorna o tema ativo atual
AppTheme theme_get_current_type(void);

// Retorna a paleta de cores do tema ativo atual
const ThemePalette *theme_get_current(void);

// Retorna a paleta de um tema especifico
const ThemePalette *theme_get(AppTheme theme);

// Altera o tema ativo e recria os brushes
void theme_set(AppTheme new_theme);

// Alterna entre Claro e Escuro e retorna o novo tema
AppTheme theme_toggle(void);

// Retorna o brush da cor de fundo da janela
HBRUSH theme_get_window_brush(void);

// Retorna o brush da cor de superficie/input
HBRUSH theme_get_surface_brush(void);

// Libera os recursos de GDI alocados pelos temas
void theme_cleanup(void);

#endif // THEME_H
