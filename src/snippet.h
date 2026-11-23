#ifndef SNIPPET_H
#define SNIPPET_H

#include <stdbool.h>
#include <stddef.h>

// Limites confortáveis para os campos de um snippet
#define MAX_NAME_LEN 64
#define MAX_TRIGGER_LEN 32
#define MAX_CONTENT_LEN 4096

// Estrutura que representa um único snippet
typedef struct {
    int id;                         // Identificador único (ex: 1, 2, 3...)
    char name[MAX_NAME_LEN];        // Nome amigável (ex: "Email pessoal")
    char trigger[MAX_TRIGGER_LEN];  // Palavra-chave/gatilho (ex: "emailpessoal" ou ";email")
    char content[MAX_CONTENT_LEN];  // Texto a ser inserido no lugar do gatilho
} Snippet;

// Lista dinâmica de snippets (array dinâmico redimensionável)
typedef struct {
    Snippet *items;                 // Ponteiro para o array de snippets alocado no heap
    size_t count;                   // Quantidade atual de snippets cadastrados
    size_t capacity;                // Capacidade total antes de precisar de realloc
} SnippetList;

// Inicializa uma lista de snippets vazia
void snippet_list_init(SnippetList *list);

// Adiciona um snippet à lista (faz realloc da capacidade automaticamente se necessário)
bool snippet_list_add(SnippetList *list, const Snippet *snippet);

// Atualiza os dados de um snippet existente pelo seu ID
bool snippet_list_update(SnippetList *list, const Snippet *snippet);

// Remove um snippet da lista pelo seu ID
bool snippet_list_remove(SnippetList *list, int id);

// Busca um snippet pelo ID (retorna ponteiro para o elemento ou NULL se não encontrar)
Snippet *snippet_list_find_by_id(SnippetList *list, int id);

// Busca um snippet pelo gatilho exato (retorna ponteiro ou NULL)
Snippet *snippet_list_find_by_trigger(SnippetList *list, const char *trigger);

// Retorna o próximo ID incremental disponível
int snippet_list_get_next_id(const SnippetList *list);

// Libera a memória alocada pela lista dinâmica
void snippet_list_free(SnippetList *list);

#endif // SNIPPET_H
