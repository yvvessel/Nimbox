#include "gui.h"
#include "storage.h"
#include <commctrl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Identificadores de controles da janela principal
#define ID_SEARCH_BOX     1001
#define ID_LISTVIEW       1002
#define ID_BTN_NEW        1003
#define ID_BTN_EDIT       1004
#define ID_BTN_DELETE     1005
#define ID_LBL_STATUS     1006

// Identificadores dos campos do modal de criacao/edicao
#define ID_MODAL_NAME     2001
#define ID_MODAL_TRIGGER  2002
#define ID_MODAL_CONTENT  2003
#define ID_MODAL_SAVE     2004
#define ID_MODAL_CANCEL   2005

static HINSTANCE s_hInstance = NULL;
static HWND s_hwndMain = NULL;
static HWND s_hwndSearch = NULL;
static HWND s_hwndListView = NULL;
static HWND s_hwndStatus = NULL;
static HFONT s_hFont = NULL;
static SnippetList *s_snippets = NULL;

// Dados em transito durante a edicao/criacao no modal
static Snippet *s_current_editing = NULL;
static HWND s_hwndModal = NULL;
static HWND s_hwndModalName = NULL;
static HWND s_hwndModalTrigger = NULL;
static HWND s_hwndModalContent = NULL;

// Funcao auxiliar para busca case-insensitive (sem depender de strcasestr)
static bool string_contains_case_insensitive(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    if (needle[0] == '\0') return true;

    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);
    if (needle_len > haystack_len) return false;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        size_t j = 0;
        while (j < needle_len && tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == needle_len) return true;
    }
    return false;
}

static void apply_font_to_window_and_children(HWND hwnd, HFONT font) {
    SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    HWND child = GetWindow(hwnd, GW_CHILD);
    while (child) {
        SendMessage(child, WM_SETFONT, (WPARAM)font, TRUE);
        child = GetWindow(child, GW_HWNDNEXT);
    }
}

// Obtem o ID do snippet selecionado atualmente na ListView (-1 se nenhum)
static int get_selected_snippet_id(void) {
    int selected_index = ListView_GetNextItem(s_hwndListView, -1, LVNI_SELECTED);
    if (selected_index == -1) return -1;

    LVITEM item;
    memset(&item, 0, sizeof(LVITEM));
    item.mask = LVIF_PARAM;
    item.iItem = selected_index;

    if (ListView_GetItem(s_hwndListView, &item)) {
        return (int)item.lParam;
    }
    return -1;
}

void gui_refresh_list(void) {
    if (!s_hwndListView || !s_snippets) return;

    ListView_DeleteAllItems(s_hwndListView);

    char search_text[128] = {0};
    GetWindowTextA(s_hwndSearch, search_text, sizeof(search_text));

    int item_index = 0;
    int visible_count = 0;

    for (size_t i = 0; i < s_snippets->count; i++) {
        Snippet *s = &s_snippets->items[i];

        // Se houver termo de busca, filtra por nome ou gatilho
        if (search_text[0] != '\0') {
            bool match_name = string_contains_case_insensitive(s->name, search_text);
            bool match_trigger = string_contains_case_insensitive(s->trigger, search_text);
            if (!match_name && !match_trigger) {
                continue;
            }
        }

        // Coluna 0: Nome
        LVITEM lv_item;
        memset(&lv_item, 0, sizeof(LVITEM));
        lv_item.mask = LVIF_TEXT | LVIF_PARAM;
        lv_item.iItem = item_index;
        lv_item.iSubItem = 0;
        lv_item.pszText = s->name;
        lv_item.lParam = (LPARAM)s->id;

        int row = ListView_InsertItem(s_hwndListView, &lv_item);

        // Coluna 1: Gatilho
        ListView_SetItemText(s_hwndListView, row, 1, s->trigger);

        // Coluna 2: Conteudo resumido (substitui quebras de linha por espaco para preview)
        char content_preview[128];
        strncpy(content_preview, s->content, sizeof(content_preview) - 1);
        content_preview[sizeof(content_preview) - 1] = '\0';
        for (char *p = content_preview; *p; p++) {
            if (*p == '\r' || *p == '\n') *p = ' ';
        }
        ListView_SetItemText(s_hwndListView, row, 2, content_preview);

        item_index++;
        visible_count++;
    }

    // Atualiza o texto da barra de status
    char status_text[128];
    if (search_text[0] != '\0') {
        snprintf(status_text, sizeof(status_text), "Exibindo %d de %zu snippets cadastrados", visible_count, s_snippets->count);
    } else {
        snprintf(status_text, sizeof(status_text), "Total: %zu snippets cadastrados", s_snippets->count);
    }
    SetWindowTextA(s_hwndStatus, status_text);
}

