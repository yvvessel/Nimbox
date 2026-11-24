#ifndef AUTOSTART_H
#define AUTOSTART_H

#include <stdbool.h>
#include <stddef.h>

// Verifica se a inicializacao automatica do Nimbox esta ativa no Registro do usuario atual
bool autostart_is_enabled(void);

// Ativa ou desativa a inicializacao automatica no Registro (HKCU\...\Run\Nimbox)
bool autostart_set_enabled(bool enable);

// Monta o comando completo com aspas e argumento --minimized usando GetModuleFileNameW
bool autostart_build_command(wchar_t *out_cmd, size_t max_chars);

// Funcoes parametrizadas por subchave (para testes automatizados isolados no Registro)
bool autostart_is_enabled_in_key(const wchar_t *subkey);
bool autostart_set_enabled_in_key(const wchar_t *subkey, bool enable);
bool autostart_read_command_in_key(const wchar_t *subkey, wchar_t *out_cmd, size_t max_chars);

#endif // AUTOSTART_H
