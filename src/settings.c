#include "settings.h"
#include "cJSON.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char s_custom_dir[MAX_PATH] = {0};

void settings_set_custom_dir(const char *dir_path) {
    if (!dir_path || dir_path[0] == '\0') {
        s_custom_dir[0] = '\0';
        return;
    }
    snprintf(s_custom_dir, sizeof(s_custom_dir), "%s", dir_path);
}

static bool settings_get_path(char *out_path, size_t max_len) {
    if (!out_path || max_len == 0) return false;

    char dir[MAX_PATH + 64];
    if (s_custom_dir[0] != '\0') {
        snprintf(dir, sizeof(dir), "%s", s_custom_dir);
    } else {
        char appdata[MAX_PATH];
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
            const char *env = getenv("APPDATA");
            if (!env) return false;
            strncpy(appdata, env, MAX_PATH - 1);
            appdata[MAX_PATH - 1] = '\0';
        }

        snprintf(dir, sizeof(dir), "%.200s\\Nimbox", appdata);
    }

    CreateDirectoryA(dir, NULL);

    int written = snprintf(out_path, max_len, "%.240s\\settings.json", dir);
    return (written > 0 && (size_t)written < max_len);
}

void settings_init_default(AppSettings *settings) {
    if (!settings) return;
    settings->theme = THEME_DARK;
    settings->start_with_windows = false;
}

bool settings_load(AppSettings *settings) {
    if (!settings) return false;
    settings_init_default(settings);

    char path[MAX_PATH];
    if (!settings_get_path(path, sizeof(path))) return false;

    FILE *f = fopen(path, "rb");
    if (!f) {
        // Arquivo ainda nao existe: salva os padroes
        settings_save(settings);
        return true;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        return false;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buf, 1, sz, f);
    buf[read_bytes] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) return false;

    cJSON *theme_item = cJSON_GetObjectItemCaseSensitive(json, "theme");
    if (cJSON_IsString(theme_item) && theme_item->valuestring) {
        if (strcmp(theme_item->valuestring, "light") == 0) {
            settings->theme = THEME_LIGHT;
        } else {
            settings->theme = THEME_DARK;
        }
    }

    cJSON *autostart_item = cJSON_GetObjectItemCaseSensitive(json, "start_with_windows");
    if (cJSON_IsBool(autostart_item)) {
        settings->start_with_windows = cJSON_IsTrue(autostart_item);
    }

    cJSON_Delete(json);
    return true;
}

bool settings_save(const AppSettings *settings) {
    if (!settings) return false;

    char path[MAX_PATH];
    if (!settings_get_path(path, sizeof(path))) return false;

    cJSON *json = cJSON_CreateObject();
    if (!json) return false;

    cJSON_AddStringToObject(json, "theme", (settings->theme == THEME_LIGHT) ? "light" : "dark");
    cJSON_AddBoolToObject(json, "start_with_windows", settings->start_with_windows);

    char *rendered = cJSON_Print(json);
    cJSON_Delete(json);
    if (!rendered) return false;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(rendered);
        return false;
    }

    fputs(rendered, f);
    fclose(f);
    free(rendered);
    return true;
}
