#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    THEME_DARK = 0,
    THEME_LIGHT = 1
} AppTheme;

typedef struct {
    AppTheme theme;
    bool start_with_windows;
} AppSettings;

// Define um diretorio customizado para settings.json (usado em testes).
// Passe NULL ou "" para restaurar o caminho padrao em %APPDATA%\Nimbox.
void settings_set_custom_dir(const char *dir_path);

// Inicializa a estrutura de configuracoes com valores padrao (Tema escuro)
void settings_init_default(AppSettings *settings);

// Carrega as configuracoes de %APPDATA%\Nimbox\settings.json
// Se o arquivo nao existir, preenche com valores padrao e salva
bool settings_load(AppSettings *settings);

// Salva as configuracoes atuais em %APPDATA%\Nimbox\settings.json
bool settings_save(const AppSettings *settings);

#endif // SETTINGS_H
