@echo off
echo =======================================
echo Compilando Nimbox...
echo =======================================

windres -i res\app.rc -o res\resources.o
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERRO] Ocorreu uma falha ao compilar os recursos ^(windres^).
    exit /b %ERRORLEVEL%
)

gcc -std=c11 -Wall -Wextra -O2 -Isrc -finput-charset=UTF-8 -fexec-charset=UTF-8 src\cJSON.c src\snippet.c src\storage.c src\settings.c src\autostart.c src\theme.c src\gui.c src\hook.c src\injector.c src\main.c res\resources.o -o Nimbox.exe -mwindows -lcomctl32 -lshell32 -ladvapi32 -ldwmapi -luxtheme -lgdiplus -lole32

if %ERRORLEVEL% equ 0 (
    echo.
    echo [SUCESSO] Nimbox compilado com exito! Executavel: Nimbox.exe
) else (
    echo.
    echo [ERRO] Ocorreu uma falha na compilacao.
    exit /b %ERRORLEVEL%
)
