#ifndef SNIPPET_H
#define SNIPPET_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <wchar.h>

// Limites confortáveis para os campos de um snippet
#define MAX_NAME_LEN 64
#define MAX_TRIGGER_LEN 32
#define MAX_CONTENT_LEN 4096
#define MAX_CONTENT_TYPE_LEN 16

// Estrutura que representa um único snippet com metadados
typedef struct {
    int id;                                  // Identificador único (ex: 1, 2, 3...)
    char name[MAX_NAME_LEN];                 // Nome amigável (ex: "Email pessoal")
    char trigger[MAX_TRIGGER_LEN];           // Palavra-chave/gatilho (ex: "emailpessoal" ou ";email")
    char content[MAX_CONTENT_LEN];           // Texto a ser inserido no lugar do gatilho
    char content_type[MAX_CONTENT_TYPE_LEN]; // Tipo de conteúdo (padrão: "Text")
    time_t created;                          // Data e hora de criação (timestamp)
    time_t modified;                         // Data e hora da última modificação (timestamp)
} Snippet;

// Lista dinâmica de snippets (array dinâmico redimensionável)
typedef struct {
    Snippet *items;                 // Ponteiro para o array de snippets alocado no heap
    size_t count;                   // Quantidade atual de snippets cadastrados
    size_t capacity;                // Capacidade total antes de precisar de realloc
} SnippetList;

// Inicializa uma lista de snippets vazia
void snippet_list_init(SnippetList *list);

// Inicializa os metadados de um snippet (content_type="Text", created=agora, modified=agora)
void snippet_init_metadata(Snippet *snippet);

// Formata uma data/hora no formato regional do Windows (inclui data e hora locais)
bool snippet_format_datetime_local(time_t t, wchar_t *out_buf, size_t max_len);

// Formata uma data/hora no formato padrão ISO-8601 UTC (ex: "2026-11-25T14:05:49Z")
bool snippet_format_datetime_iso(time_t t, char *out_buf, size_t max_len);

// Converte uma string ISO-8601 ou timestamp numérico para time_t
time_t snippet_parse_datetime(const char *str);

// Adiciona um snippet à lista (faz realloc da capacidade automaticamente se necessário)
bool snippet_list_add(SnippetList *list, const Snippet *snippet);

// Atualiza os dados de um snippet existente pelo seu ID (preserva Created e atualiza Modified)
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
