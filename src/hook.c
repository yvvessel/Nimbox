#include "hook.h"
#include "injector.h"
#include <stdio.h>
#include <string.h>

#define BUFFER_CAPACITY 64

static HHOOK s_hook = NULL;
static SnippetList *s_snippets = NULL;
static HWND s_hwndMain = NULL;
static bool s_paused = false;

// Buffer circular de memoria contendo os caracteres recentes digitados (formato UTF-8)
static char s_buffer[BUFFER_CAPACITY] = {0};
static size_t s_buffer_len = 0;

void hook_clear_buffer(void) {
    s_buffer_len = 0;
    s_buffer[0] = '\0';
}

void hook_set_paused(bool paused) {
    s_paused = paused;
    if (paused) {
        hook_clear_buffer();
    }
}

bool hook_is_paused(void) {
    return s_paused;
}

// Verifica se o final do buffer coincide com o gatilho de algum snippet ativo
static void check_triggers(void) {
    if (!s_snippets || s_buffer_len == 0 || s_paused || injector_is_expanding()) {
        return;
    }

    for (size_t i = 0; i < s_snippets->count; i++) {
        const Snippet *snip = &s_snippets->items[i];
        size_t trig_len = strlen(snip->trigger);

        if (trig_len > 0 && trig_len <= s_buffer_len) {
            // Compara o final do buffer com o gatilho do snippet
            const char *suffix = s_buffer + s_buffer_len - trig_len;
            if (strcmp(suffix, snip->trigger) == 0) {
                // Gatilho identificado!
                // Limpa o buffer imediatamente para evitar repeticoes
                hook_clear_buffer();

                // Notifica a janela principal de forma assincrona para realizar a substituicao
                if (s_hwndMain) {
                    PostMessage(s_hwndMain, WM_APP_EXPAND_SNIPPET, (WPARAM)snip->id, 0);
                }
                break;
            }
        }
    }
}

// Callback executado pelo Windows a cada tecla pressionada no sistema
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION) {
        return CallNextHookEx(s_hook, nCode, wParam, lParam);
    }

    KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT *)lParam;

    // 1. Ignora teclas injetadas pelo proprio SendInput do Nimbox
    if ((kbd->flags & LLKHF_INJECTED) || injector_is_expanding()) {
        return CallNextHookEx(s_hook, nCode, wParam, lParam);
    }

    // 2. Se a deteccao estiver pausada pelo usuario, apenas repassa a tecla
    if (s_paused) {
        return CallNextHookEx(s_hook, nCode, wParam, lParam);
    }

    // Processa apenas no evento de pressionamento da tecla
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        DWORD vk = kbd->vkCode;

        // Se o usuario usar atalhos como Ctrl+C, Ctrl+V, etc (sem AltGr), limpa o buffer
        bool ctrl_down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool alt_down = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        if (ctrl_down && !alt_down) {
            hook_clear_buffer();
            return CallNextHookEx(s_hook, nCode, wParam, lParam);
        }

        // Teclas de controle/navegacao que invalidam qualquer palavra em construcao
        if (vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_TAB ||
            vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT ||
            vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT) {
            hook_clear_buffer();
            return CallNextHookEx(s_hook, nCode, wParam, lParam);
        }

        // Tecla Backspace: remove o ultimo caractere digitado (considerando sequencia UTF-8)
        if (vk == VK_BACK) {
            if (s_buffer_len > 0) {
                // Em UTF-8, bytes de continuacao comecam com os bits 10xxxxxx (0x80..0xBF)
                do {
                    s_buffer_len--;
                } while (s_buffer_len > 0 && ((unsigned char)s_buffer[s_buffer_len] & 0xC0) == 0x80);
                s_buffer[s_buffer_len] = '\0';
            }
            return CallNextHookEx(s_hook, nCode, wParam, lParam);
        }

        // Obtem o layout de teclado ativo no aplicativo com foco
        HWND hForeground = GetForegroundWindow();
        DWORD threadId = GetWindowThreadProcessId(hForeground, NULL);
        HKL layout = GetKeyboardLayout(threadId);

        // Se mudou de janela em foco, limpa o buffer para evitar disparos acidentais
        static HWND s_last_foreground = NULL;
        if (hForeground != s_last_foreground) {
            s_last_foreground = hForeground;
            hook_clear_buffer();
        }

        // Prepara o mapa de estado das teclas modificadoras
        BYTE keyState[256] = {0};
        GetKeyboardState(keyState);

        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) keyState[VK_SHIFT] = 0x80;
        if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) keyState[VK_LSHIFT] = 0x80;
        if (GetAsyncKeyState(VK_RSHIFT) & 0x8000) keyState[VK_RSHIFT] = 0x80;
        if (GetKeyState(VK_CAPITAL) & 0x0001) keyState[VK_CAPITAL] = 0x01;
        if (ctrl_down) keyState[VK_CONTROL] = 0x80;
        if (alt_down) keyState[VK_MENU] = 0x80;

        // Traduz o codigo de tecla virtual para caractere(s) Unicode
        // Flag 4 (0x04) evita corromper o estado de teclas mortas (acentos como ~, ^, ´) no app ativo
        WCHAR wchars[8] = {0};
        int count = ToUnicodeEx(vk, kbd->scanCode, keyState, wchars, 8, 4, layout);

        if (count > 0) {
            // Converte WCHAR para UTF-8
            char utf8_seq[16] = {0};
            int utf8_bytes = WideCharToMultiByte(CP_UTF8, 0, wchars, count, utf8_seq, sizeof(utf8_seq) - 1, NULL, NULL);

            if (utf8_bytes > 0) {
                utf8_seq[utf8_bytes] = '\0';

                // Se o buffer for estourar a capacidade, desloca os dados antigos para a esquerda
                if (s_buffer_len + utf8_bytes >= BUFFER_CAPACITY) {
                    size_t shift = (s_buffer_len + utf8_bytes) - BUFFER_CAPACITY + 8;
                    if (shift > s_buffer_len) shift = s_buffer_len;
                    memmove(s_buffer, s_buffer + shift, s_buffer_len - shift);
                    s_buffer_len -= shift;
                }

                // Adiciona o novo caractere digitado ao buffer
                memcpy(s_buffer + s_buffer_len, utf8_seq, utf8_bytes);
                s_buffer_len += utf8_bytes;
                s_buffer[s_buffer_len] = '\0';

                // Checa se o novo estado do buffer ativa algum snippet
                check_triggers();
            }
        }
    }

    return CallNextHookEx(s_hook, nCode, wParam, lParam);
}

bool hook_init(SnippetList *snippets, HWND hwndMain) {
    s_snippets = snippets;
    s_hwndMain = hwndMain;
    s_paused = false;
    hook_clear_buffer();

    s_hook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(NULL),
        0
    );

    return (s_hook != NULL);
}

void hook_cleanup(void) {
    if (s_hook) {
        UnhookWindowsHookEx(s_hook);
        s_hook = NULL;
    }
    hook_clear_buffer();
}
