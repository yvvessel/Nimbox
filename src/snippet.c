#include "snippet.h"
#include <stdlib.h>
#include <string.h>

void snippet_list_init(SnippetList *list) {
    if (!list) return;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
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

    // Copia o novo snippet para o final da lista
    list->items[list->count] = *snippet;
    list->count++;
    return true;
}

bool snippet_list_update(SnippetList *list, const Snippet *snippet) {
    if (!list || !snippet) return false;

    Snippet *existing = snippet_list_find_by_id(list, snippet->id);
    if (!existing) return false;

    // Atualiza os campos do snippet encontrado
    *existing = *snippet;
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
