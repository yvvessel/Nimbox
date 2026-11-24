#include <windows.h>
#include <shellapi.h>
#include "snippet.h"
#include "storage.h"
#include "settings.h"
#include "autostart.h"
#include "gui.h"
#include "hook.h"

static bool has_minimized_arg(void) {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool minimized = false;
    if (argv) {
        for (int i = 1; i < argc; i++) {
            if (_wcsicmp(argv[i], L"--minimized") == 0) {
                minimized = true;
                break;
            }
        }
        LocalFree(argv);
    }
    return minimized;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    bool start_minimized = has_minimized_arg();

    // 0. Previne multiplas instancias do Nimbox em execucao simultanea
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"NimboxSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!start_minimized) {
            HWND hExisting = FindWindowW(L"NimboxMainWindowClassW", NULL);
            if (hExisting) {
                ShowWindow(hExisting, SW_RESTORE);
                SetForegroundWindow(hExisting);
            }
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // 1. Carrega as preferencias de configuracao (Tema claro/escuro e estado real do autostart)
    AppSettings settings;
    settings_load(&settings);
    settings.start_with_windows = autostart_is_enabled();

    // 2. Inicializa a lista dinamica de snippets em memoria e carrega do disco
    SnippetList snippets;
    snippet_list_init(&snippets);
    storage_load_snippets(&snippets);

    // 3. Inicializa a interface grafica nativa Win32 (Unicode), oculta na bandeja se --minimized
    int initial_show = start_minimized ? SW_HIDE : nCmdShow;
    if (!gui_init(hInstance, initial_show, &snippets, &settings)) {
        MessageBoxW(NULL, L"Erro ao inicializar a interface gráfica do Nimbox.", L"Erro Fatal", MB_OK | MB_ICONERROR);
        snippet_list_free(&snippets);
        return 1;
    }

    // 4. Inicializa o hook global de teclado para deteccao dos gatilhos
    if (!hook_init(&snippets, gui_get_main_hwnd())) {
        MessageBoxW(gui_get_main_hwnd(), L"Aviso: Não foi possível instalar o hook global de teclado.\nA expansão automática ficará desativada.", L"Aviso Nimbox", MB_OK | MB_ICONWARNING);
    }

    // 5. Loop de mensagens principal do Windows (Unicode)
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 6. Remove o hook global e libera recursos
    hook_cleanup();

    // 7. Libera a memoria alocada ao encerrar o aplicativo
    snippet_list_free(&snippets);

    // 8. Libera o Mutex de instancia unica
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return (int)msg.wParam;
}
