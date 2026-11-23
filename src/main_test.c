#include <stdio.h>
#include <stdlib.h>
#include "snippet.h"
#include "storage.h"

static void print_snippets(const SnippetList *list) {
    printf("\n=== Snippets Cadastrados (%zu) ===\n", list->count);
    for (size_t i = 0; i < list->count; i++) {
        printf("[%d] \"%s\"\n", list->items[i].id, list->items[i].name);
        printf("    Gatilho:  %s\n", list->items[i].trigger);
        printf("    Conteudo: %s\n\n", list->items[i].content);
    }
}

int main(void) {
    printf("=========================================\n");
    printf("    Nimbox - Teste de Armazenamento     \n");
    printf("=========================================\n\n");

    char path[260];
    if (storage_get_file_path(path, sizeof(path))) {
        printf("[OK] Caminho do arquivo identificado:\n     %s\n\n", path);
    } else {
        printf("[ERRO] Nao foi possivel obter o caminho do AppData.\n");
        return 1;
    }

    // 1. Inicializa a lista
    SnippetList list;
    snippet_list_init(&list);

    // 2. Carrega do disco (se nao existir, o storage cria os padroes)
    printf("[*] Carregando snippets do disco...\n");
    if (storage_load_snippets(&list)) {
        printf("[OK] Snippets carregados com sucesso!\n");
        print_snippets(&list);
    } else {
        printf("[ERRO] Falha ao carregar snippets.\n");
        snippet_list_free(&list);
        return 1;
    }

    // 3. Adiciona um novo snippet para testar a persistencia
    int next_id = snippet_list_get_next_id(&list);
    Snippet novo = {
        .id = next_id,
        .name = "Teste Dinamico",
        .trigger = "!teste",
        .content = "Texto gerado com sucesso pelo Nimbox na Etapa 1!"
    };

    printf("[*] Adicionando novo snippet de teste (ID %d)...\n", novo.id);
    snippet_list_add(&list, &novo);

    // 4. Salva a lista atualizada no disco
    printf("[*] Salvando dados atualizados no disco...\n");
    if (storage_save_snippets(&list)) {
        printf("[OK] Arquivo JSON gravado com sucesso!\n");
    } else {
        printf("[ERRO] Falha ao gravar no arquivo JSON.\n");
    }

    // 5. Libera a memoria atual para simular fechamento do app
    snippet_list_free(&list);

    // 6. Recarrega do zero para comprovar que os dados foram realmente persistidos
    printf("\n[*] Simulando reinicializacao do aplicativo...\n");
    SnippetList nova_sessao;
    snippet_list_init(&nova_sessao);

    if (storage_load_snippets(&nova_sessao)) {
        printf("[OK] Dados recarregados do disco com sucesso!\n");
        print_snippets(&nova_sessao);
    } else {
        printf("[ERRO] Falha ao recarregar dados persistidos.\n");
    }

    snippet_list_free(&nova_sessao);

    printf("=========================================\n");
    printf("  [SUCESSO] Etapa 1 concluida com exito! \n");
    printf("=========================================\n");

    return 0;
}
