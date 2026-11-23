#ifndef GUI_H
#define GUI_H

#include "snippet.h"
#include <windows.h>
#include <stdbool.h>

// Inicializa a interface grafica principal do Nimbox
bool gui_init(HINSTANCE hInstance, int nCmdShow, SnippetList *snippets);

// Atualiza a tabela ListView com base nos snippets e no termo de pesquisa atual
void gui_refresh_list(void);

// Exibe ou oculta a janela principal
void gui_show_window(bool show);

// Retorna se a janela principal esta visivel
bool gui_is_visible(void);

// Retorna o identificador HWND da janela principal
HWND gui_get_main_hwnd(void);

#endif // GUI_H
