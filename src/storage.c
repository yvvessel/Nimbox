#include "storage.h"
#include "cJSON.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool storage_get_file_path(char *out_path, size_t max_len) {
    if (!out_path || max_len == 0) return false;

    char appdata_path[MAX_PATH];

    // Busca o caminho padrao da pasta AppData\Roaming do usuario atual
    HRESULT hr = SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata_path);
    if (FAILED(hr)) {
        // Fallback para a variavel de ambiente se a API falhar
        const char *env_appdata = getenv("APPDATA");
        if (!env_appdata) return false;
        strncpy(appdata_path, env_appdata, MAX_PATH - 1);
        appdata_path[MAX_PATH - 1] = '\0';
    }

    // Cria a pasta "Nimbox" dentro de AppData caso ainda nao exista
    char dir_path[MAX_PATH + 32];
    snprintf(dir_path, sizeof(dir_path), "%s\\Nimbox", appdata_path);
    CreateDirectoryA(dir_path, NULL);

    // Monta o caminho completo do arquivo: %APPDATA%\Nimbox\snippets.json
    snprintf(out_path, max_len, "%s\\snippets.json", dir_path);
    return true;
}

static void create_default_snippets(SnippetList *list) {
    Snippet s1 = {
        .id = 1,
        .name = "Email pessoal",
        .trigger = "emailpessoal",
        .content = "aura.123@gmail.com"
    };

    Snippet s2 = {
        .id = 2,
        .name = "Meu endereço",
        .trigger = ";meuendereco",
        .content = "Rua das Flores, 123 - Centro"
    };

    Snippet s3 = {
        .id = 3,
        .name = "Chave Pix",
        .trigger = "!pix",
        .content = "aura.123@gmail.com"
    };

    snippet_list_add(list, &s1);
    snippet_list_add(list, &s2);
    snippet_list_add(list, &s3);
}

bool storage_load_snippets(SnippetList *list) {
    if (!list) return false;

    char file_path[MAX_PATH];
    if (!storage_get_file_path(file_path, sizeof(file_path))) {
        return false;
    }

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        // Arquivo ainda não existe (primeira execução): cria dados de exemplo e salva
        create_default_snippets(list);
        storage_save_snippets(list);
        return true;
    }

    // Descobre o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return false;
    }

    // Aloca memória e lê todo o conteúdo para uma string
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return false;
    }

    size_t read_bytes = fread(buffer, 1, file_size, file);
    buffer[read_bytes] = '\0';
    fclose(file);

    // Converte o texto JSON em árvore de objetos cJSON
    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (!json || !cJSON_IsArray(json)) {
        if (json) cJSON_Delete(json);
        return false;
    }

    // Itera por cada item do array JSON e popula a nossa SnippetList
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, json) {
        cJSON *id_item = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *name_item = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *trigger_item = cJSON_GetObjectItemCaseSensitive(item, "trigger");
        cJSON *content_item = cJSON_GetObjectItemCaseSensitive(item, "content");

        if (cJSON_IsNumber(id_item) &&
            cJSON_IsString(name_item) && (name_item->valuestring != NULL) &&
            cJSON_IsString(trigger_item) && (trigger_item->valuestring != NULL) &&
            cJSON_IsString(content_item) && (content_item->valuestring != NULL)) {

            Snippet s;
            s.id = id_item->valueint;

            strncpy(s.name, name_item->valuestring, MAX_NAME_LEN - 1);
            s.name[MAX_NAME_LEN - 1] = '\0';

            strncpy(s.trigger, trigger_item->valuestring, MAX_TRIGGER_LEN - 1);
            s.trigger[MAX_TRIGGER_LEN - 1] = '\0';

            strncpy(s.content, content_item->valuestring, MAX_CONTENT_LEN - 1);
            s.content[MAX_CONTENT_LEN - 1] = '\0';

            snippet_list_add(list, &s);
        }
    }

    cJSON_Delete(json);
    return true;
}

bool storage_save_snippets(const SnippetList *list) {
    if (!list) return false;

    char file_path[MAX_PATH];
    if (!storage_get_file_path(file_path, sizeof(file_path))) {
        return false;
    }

    // Cria o nó raiz como um array JSON: [ { ... }, { ... } ]
    cJSON *root = cJSON_CreateArray();
    if (!root) return false;

    for (size_t i = 0; i < list->count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) continue;

        cJSON_AddNumberToObject(item, "id", list->items[i].id);
        cJSON_AddStringToObject(item, "name", list->items[i].name);
        cJSON_AddStringToObject(item, "trigger", list->items[i].trigger);
        cJSON_AddStringToObject(item, "content", list->items[i].content);

        cJSON_AddItemToArray(root, item);
    }

    // Converte a estrutura cJSON para texto formatado legível
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_string) return false;

    // Escrita segura em arquivo temporario para evitar perda de dados
    char temp_path[MAX_PATH + 16];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", file_path);

    FILE *file = fopen(temp_path, "wb");
    if (!file) {
        cJSON_free(json_string);
        return false;
    }

    fputs(json_string, file);
    fclose(file);
    cJSON_free(json_string);

    // Substitui o arquivo final atomicamente
    MoveFileExA(temp_path, file_path, MOVEFILE_REPLACE_EXISTING);
    return true;
}
