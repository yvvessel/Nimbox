#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wchar.h>
#include "snippet.h"
#include "storage.h"
#include "settings.h"
#include "autostart.h"

static void cleanup_test_registry_root(void) {
    HKEY hSoft = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hSoft) == ERROR_SUCCESS) {
        RegDeleteTreeW(hSoft, L"NimboxTest");
        RegCloseKey(hSoft);
    }
}

static bool create_isolated_test_dir(char *out_dir, size_t max_len) {
    if (!out_dir || max_len == 0) return false;

    char temp_root[MAX_PATH] = {0};
    DWORD len = GetTempPathA(MAX_PATH, temp_root);
    if (len == 0 || len >= MAX_PATH) {
        return false;
    }

    int written = snprintf(
        out_dir,
        max_len,
        "%snimbox_test_%lu_%lu",
        temp_root,
        (unsigned long)GetCurrentProcessId(),
        (unsigned long)GetTickCount()
    );
    if (written <= 0 || (size_t)written >= max_len) {
        return false;
    }

    if (!CreateDirectoryA(out_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    return true;
}

static void cleanup_isolated_test_dir(const char *test_dir) {
    if (!test_dir || test_dir[0] == '\0') return;

    const char *files[] = {"settings.json", "snippets.json", "snippets.json.tmp"};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char file_path[MAX_PATH + 64];
        snprintf(file_path, sizeof(file_path), "%s\\%s", test_dir, files[i]);
        DeleteFileA(file_path);
    }
    RemoveDirectoryA(test_dir);
}

int main(void) {
    int exit_code = 1;
    char test_dir[MAX_PATH] = {0};
    SnippetList list;
    SnippetList reloaded;
    snippet_list_init(&list);
    snippet_list_init(&reloaded);

    printf("====================================================\n");
    printf("   Nimbox - Testes Unitarios e Validacao Etapas 4/5 \n");
    printf("====================================================\n\n");

    if (!create_isolated_test_dir(test_dir, sizeof(test_dir))) {
        printf("[FALHA] Nao foi possivel criar diretorio temporario exclusivo para os testes.\n");
        goto cleanup;
    }

    settings_set_custom_dir(test_dir);
    storage_set_custom_dir(test_dir);
    printf("[*] Diretorio isolado de testes: %s\n\n", test_dir);

    char storage_path[MAX_PATH] = {0};
    if (!storage_get_file_path(storage_path, sizeof(storage_path)) ||
        strncmp(storage_path, test_dir, strlen(test_dir)) != 0) {
        printf("[FALHA] Caminho de armazenamento nao esta isolado no diretorio temporario.\n");
        goto cleanup;
    }

    // 1. Teste de Configuracoes (settings.json isolado)
    printf("[1] Testando persistencia de configuracoes (settings.json)...\n");
    AppSettings default_settings;
    settings_init_default(&default_settings);
    if (default_settings.start_with_windows != false) {
        printf("    [FALHA] start_with_windows deveria iniciar desativado por padrao!\n");
        goto cleanup;
    }
    printf("    [OK] Valor padrao de start_with_windows = false!\n");

    AppSettings settings;
    if (settings_load(&settings)) {
        printf("    [OK] Configuracoes carregadas com sucesso! Tema atual: %s\n",
            (settings.theme == THEME_LIGHT) ? "Claro (LIGHT)" : "Escuro (DARK)");
    } else {
        printf("    [FALHA] Nao foi possivel carregar settings.json\n");
        goto cleanup;
    }

    // Alterna o tema e start_with_windows e salva
    AppTheme original_theme = settings.theme;
    bool original_autostart = settings.start_with_windows;
    settings.theme = (original_theme == THEME_DARK) ? THEME_LIGHT : THEME_DARK;
    settings.start_with_windows = !original_autostart;
    printf("    [*] Alterando tema para: %s e salvando...\n",
        (settings.theme == THEME_LIGHT) ? "Claro" : "Escuro");

    if (!settings_save(&settings)) {
        printf("    [FALHA] Nao foi possivel salvar settings.json!\n");
        goto cleanup;
    }
    printf("    [OK] settings.json salvo com exito!\n");

    // Recarrega para comprovar que persistiu
    AppSettings reload_settings;
    if (!settings_load(&reload_settings)) {
        printf("    [FALHA] Nao foi possivel recarregar settings.json!\n");
        goto cleanup;
    }
    if (reload_settings.theme == settings.theme &&
        reload_settings.start_with_windows == settings.start_with_windows) {
        printf("    [OK] Persistencia de tema e preferencia comprovada!\n");
    } else {
        printf("    [FALHA] Os dados recarregados nao batem com os salvos.\n");
        goto cleanup;
    }

    // Restaura configuracoes no arquivo isolado
    settings.theme = original_theme;
    settings.start_with_windows = original_autostart;
    if (!settings_save(&settings)) {
        printf("    [FALHA] Nao foi possivel restaurar settings.json isolado.\n");
        goto cleanup;
    }
    printf("    [OK] Configuracoes restauradas no arquivo isolado.\n\n");

    // 2. Teste de Acentuacao Unicode em Snippets (UTF-8)
    printf("[2] Testando snippets com caracteres acentuados em portugues...\n");
    if (!storage_load_snippets(&list)) {
        printf("    [FALHA] Nao foi possivel inicializar snippets.json isolado.\n");
        goto cleanup;
    }

    const char *test_trigger = "!frase";
    const char *test_content = "Olá, você está bem? Ação, informação e coração. ã õ ç é á í ó ú Ê À";

    // Se ja existir um snippet com esse gatilho, atualiza; senao adiciona
    Snippet *existing = snippet_list_find_by_trigger(&list, test_trigger);
    if (existing) {
        strncpy(existing->content, test_content, MAX_CONTENT_LEN - 1);
        existing->content[MAX_CONTENT_LEN - 1] = '\0';
        snippet_list_update(&list, existing);
    } else {
        Snippet novo;
        memset(&novo, 0, sizeof(novo));
        novo.id = snippet_list_get_next_id(&list);
        strncpy(novo.name, "Mensagem com Acentos", MAX_NAME_LEN - 1);
        strncpy(novo.trigger, test_trigger, MAX_TRIGGER_LEN - 1);
        strncpy(novo.content, test_content, MAX_CONTENT_LEN - 1);
        snippet_list_add(&list, &novo);
    }

    if (!storage_save_snippets(&list)) {
        printf("    [FALHA] Nao foi possivel salvar snippet acentuado no diretorio isolado!\n");
        goto cleanup;
    }
    printf("    [OK] Snippet acentuado salvo no disco isolado com sucesso!\n");
    snippet_list_free(&list);

    // Recarrega e valida conteudo byte a byte
    if (!storage_load_snippets(&reloaded)) {
        printf("    [FALHA] Nao foi possivel recarregar snippets.json isolado!\n");
        goto cleanup;
    }

    Snippet *verified = snippet_list_find_by_trigger(&reloaded, test_trigger);
    if (verified && strcmp(verified->content, test_content) == 0) {
        printf("    [OK] Validacao perfeita de acentos!\n");
        printf("         Gatilho:  %s\n", verified->trigger);
        printf("         Conteudo: %s\n", verified->content);
    } else {
        printf("    [FALHA] Conteudo acentuado corrompido ao recarregar!\n");
        goto cleanup;
    }
    snippet_list_free(&reloaded);

    // 3. Teste isolado de Inicializacao com o Windows (Registro HKCU\Software\NimboxTest\Run)
    printf("\n[3] Testando autostart no Registro (chave isolada HKCU\\Software\\NimboxTest\\Run)...\n");
    const wchar_t *test_subkey = L"Software\\NimboxTest\\Run";
    cleanup_test_registry_root();

    if (autostart_is_enabled_in_key(test_subkey)) {
        printf("    [FALHA] Chave isolada deveria iniciar desativada.\n");
        goto cleanup;
    }

    if (!autostart_set_enabled_in_key(test_subkey, true) || !autostart_is_enabled_in_key(test_subkey)) {
        printf("    [FALHA] Nao foi possivel ativar autostart na chave de teste.\n");
        goto cleanup;
    }
    printf("    [OK] Autostart ativado com sucesso na chave isolada.\n");

    wchar_t reg_cmd[MAX_PATH + 32] = {0};
    wchar_t exe_path[MAX_PATH] = {0};
    wchar_t expected_cmd[MAX_PATH + 32] = {0};
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    swprintf(expected_cmd, MAX_PATH + 32, L"\"%ls\" --minimized", exe_path);

    if (!autostart_read_command_in_key(test_subkey, reg_cmd, MAX_PATH + 32) ||
        wcscmp(reg_cmd, expected_cmd) != 0) {
        printf("    [FALHA] Valor do Registro nao corresponde ao caminho com aspas e --minimized.\n");
        goto cleanup;
    }
    printf("    [OK] Valor do Registro validado com aspas e argumento --minimized.\n");

    // Adiciona outro valor irmao na mesma chave para garantir que desativar remove apenas "Nimbox"
    HKEY hTestKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, test_subkey, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hTestKey) == ERROR_SUCCESS) {
        const wchar_t *dummy_val = L"\"C:\\Dummy\\OtherApp.exe\"";
        RegSetValueExW(hTestKey, L"OtherApp", 0, REG_SZ, (const BYTE *)dummy_val, (DWORD)((wcslen(dummy_val) + 1) * sizeof(wchar_t)));
        RegCloseKey(hTestKey);
    }

    if (!autostart_set_enabled_in_key(test_subkey, false) || autostart_is_enabled_in_key(test_subkey)) {
        printf("    [FALHA] Nao foi possivel desativar autostart na chave de teste.\n");
        goto cleanup;
    }

    // Confirma que OtherApp permaneceu intacto
    if (RegOpenKeyExW(HKEY_CURRENT_USER, test_subkey, 0, KEY_QUERY_VALUE, &hTestKey) == ERROR_SUCCESS) {
        wchar_t sibling_buf[128] = {0};
        DWORD sibling_sz = sizeof(sibling_buf);
        LONG sib_res = RegQueryValueExW(hTestKey, L"OtherApp", NULL, NULL, (LPBYTE)sibling_buf, &sibling_sz);
        RegCloseKey(hTestKey);
        if (sib_res != ERROR_SUCCESS) {
            printf("    [FALHA] A desativacao afetou outros valores na chave Run.\n");
            goto cleanup;
        }
    }
    printf("    [OK] Desativacao removeu somente o valor Nimbox sem afetar outros itens.\n");

    exit_code = 0;

cleanup:
    snippet_list_free(&list);
    snippet_list_free(&reloaded);
    cleanup_test_registry_root();
    settings_set_custom_dir(NULL);
    storage_set_custom_dir(NULL);
    cleanup_isolated_test_dir(test_dir);

    if (exit_code == 0) {
        printf("    [OK] Chave e diretorio temporarios removidos com limpeza total.\n");
        printf("\n====================================================\n");
        printf("  [SUCESSO] Todos os testes das Etapas 4 e 5 passaram!\n");
        printf("====================================================\n");
    }

    return exit_code;
}
