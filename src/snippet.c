#include "snippet.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void snippet_list_init(SnippetList *list) {
    if (!list) return;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void snippet_init_metadata(Snippet *snippet) {
    if (!snippet) return;
    snprintf(snippet->content_type, sizeof(snippet->content_type), "Text");
    time_t now = time(NULL);
    snippet->created = now;
    snippet->modified = now;
}

bool snippet_format_datetime_local(time_t t, wchar_t *out_buf, size_t max_len) {
    if (!out_buf || max_len == 0) return false;
    if (t <= 0) {
        wcsncpy(out_buf, L"—", max_len - 1);
        out_buf[max_len - 1] = L'\0';
        return true;
    }

    struct tm *lt = localtime(&t);
    if (!lt) {
        wcsncpy(out_buf, L"—", max_len - 1);
        out_buf[max_len - 1] = L'\0';
        return false;
    }

    SYSTEMTIME st = {0};
    st.wYear = (WORD)(lt->tm_year + 1900);
    st.wMonth = (WORD)(lt->tm_mon + 1);
    st.wDayOfWeek = (WORD)lt->tm_wday;
    st.wDay = (WORD)lt->tm_mday;
    st.wHour = (WORD)lt->tm_hour;
    st.wMinute = (WORD)lt->tm_min;
    st.wSecond = (WORD)lt->tm_sec;

    wchar_t date_str[64] = {0};
    wchar_t time_str[64] = {0};
    GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date_str, (int)(sizeof(date_str) / sizeof(date_str[0])));
    GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, time_str, (int)(sizeof(time_str) / sizeof(time_str[0])));

    int written = _snwprintf(out_buf, max_len, L"%ls %ls", date_str, time_str);
    if (written < 0 || (size_t)written >= max_len) {
        out_buf[max_len - 1] = L'\0';
        return false;
    }
    return true;
}

bool snippet_format_datetime_iso(time_t t, char *out_buf, size_t max_len) {
    if (!out_buf || max_len == 0) return false;
    if (t <= 0) {
        t = time(NULL);
    }
    struct tm *gmt = gmtime(&t);
    if (!gmt) return false;

    int written = snprintf(out_buf, max_len, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                           gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
                           gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    return (written > 0 && (size_t)written < max_len);
}

time_t snippet_parse_datetime(const char *str) {
    if (!str || str[0] == '\0') return 0;

    // Se for string numerica pura (timestamp unix em segundos)
    char *endptr = NULL;
    long long val = strtoll(str, &endptr, 10);
    if (endptr && *endptr == '\0' && val > 0) {
        return (time_t)val;
    }

    // Tenta formato ISO 8601: "YYYY-MM-DDTHH:MM:SS" ou "YYYY-MM-DD HH:MM:SS"
    int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
    if (sscanf(str, "%d-%d-%d%*c%d:%d:%d", &year, &mon, &day, &hour, &min, &sec) >= 3) {
        struct tm tm_val = {0};
        tm_val.tm_year = year - 1900;
        tm_val.tm_mon = mon - 1;
        tm_val.tm_mday = day;
        tm_val.tm_hour = hour;
        tm_val.tm_min = min;
        tm_val.tm_sec = sec;
        tm_val.tm_isdst = -1;

        if (strchr(str, 'Z') || strchr(str, 'z')) {
            return _mkgmtime(&tm_val);
        } else {
            return mktime(&tm_val);
        }
    }

    return 0;
}

bool snippet_list_add(SnippetList *list, const Snippet *snippet) {
    if (!list || !snippet) return false;

    // Se atingiu a capacidade máxima atual, dobra de tamanho (ou começa com 8)
    if (list->count >= list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        Snippet *new_items = (Snippet *)realloc(list->items, new_capacity * sizeof(Snippet));
        if (!new_items) {
            return false; // Falha na alocação de memória
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    // Copia o novo snippet para o final da lista, garantindo metadados válidos
    Snippet copy = *snippet;
    if (copy.content_type[0] == '\0') {
        snprintf(copy.content_type, sizeof(copy.content_type), "Text");
    }
    if (copy.created == 0) {
        copy.created = time(NULL);
    }
    if (copy.modified == 0) {
        copy.modified = copy.created;
    }

    list->items[list->count] = copy;
    list->count++;
    return true;
}

bool snippet_list_update(SnippetList *list, const Snippet *snippet) {
    if (!list || !snippet) return false;

    Snippet *existing = snippet_list_find_by_id(list, snippet->id);
    if (!existing) return false;

    // Preserva Created original inalterado
    time_t orig_created = (existing->created != 0) ? existing->created : ((snippet->created != 0) ? snippet->created : time(NULL));

    // Verifica se houve alteração em nome, gatilho ou conteúdo
    bool changed = (strcmp(existing->name, snippet->name) != 0 ||
                    strcmp(existing->trigger, snippet->trigger) != 0 ||
                    strcmp(existing->content, snippet->content) != 0);

    time_t new_modified = existing->modified;
    if (snippet->modified != 0 && snippet->modified != existing->modified) {
        new_modified = snippet->modified;
    } else if (changed) {
        new_modified = time(NULL);
    }

    // Atualiza os campos do snippet encontrado mantendo metadados consistentes
    *existing = *snippet;
    existing->created = orig_created;
    existing->modified = new_modified;

    if (existing->content_type[0] == '\0') {
        snprintf(existing->content_type, sizeof(existing->content_type), "Text");
    }

    return true;
}

bool snippet_list_remove(SnippetList *list, int id) {
    if (!list) return false;

    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].id == id) {
            // Desloca os elementos subsequentes para a esquerda
            for (size_t j = i; j < list->count - 1; j++) {
                list->items[j] = list->items[j + 1];
            }
            list->count--;
            return true;
        }
    }

    return false;
}

Snippet *snippet_list_find_by_id(SnippetList *list, int id) {
    if (!list) return NULL;

    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].id == id) {
            return &list->items[i];
        }
    }

    return NULL;
}

Snippet *snippet_list_find_by_trigger(SnippetList *list, const char *trigger) {
    if (!list || !trigger) return NULL;

    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].trigger, trigger) == 0) {
            return &list->items[i];
        }
    }

    return NULL;
}

int snippet_list_get_next_id(const SnippetList *list) {
    if (!list || list->count == 0) return 1;

    int max_id = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].id > max_id) {
            max_id = list->items[i].id;
        }
    }

    return max_id + 1;
}

void snippet_list_free(SnippetList *list) {
    if (!list) return;

    if (list->items) {
        free(list->items);
        list->items = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}
