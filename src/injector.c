#include "injector.h"
#include <windows.h>
#include <stdlib.h>

static bool s_is_expanding = false;

bool injector_is_expanding(void) {
    return s_is_expanding;
}

void injector_replace_snippet(const Snippet *snippet) {
    if (!snippet || s_is_expanding) return;

    s_is_expanding = true;

    // 1. Calcula a quantidade de caracteres do gatilho para enviar Backspaces
    int trigger_chars = MultiByteToWideChar(CP_UTF8, 0, snippet->trigger, -1, NULL, 0) - 1;
    if (trigger_chars <= 0) {
        s_is_expanding = false;
        return;
    }

    // Se o Ctrl estiver fisicamente pressionado, libera temporariamente para evitar Ctrl+Backspace (que apagaria palavras inteiras)
    bool ctrl_was_down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (ctrl_was_down) {
        INPUT rel = {0};
        rel.type = INPUT_KEYBOARD;
        rel.ki.wVk = VK_CONTROL;
        rel.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &rel, sizeof(INPUT));
    }

    // Aloca eventos para os Backspaces (2 eventos por caractere: KEYDOWN e KEYUP)
    INPUT *bs_inputs = (INPUT *)calloc(trigger_chars * 2, sizeof(INPUT));
    if (bs_inputs) {
        for (int i = 0; i < trigger_chars; i++) {
            bs_inputs[i * 2].type = INPUT_KEYBOARD;
            bs_inputs[i * 2].ki.wVk = VK_BACK;

            bs_inputs[i * 2 + 1].type = INPUT_KEYBOARD;
            bs_inputs[i * 2 + 1].ki.wVk = VK_BACK;
            bs_inputs[i * 2 + 1].ki.dwFlags = KEYEVENTF_KEYUP;
        }
        SendInput((UINT)(trigger_chars * 2), bs_inputs, sizeof(INPUT));
        free(bs_inputs);
    }

    // Pequena pausa (10ms) para dar tempo do aplicativo de destino processar os backspaces
    Sleep(10);

    // 2. Converte o conteudo UTF-8 para UTF-16 (WCHAR) para envio Unicode
    int content_wlen = MultiByteToWideChar(CP_UTF8, 0, snippet->content, -1, NULL, 0);
    if (content_wlen > 1) {
        WCHAR *wcontent = (WCHAR *)malloc(content_wlen * sizeof(WCHAR));
        if (wcontent) {
            MultiByteToWideChar(CP_UTF8, 0, snippet->content, -1, wcontent, content_wlen);

            // Aloca espaco para os eventos de insercao de texto
            INPUT *text_inputs = (INPUT *)calloc(content_wlen * 2, sizeof(INPUT));
            if (text_inputs) {
                int input_count = 0;

                for (int i = 0; i < content_wlen - 1; i++) {
                    WCHAR ch = wcontent[i];

                    if (ch == L'\r') {
                        // Se for \r\n, pula o \r e trata apenas a quebra no \n
                        if (i + 1 < content_wlen - 1 && wcontent[i + 1] == L'\n') {
                            continue;
                        }
                        // Enter isolado (\r)
                        text_inputs[input_count].type = INPUT_KEYBOARD;
                        text_inputs[input_count].ki.wVk = VK_RETURN;
                        input_count++;

                        text_inputs[input_count].type = INPUT_KEYBOARD;
                        text_inputs[input_count].ki.wVk = VK_RETURN;
                        text_inputs[input_count].ki.dwFlags = KEYEVENTF_KEYUP;
                        input_count++;
                    } else if (ch == L'\n') {
                        // Enter (\n)
                        text_inputs[input_count].type = INPUT_KEYBOARD;
                        text_inputs[input_count].ki.wVk = VK_RETURN;
                        input_count++;

                        text_inputs[input_count].type = INPUT_KEYBOARD;
                        text_inputs[input_count].ki.wVk = VK_RETURN;
                        text_inputs[input_count].ki.dwFlags = KEYEVENTF_KEYUP;
                        input_count++;
                    } else {
                        // Caractere Unicode (letras maiusculas/minusculas, acentos, espacos, simbolos)
                        text_inputs[input_count].type = INPUT_KEYBOARD;
                        text_inputs[input_count].ki.wScan = ch;
                        text_inputs[input_count].ki.dwFlags = KEYEVENTF_UNICODE;
                        input_count++;

                        text_inputs[input_count].type = INPUT_KEYBOARD;
                        text_inputs[input_count].ki.wScan = ch;
                        text_inputs[input_count].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                        input_count++;
                    }
                }

                SendInput((UINT)input_count, text_inputs, sizeof(INPUT));
                free(text_inputs);
            }

            free(wcontent);
        }
    }

    s_is_expanding = false;
}
