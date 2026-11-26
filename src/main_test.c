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

    // 4. Testes de Metadados e Ciclo de Vida do Snippet (Cenarios 1 a 5)
    printf("\n[4] Testando ciclo de vida dos metadados de Snippets (Cenarios 1 a 5)...\n");
    Snippet s_meta;
    memset(&s_meta, 0, sizeof(s_meta));
    s_meta.id = 101;
    strncpy(s_meta.name, "Snippet Metadados", MAX_NAME_LEN - 1);
    strncpy(s_meta.trigger, "!meta", MAX_TRIGGER_LEN - 1);
    strncpy(s_meta.content, "Conteudo Inicial", MAX_CONTENT_LEN - 1);

    // Cenario 1: Criacao de snippet com Created e Modified
    time_t base_time = time(NULL) - 60; // 60 segundos no passado
    s_meta.created = base_time;
    s_meta.modified = base_time;
    snprintf(s_meta.content_type, sizeof(s_meta.content_type), "Text");

    SnippetList meta_list;
    snippet_list_init(&meta_list);
    if (!snippet_list_add(&meta_list, &s_meta) || meta_list.count != 1) {
        printf("    [FALHA] Falha ao adicionar snippet com metadados.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    if (meta_list.items[0].created != base_time || meta_list.items[0].modified != base_time ||
        strcmp(meta_list.items[0].content_type, "Text") != 0) {
        printf("    [FALHA] Metadados iniciais incorretos na criacao do snippet.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    printf("    [OK] Cenario 1: Snippet criado com Created e Modified validos (Type=%s).\n", meta_list.items[0].content_type);

    // Cenario 2: Atualizacao de Modified apos edicao do nome
    Snippet edit_name = meta_list.items[0];
    strncpy(edit_name.name, "Snippet Nome Editado", MAX_NAME_LEN - 1);
    if (!snippet_list_update(&meta_list, &edit_name)) {
        printf("    [FALHA] Falha ao atualizar nome do snippet.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    if (meta_list.items[0].modified <= base_time || meta_list.items[0].created != base_time) {
        printf("    [FALHA] Modified nao foi atualizado ou Created foi violado apos edicao de nome.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    printf("    [OK] Cenario 2: Modified atualizado com sucesso apos edicao de nome.\n");

    // Cenario 3: Atualizacao de Modified apos edicao do gatilho
    meta_list.items[0].modified = base_time;
    Snippet edit_trigger = meta_list.items[0];
    strncpy(edit_trigger.trigger, "!novogatilho", MAX_TRIGGER_LEN - 1);
    if (!snippet_list_update(&meta_list, &edit_trigger)) {
        printf("    [FALHA] Falha ao atualizar gatilho do snippet.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    if (meta_list.items[0].modified <= base_time || meta_list.items[0].created != base_time) {
        printf("    [FALHA] Modified nao foi atualizado ou Created foi violado apos edicao de gatilho.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    printf("    [OK] Cenario 3: Modified atualizado com sucesso apos edicao de gatilho.\n");

    // Cenario 4: Atualizacao de Modified apos edicao do conteudo
    meta_list.items[0].modified = base_time;
    Snippet edit_content = meta_list.items[0];
    strncpy(edit_content.content, "Conteudo Totalmente Novo", MAX_CONTENT_LEN - 1);
    if (!snippet_list_update(&meta_list, &edit_content)) {
        printf("    [FALHA] Falha ao atualizar conteudo do snippet.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    if (meta_list.items[0].modified <= base_time || meta_list.items[0].created != base_time) {
        printf("    [FALHA] Modified nao foi atualizado ou Created foi violado apos edicao de conteudo.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    printf("    [OK] Cenario 4: Modified atualizado com sucesso apos edicao de conteudo.\n");

    // Cenario 5: Preservacao de Created apos qualquer edicao e Modified inalterado sem mudancas
    time_t current_mod = meta_list.items[0].modified;
    Snippet no_change = meta_list.items[0];
    if (!snippet_list_update(&meta_list, &no_change)) {
        printf("    [FALHA] Falha na chamada update sem alteracoes.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    if (meta_list.items[0].created != base_time || meta_list.items[0].modified != current_mod) {
        printf("    [FALHA] Created foi violado ou Modified foi alterado sem mudanca de dados.\n");
        snippet_list_free(&meta_list);
        goto cleanup;
    }
    printf("    [OK] Cenario 5: Created estritamente preservado e Modified inalterado em operacao idempotente.\n");
    snippet_list_free(&meta_list);

    // 5. Testes de Persistencia, Migracao e Idempotencia (Cenarios 6 a 9)
    printf("\n[5] Testando persistencia, migracao e idempotencia em JSON (Cenarios 6 a 9)...\n");

    // Cenario 6: Persistencia e leitura dos metadados em JSON
    SnippetList json_test_list;
    snippet_list_init(&json_test_list);
    Snippet s_persist;
    memset(&s_persist, 0, sizeof(s_persist));
    s_persist.id = 200;
    strncpy(s_persist.name, "Snippet Persistente", MAX_NAME_LEN - 1);
    strncpy(s_persist.trigger, "!persist", MAX_TRIGGER_LEN - 1);
    strncpy(s_persist.content, "Linha persistida com metadados.", MAX_CONTENT_LEN - 1);
    s_persist.created = 1700000000;
    s_persist.modified = 1700005000;
    snprintf(s_persist.content_type, sizeof(s_persist.content_type), "Text");
    snippet_list_add(&json_test_list, &s_persist);

    if (!storage_save_snippets(&json_test_list)) {
        printf("    [FALHA] storage_save_snippets falhou para metadados.\n");
        snippet_list_free(&json_test_list);
        goto cleanup;
    }
    snippet_list_free(&json_test_list);

    // Verifica arquivo bruto
    char snippets_file_path[MAX_PATH] = {0};
    storage_get_file_path(snippets_file_path, sizeof(snippets_file_path));
    FILE *f_raw = fopen(snippets_file_path, "rb");
    if (!f_raw) {
        printf("    [FALHA] Nao foi possivel abrir snippets.json gravado.\n");
        goto cleanup;
    }
    fseek(f_raw, 0, SEEK_END);
    long f_sz = ftell(f_raw);
    fseek(f_raw, 0, SEEK_SET);
    char *raw_text = (char *)malloc(f_sz + 1);
    if (!raw_text) {
        fclose(f_raw);
        goto cleanup;
    }
    fread(raw_text, 1, f_sz, f_raw);
    raw_text[f_sz] = '\0';
    fclose(f_raw);

    if (!strstr(raw_text, "\"content_type\"") || !strstr(raw_text, "\"Text\"") ||
        !strstr(raw_text, "\"created\"") || !strstr(raw_text, "\"modified\"")) {
        printf("    [FALHA] O arquivo JSON nao contem os campos esperados de metadados.\n");
        free(raw_text);
        goto cleanup;
    }
    free(raw_text);

    // Recarrega via storage_load_snippets e compara timestamps
    SnippetList reloaded_meta;
    snippet_list_init(&reloaded_meta);
    if (!storage_load_snippets(&reloaded_meta) || reloaded_meta.count != 1) {
        printf("    [FALHA] Falha ao recarregar snippets.json com metadados.\n");
        snippet_list_free(&reloaded_meta);
        goto cleanup;
    }
    if (reloaded_meta.items[0].created != 1700000000 || reloaded_meta.items[0].modified != 1700005000 ||
        strcmp(reloaded_meta.items[0].content_type, "Text") != 0) {
        printf("    [FALHA] Timestamps recarregados nao coincidem com os persistidos.\n");
        snippet_list_free(&reloaded_meta);
        goto cleanup;
    }
    printf("    [OK] Cenario 6: Persistencia e leitura integral dos metadados em JSON validadas.\n");
    snippet_list_free(&reloaded_meta);

    // Cenarios 7 e 8: Migracao de snippets antigos sem metadados e preservacao de dados
    FILE *f_legacy = fopen(snippets_file_path, "wb");
    if (!f_legacy) {
        printf("    [FALHA] Nao foi possivel criar snippets.json legado.\n");
        goto cleanup;
    }
    const char *legacy_json = "[\n"
                              "  {\n"
                              "    \"id\": 300,\n"
                              "    \"name\": \"Snippet Legado Antigo\",\n"
                              "    \"trigger\": \";legado\",\n"
                              "    \"content\": \"Corpo original do snippet legado sem metadados.\"\n"
                              "  }\n"
                              "]";
    fwrite(legacy_json, 1, strlen(legacy_json), f_legacy);
    fclose(f_legacy);

    time_t pre_migration = time(NULL);
    SnippetList migrated_list;
    snippet_list_init(&migrated_list);
    if (!storage_load_snippets(&migrated_list) || migrated_list.count != 1) {
        printf("    [FALHA] Falha ao carregar e migrar snippet legado.\n");
        snippet_list_free(&migrated_list);
        goto cleanup;
    }

    // Cenario 8: Preservacao de todos os dados antigos durante a migracao
    if (migrated_list.items[0].id != 300 ||
        strcmp(migrated_list.items[0].name, "Snippet Legado Antigo") != 0 ||
        strcmp(migrated_list.items[0].trigger, ";legado") != 0 ||
        strcmp(migrated_list.items[0].content, "Corpo original do snippet legado sem metadados.") != 0) {
        printf("    [FALHA] Cenario 8: Dados antigos foram corrompidos ou alterados durante a migracao!\n");
        snippet_list_free(&migrated_list);
        goto cleanup;
    }
    printf("    [OK] Cenario 8: Todos os dados antigos preservados integralmente na migracao.\n");

    // Cenario 7: Metadados preenchidos automaticamente na migracao
    time_t post_migration = time(NULL);
    if (migrated_list.items[0].created < pre_migration || migrated_list.items[0].created > post_migration ||
        migrated_list.items[0].modified < pre_migration || migrated_list.items[0].modified > post_migration ||
        strcmp(migrated_list.items[0].content_type, "Text") != 0) {
        printf("    [FALHA] Cenario 7: Metadados de migracao nao foram inicializados com o tempo atual.\n");
        snippet_list_free(&migrated_list);
        goto cleanup;
    }
    time_t saved_migrated_created = migrated_list.items[0].created;
    time_t saved_migrated_modified = migrated_list.items[0].modified;
    printf("    [OK] Cenario 7: Snippet legado migrado com sucesso (Created=%lld, Modified=%lld, Type=Text).\n",
           (long long)saved_migrated_created, (long long)saved_migrated_modified);
    snippet_list_free(&migrated_list);

    // Cenario 9: Comportamento correto para registros ja migrados, sem reinicializar suas datas em cada inicializacao
    Sleep(1100);
    SnippetList idempotent_list;
    snippet_list_init(&idempotent_list);
    if (!storage_load_snippets(&idempotent_list) || idempotent_list.count != 1) {
        printf("    [FALHA] Falha ao recarregar snippet ja migrado.\n");
        snippet_list_free(&idempotent_list);
        goto cleanup;
    }
    if (idempotent_list.items[0].created != saved_migrated_created ||
        idempotent_list.items[0].modified != saved_migrated_modified) {
        printf("    [FALHA] Cenario 9: Metadados foram reinicializados em carregamento subsequente!\n");
        printf("            Esperado: Created=%lld, Modified=%lld\n", (long long)saved_migrated_created, (long long)saved_migrated_modified);
        printf("            Obtido:   Created=%lld, Modified=%lld\n", (long long)idempotent_list.items[0].created, (long long)idempotent_list.items[0].modified);
        snippet_list_free(&idempotent_list);
        goto cleanup;
    }
    printf("    [OK] Cenario 9: Idempotencia comprovada! Snippets migrados nao reinicializam datas nas inicializacoes seguintes.\n");
    snippet_list_free(&idempotent_list);

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