// -------------------------------------------------------------
// Janela Modal de Criacao e Edicao de Snippet
// -------------------------------------------------------------

static LRESULT CALLBACK ModalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int control_id = LOWORD(wParam);
            if (control_id == ID_MODAL_SAVE) {
                char name[MAX_NAME_LEN] = {0};
                char trigger[MAX_TRIGGER_LEN] = {0};
                char content[MAX_CONTENT_LEN] = {0};

                GetWindowTextA(s_hwndModalName, name, sizeof(name));
                GetWindowTextA(s_hwndModalTrigger, trigger, sizeof(trigger));
                GetWindowTextA(s_hwndModalContent, content, sizeof(content));

                // Validacoes basicas
                if (strlen(name) == 0) {
                    MessageBoxA(hwnd, "Por favor, digite um nome para o snippet.", "Campo Obrigatorio", MB_OK | MB_ICONWARNING);
                    SetFocus(s_hwndModalName);
                    return 0;
                }
                if (strlen(trigger) == 0) {
                    MessageBoxA(hwnd, "Por favor, defina um gatilho (ex: email ou ;email).", "Campo Obrigatorio", MB_OK | MB_ICONWARNING);
                    SetFocus(s_hwndModalTrigger);
                    return 0;
                }
                if (strlen(content) == 0) {
                    MessageBoxA(hwnd, "Por favor, digite o texto a ser expandido.", "Campo Obrigatorio", MB_OK | MB_ICONWARNING);
                    SetFocus(s_hwndModalContent);
                    return 0;
                }

                // Verifica se o gatilho ja esta em uso por outro snippet
                Snippet *existing_with_trigger = snippet_list_find_by_trigger(s_snippets, trigger);
                if (existing_with_trigger && (!s_current_editing || existing_with_trigger->id != s_current_editing->id)) {
                    MessageBoxA(hwnd, "Ja existe um snippet com este mesmo gatilho. Escolha outro.", "Gatilho Duplicado", MB_OK | MB_ICONWARNING);
                    SetFocus(s_hwndModalTrigger);
                    return 0;
                }

                if (s_current_editing) {
                    // Atualiza o existente
                    snprintf(s_current_editing->name, sizeof(s_current_editing->name), "%s", name);
                    snprintf(s_current_editing->trigger, sizeof(s_current_editing->trigger), "%s", trigger);
                    snprintf(s_current_editing->content, sizeof(s_current_editing->content), "%s", content);
                    snippet_list_update(s_snippets, s_current_editing);
                } else {
                    // Cria um novo
                    Snippet new_snip;
                    memset(&new_snip, 0, sizeof(Snippet));
                    new_snip.id = snippet_list_get_next_id(s_snippets);
                    snprintf(new_snip.name, sizeof(new_snip.name), "%s", name);
                    snprintf(new_snip.trigger, sizeof(new_snip.trigger), "%s", trigger);
                    snprintf(new_snip.content, sizeof(new_snip.content), "%s", content);
                    snippet_list_add(s_snippets, &new_snip);
                }

                // Salva no disco imediatamente
                storage_save_snippets(s_snippets);
                gui_refresh_list();

                EnableWindow(s_hwndMain, TRUE);
                DestroyWindow(hwnd);
                s_hwndModal = NULL;
                s_current_editing = NULL;
                SetActiveWindow(s_hwndMain);
                return 0;
            } else if (control_id == ID_MODAL_CANCEL || control_id == IDCANCEL) {
                EnableWindow(s_hwndMain, TRUE);
                DestroyWindow(hwnd);
                s_hwndModal = NULL;
                s_current_editing = NULL;
                SetActiveWindow(s_hwndMain);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            EnableWindow(s_hwndMain, TRUE);
            DestroyWindow(hwnd);
            s_hwndModal = NULL;
            s_current_editing = NULL;
            SetActiveWindow(s_hwndMain);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static void open_snippet_modal(HWND hParent, Snippet *snippet_to_edit) {
    if (s_hwndModal) {
        SetActiveWindow(s_hwndModal);
        return;
    }

    s_current_editing = snippet_to_edit;

    const char *title = snippet_to_edit ? "Nimbox — Editar Snippet" : "Nimbox — Novo Snippet";

    // Registra a classe do modal se ainda nao estiver registrada
    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASSA wc = {0};
        wc.lpfnWndProc = ModalWndProc;
        wc.hInstance = s_hInstance;
        wc.lpszClassName = "NimboxModalClass";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassA(&wc);
        class_registered = true;
    }

    // Calcula posicao centralizada em relacao a janela principal
    RECT rcMain;
    GetWindowRect(hParent, &rcMain);
    int modal_width = 460;
    int modal_height = 370;
    int x = rcMain.left + (rcMain.right - rcMain.left - modal_width) / 2;
    int y = rcMain.top + (rcMain.bottom - rcMain.top - modal_height) / 2;

    s_hwndModal = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "NimboxModalClass",
        title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, modal_width, modal_height,
        hParent, NULL, s_hInstance, NULL
    );

    EnableWindow(hParent, FALSE);

    // Labels e campos
    CreateWindowA("STATIC", "Nome do Snippet:", WS_CHILD | WS_VISIBLE, 20, 16, 200, 18, s_hwndModal, NULL, s_hInstance, NULL);
    s_hwndModalName = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 36, 400, 24, s_hwndModal, (HMENU)ID_MODAL_NAME, s_hInstance, NULL);

    CreateWindowA("STATIC", "Palavra-chave / Gatilho (ex: email ou ;email):", WS_CHILD | WS_VISIBLE, 20, 68, 300, 18, s_hwndModal, NULL, s_hInstance, NULL);
    s_hwndModalTrigger = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 88, 400, 24, s_hwndModal, (HMENU)ID_MODAL_TRIGGER, s_hInstance, NULL);

    CreateWindowA("STATIC", "Conteudo a ser inserido:", WS_CHILD | WS_VISIBLE, 20, 120, 200, 18, s_hwndModal, NULL, s_hInstance, NULL);
    s_hwndModalContent = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        20, 140, 400, 125,
        s_hwndModal, (HMENU)ID_MODAL_CONTENT, s_hInstance, NULL
    );

    // Botao Salvar e Cancelar
    CreateWindowA("BUTTON", "Salvar", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 230, 280, 90, 30, s_hwndModal, (HMENU)ID_MODAL_SAVE, s_hInstance, NULL);
    CreateWindowA("BUTTON", "Cancelar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 330, 280, 90, 30, s_hwndModal, (HMENU)ID_MODAL_CANCEL, s_hInstance, NULL);

    apply_font_to_window_and_children(s_hwndModal, s_hFont);

    // Se estiver editando, preenche os campos com os valores atuais
    if (snippet_to_edit) {
        SetWindowTextA(s_hwndModalName, snippet_to_edit->name);
        SetWindowTextA(s_hwndModalTrigger, snippet_to_edit->trigger);
        SetWindowTextA(s_hwndModalContent, snippet_to_edit->content);
    }

    SetFocus(s_hwndModalName);
}

