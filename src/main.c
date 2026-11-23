#include <windows.h>
#include "snippet.h"
#include "storage.h"
#include "gui.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    // 1. Inicializa a lista dinamica de snippets em memoria
    SnippetList snippets;
    snippet_list_init(&snippets);

    // 2. Carrega os snippets do arquivo JSON no AppData (ou cria os padroes)
    storage_load_snippets(&snippets);

    // 3. Inicializa e exibe a interface grafica nativa Win32
    if (!gui_init(hInstance, nCmdShow, &snippets)) {
        MessageBoxA(NULL, "Erro ao inicializar a interface grafica do Nimbox.", "Erro Fatal", MB_OK | MB_ICONERROR);
        snippet_list_free(&snippets);
        return 1;
    }

    // 4. Loop de mensagens principal do Windows
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 5. Libera a memoria alocada ao encerrar o aplicativo
    snippet_list_free(&snippets);

    return (int)msg.wParam;
}
