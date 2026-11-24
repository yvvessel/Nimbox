#ifndef INJECTOR_H
#define INJECTOR_H

#include "snippet.h"
#include <stdbool.h>

// Realiza a substituicao do snippet no aplicativo ativo:
// 1. Envia pulsos de Backspace para apagar a palavra-chave/gatilho.
// 2. Envia o texto correspondente utilizando eventos Unicode via SendInput.
void injector_replace_snippet(const Snippet *snippet);

// Retorna se o injetor esta atualmente em processo de expansao
bool injector_is_expanding(void);

#endif // INJECTOR_H
