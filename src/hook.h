#ifndef HOOK_H
#define HOOK_H

#include "snippet.h"
#include <windows.h>
#include <stdbool.h>

// Mensagem customizada do Windows enviada a janela principal quando um snippet e detectado
#define WM_APP_EXPAND_SNIPPET (WM_APP + 101)

// Inicializa o hook de teclado de baixo nivel (WH_KEYBOARD_LL)
bool hook_init(SnippetList *snippets, HWND hwndMain);

// Remove o hook global e limpa recursos
void hook_cleanup(void);

// Pausa ou retoma a deteccao de snippets
void hook_set_paused(bool paused);

// Retorna se a deteccao esta pausada
bool hook_is_paused(void);

// Limpa o buffer de caracteres digitados
void hook_clear_buffer(void);

#endif // HOOK_H
