#ifndef STORAGE_H
#define STORAGE_H

#include "snippet.h"
#include <stdbool.h>

// Define um diretorio customizado para armazenamento (usado em testes).
// Passe NULL ou "" para restaurar o caminho padrao em %APPDATA%\Nimbox.
void storage_set_custom_dir(const char *dir_path);

// Obtém o caminho completo para o arquivo snippets.json dentro de %APPDATA%\Nimbox
bool storage_get_file_path(char *out_path, size_t max_len);

// Carrega os snippets do arquivo JSON. Se o arquivo não existir, cria
// a pasta e inicializa com alguns snippets de exemplo padrão.
bool storage_load_snippets(SnippetList *list);

// Salva a lista de snippets atual no arquivo JSON.
bool storage_save_snippets(const SnippetList *list);

#endif // STORAGE_H