// -------------------------------------------------------------
// Procedimento da Janela Principal (Main WndProc)
// -------------------------------------------------------------

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Barra de busca
            s_hwndSearch = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                16, 16, 380, 28,
                hwnd, (HMENU)ID_SEARCH_BOX, s_hInstance, NULL
            );

            // Botao Novo
            CreateWindowA("BUTTON", "+ Novo Snippet", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 410, 15, 120, 29, hwnd, (HMENU)ID_BTN_NEW, s_hInstance, NULL);

            // Botao Editar
            CreateWindowA("BUTTON", "Editar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 538, 15, 80, 29, hwnd, (HMENU)ID_BTN_EDIT, s_hInstance, NULL);

            // Botao Excluir
            CreateWindowA("BUTTON", "Excluir", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 624, 15, 80, 29, hwnd, (HMENU)ID_BTN_DELETE, s_hInstance, NULL);

            // ListView para exibicao dos snippets
            s_hwndListView = CreateWindowExA(
                0, WC_LISTVIEWA, "",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER,
                16, 56, 688, 380,
                hwnd, (HMENU)ID_LISTVIEW, s_hInstance, NULL
            );

            // Estilos estendidos modernos: selecao de linha inteira e linhas de grade
            ListView_SetExtendedListViewStyle(s_hwndListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            // Cria as colunas da ListView
            LVCOLUMNA col;
            memset(&col, 0, sizeof(LVCOLUMNA));
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            col.cx = 180;
            col.pszText = "Nome";
            ListView_InsertColumn(s_hwndListView, 0, &col);

            col.cx = 130;
            col.pszText = "Gatilho";
            ListView_InsertColumn(s_hwndListView, 1, &col);

            col.cx = 360;
            col.pszText = "Conteudo";
            ListView_InsertColumn(s_hwndListView, 2, &col);

            // Label de status no rodape
            s_hwndStatus = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 18, 446, 680, 20, hwnd, (HMENU)ID_LBL_STATUS, s_hInstance, NULL);

            apply_font_to_window_and_children(hwnd, s_hFont);

            gui_refresh_list();
            break;
        }

        case WM_COMMAND: {
            int control_id = LOWORD(wParam);
            int notification = HIWORD(wParam);

            // Atualiza lista em tempo real conforme digita na busca
            if (control_id == ID_SEARCH_BOX && notification == EN_CHANGE) {
                gui_refresh_list();
            } else if (control_id == ID_BTN_NEW) {
                open_snippet_modal(hwnd, NULL);
            } else if (control_id == ID_BTN_EDIT) {
                int id = get_selected_snippet_id();
                if (id != -1) {
                    Snippet *s = snippet_list_find_by_id(s_snippets, id);
                    if (s) open_snippet_modal(hwnd, s);
                } else {
                    MessageBoxA(hwnd, "Selecione um snippet na lista para editar.", "Nenhum Snippet Selecionado", MB_OK | MB_ICONINFORMATION);
                }
            } else if (control_id == ID_BTN_DELETE) {
                int id = get_selected_snippet_id();
                if (id != -1) {
                    Snippet *s = snippet_list_find_by_id(s_snippets, id);
                    if (s) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Tem certeza que deseja excluir o snippet \"%s\"?", s->name);
                        int choice = MessageBoxA(hwnd, msg, "Confirmar Exclusao", MB_YESNO | MB_ICONQUESTION);
                        if (choice == IDYES) {
                            snippet_list_remove(s_snippets, id);
                            storage_save_snippets(s_snippets);
                            gui_refresh_list();
                        }
                    }
                } else {
                    MessageBoxA(hwnd, "Selecione um snippet na lista para excluir.", "Nenhum Snippet Selecionado", MB_OK | MB_ICONINFORMATION);
                }
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->idFrom == ID_LISTVIEW && nmhdr->code == NM_DBLCLK) {
                // Duplo clique na lista abre diretamente para edicao
                int id = get_selected_snippet_id();
                if (id != -1) {
                    Snippet *s = snippet_list_find_by_id(s_snippets, id);
                    if (s) open_snippet_modal(hwnd, s);
                }
            }
            break;
        }

        case WM_CLOSE:
            // Por enquanto, encerra o aplicativo. Na etapa da bandeja do sistema, apenas ocultara a janela.
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

