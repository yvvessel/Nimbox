# Nimbox

> **Aviso:** Este projeto ainda está em desenvolvimento. Recursos, organização interna e interface podem passar por ajustes antes de uma versão estável.

O **Nimbox** é um aplicativo nativo para Windows, escrito em C (Win32), para gerenciamento e expansão automática de snippets de texto.

## Problema que resolve

Digitar repetidamente os mesmos e-mails, assinaturas, respostas padrão, comandos ou blocos de texto consome tempo e favorece erros de digitação. O Nimbox permite associar uma palavra-chave curta (gatilho) a um texto completo e expandi-lo automaticamente em qualquer aplicativo do Windows enquanto você digita.

## Funcionalidades atuais

- **Gerenciamento de snippets (CRUD):** criação, edição, exclusão e listagem em tabela personalizada com pré-visualização.
- **Pesquisa em tempo real:** filtragem por nome ou gatilho diretamente na janela principal.
- **Expansão automática global:** detecção de gatilhos em segundo plano via hook de teclado de baixo nível (`WH_KEYBOARD_LL`) e substituição automática via `SendInput` com suporte a Unicode (acentuação e caracteres especiais) e quebras de linha.
- **Controle de detecção:** botão e opção na bandeja para pausar e retomar a expansão automática a qualquer momento.
- **Bandeja do sistema (System Tray):** ao fechar a janela, o aplicativo continua ativo na bandeja com menu rápido (*Abrir Nimbox*, *Pausar/Retomar detecção*, *Alternar para Tema Claro/Escuro*, *Iniciar com o Windows* e *Sair*).
- **Temas Claro e Escuro:** alternância instantânea com persistência da preferência do usuário.
- **Instância única:** impede múltiplas instâncias simultâneas; tentar abrir o aplicativo novamente restaura a janela já em execução.
- **Inicialização minimizada (`--minimized`) e com o Windows:** suporte ao argumento `--minimized` e integração opcional com a chave de inicialização do usuário atual no Registro do Windows (`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`).
- **Armazenamento local e privacidade:** snippets e configurações são salvos apenas localmente em JSON. O hook mantém somente um buffer temporário pequeno em memória (64 bytes), limpo após disparos, atalhos ou trocas de janela, sem gravar histórico de teclas nem enviar dados pela rede.

## Tecnologias utilizadas

- **Linguagem C (C11)**
- **API Win32 nativa** (Unicode/UTF-16, Common Controls v6, GDI+, DWM/UxTheme, `SetWindowsHookExW`, `SendInput`, `Shell_NotifyIconW`)
- **cJSON** (biblioteca embutida em `src/cJSON.c` e `src/cJSON.h` para serialização JSON)
- **MinGW-w64 / MSYS2 UCRT64** (`gcc`, `windres`, `mingw32-make`)

## Estrutura básica do projeto

```text
Nimbox/
├── assets/
│   └── branding/
│       └── NimboxLogo.png   # Logotipo embutido nos recursos do executável
├── res/
│   ├── app.ico              # Ícone do aplicativo (múltiplas resoluções)
│   ├── app.manifest         # Manifesto Win32 (Common Controls v6 / Windows 10 e 11)
│   └── app.rc               # Script de recursos compilado pelo windres
├── src/
│   ├── autostart.c / .h     # Controle de inicialização com o Windows (HKCU\...\Run)
│   ├── cJSON.c / .h         # Parser e gerador JSON
│   ├── gui.c / .h           # Interface gráfica Win32, modal e bandeja do sistema
│   ├── hook.c / .h          # Hook global de teclado (WH_KEYBOARD_LL) e buffer em memória
│   ├── injector.c / .h      # Substituição do gatilho pelo conteúdo via SendInput Unicode
│   ├── main.c               # Ponto de entrada (WinMain) e loop de mensagens
│   ├── main_test.c          # Testes de armazenamento, configurações e autostart
│   ├── settings.c / .h      # Persistência de configurações (settings.json)
│   ├── snippet.c / .h       # Estrutura de dados e lista dinâmica de snippets
│   ├── storage.c / .h       # Persistência atômica de snippets (snippets.json)
│   └── theme.c / .h         # Paletas e brushes para Tema Escuro e Tema Claro
├── .gitignore
├── build.bat                # Script auxiliar de compilação no Windows
└── Makefile                 # Receita de compilação para mingw32-make
```

## Requisitos para compilação (Windows com MinGW/MSYS2 UCRT64)

- Windows 10 ou Windows 11.
- **MSYS2** com o toolchain **UCRT64** instalado (incluindo `gcc`, `windres` e `mingw32-make`).
- O diretório `bin` do UCRT64 (ex.: `C:\msys64\ucrt64\bin`) configurado no `PATH` do sistema ou utilizando o terminal **MSYS2 UCRT64**.

## Como compilar

No diretório raiz do repositório, execute:

```bash
mingw32-make clean
mingw32-make
```

Para compilar e executar os testes automatizados (`test_storage.exe`):

```bash
mingw32-make test
```

> **Observação:** Os testes executados por `mingw32-make test` utilizam um diretório temporário exclusivo (que é removido ao final) e uma chave isolada no Registro (`HKCU\Software\NimboxTest\Run`, também removida ao final), sem alterar seus dados reais em `%APPDATA%\Nimbox` nem habilitar a inicialização automática.

Como alternativa no Prompt de Comando do Windows, também é possível compilar executando `build.bat`.

## Como executar

Após a compilação, execute o binário gerado na raiz do projeto:

```powershell
.\Nimbox.exe
```

Para iniciar diretamente oculto na bandeja do sistema:

```powershell
.\Nimbox.exe --minimized
```

## Onde os dados são armazenados

Os arquivos de dados do usuário são salvos localmente no diretório de perfil do Windows:

- **Snippets:** `%APPDATA%\Nimbox\snippets.json`
- **Configurações:** `%APPDATA%\Nimbox\settings.json`
