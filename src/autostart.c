#include "autostart.h"
#include <windows.h>
#include <wchar.h>

#define AUTOSTART_RUN_KEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define AUTOSTART_VALUE_NAME L"Nimbox"
#define AUTOSTART_ARG       L"--minimized"

bool autostart_build_command(wchar_t *out_cmd, size_t max_chars) {
    if (!out_cmd || max_chars < 16) return false;

    wchar_t exe_path[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return false;
    }

    int written = swprintf(out_cmd, max_chars, L"\"%ls\" %ls", exe_path, AUTOSTART_ARG);
    return (written > 0 && (size_t)written < max_chars);
}

bool autostart_read_command_in_key(const wchar_t *subkey, wchar_t *out_cmd, size_t max_chars) {
    if (!subkey || !out_cmd || max_chars == 0) return false;
    out_cmd[0] = L'\0';

    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_QUERY_VALUE, &hKey);
    if (res != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD byte_size = (DWORD)(max_chars * sizeof(wchar_t));
    res = RegQueryValueExW(hKey, AUTOSTART_VALUE_NAME, NULL, &type, (LPBYTE)out_cmd, &byte_size);
    RegCloseKey(hKey);

    if (res != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        out_cmd[0] = L'\0';
        return false;
    }

    out_cmd[max_chars - 1] = L'\0';
    return (out_cmd[0] != L'\0');
}

bool autostart_is_enabled_in_key(const wchar_t *subkey) {
    wchar_t cmd[MAX_PATH + 32];
    return autostart_read_command_in_key(subkey, cmd, MAX_PATH + 32);
}

bool autostart_set_enabled_in_key(const wchar_t *subkey, bool enable) {
    if (!subkey) return false;

    if (enable) {
        wchar_t cmd[MAX_PATH + 32];
        if (!autostart_build_command(cmd, MAX_PATH + 32)) {
            return false;
        }

        HKEY hKey = NULL;
        LONG res = RegCreateKeyExW(
            HKEY_CURRENT_USER,
            subkey,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            NULL,
            &hKey,
            NULL
        );
        if (res != ERROR_SUCCESS) {
            return false;
        }

        DWORD data_bytes = (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t));
        res = RegSetValueExW(hKey, AUTOSTART_VALUE_NAME, 0, REG_SZ, (const BYTE *)cmd, data_bytes);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    } else {
        HKEY hKey = NULL;
        LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_SET_VALUE, &hKey);
        if (res == ERROR_FILE_NOT_FOUND) {
            return true; // Chave nao existe, logo ja esta desativado
        }
        if (res != ERROR_SUCCESS) {
            return false;
        }

        res = RegDeleteValueW(hKey, AUTOSTART_VALUE_NAME);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND);
    }
}

bool autostart_is_enabled(void) {
    return autostart_is_enabled_in_key(AUTOSTART_RUN_KEY);
}

bool autostart_set_enabled(bool enable) {
    return autostart_set_enabled_in_key(AUTOSTART_RUN_KEY, enable);
}