bool gui_init(HINSTANCE hInstance, int nCmdShow, SnippetList *snippets) {
    s_hInstance = hInstance;
    s_snippets = snippets;

    // Inicializa controles visuais modernos (ListView, etc)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // Fonte Segoe UI padrao moderna do Windows
    s_hFont = CreateFontA(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
    );

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "NimboxMainWindowClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassA(&wc)) {
        return false;
    }

    int win_width = 736;
    int win_height = 515;

    // Centraliza a janela na tela do usuario
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_w - win_width) / 2;
    int y = (screen_h - win_height) / 2;

    s_hwndMain = CreateWindowExA(
        WS_EX_APPWINDOW,
        "NimboxMainWindowClass",
        "Nimbox — Gerenciador de Snippets",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, win_width, win_height,
        NULL, NULL, hInstance, NULL
    );

    if (!s_hwndMain) {
        return false;
    }

    ShowWindow(s_hwndMain, nCmdShow);
    UpdateWindow(s_hwndMain);
    return true;
}

void gui_show_window(bool show) {
    if (s_hwndMain) {
        ShowWindow(s_hwndMain, show ? SW_SHOW : SW_HIDE);
        if (show) {
            SetForegroundWindow(s_hwndMain);
        }
    }
}

bool gui_is_visible(void) {
    return s_hwndMain && IsWindowVisible(s_hwndMain);
}

HWND gui_get_main_hwnd(void) {
    return s_hwndMain;
}
