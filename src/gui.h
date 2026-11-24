#ifndef GUI_H
#define GUI_H

#include "snippet.h"
#include "settings.h"
#include <windows.h>
#include <stdbool.h>

// Mensagens customizadas da aplicacao
#define WM_APP_TRAYMSG (WM_APP + 102)

// Inicializa a interface grafica principal do Nimbox com suporte Unicode completo
bool gui_init(HINSTANCE hInstance, int nCmdShow, SnippetList *snippets, AppSettings *settings);

// Atualiza a tabela ListView com base nos snippets e no termo de pesquisa atual
void gui_refresh_list(void);

// Aplica um novo tema visual na interface e atualiza todos os controles
void gui_apply_theme(AppTheme theme);

// Exibe ou oculta a janela principal
void gui_show_window(bool show);

// Retorna se a janela principal esta visivel
bool gui_is_visible(void);

// Retorna o identificador HWND da janela principal
HWND gui_get_main_hwnd(void);

#endif // GUI_H
