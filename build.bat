@echo off
echo =======================================
echo Compilando Nimbox...
echo =======================================

windres -i res\app.rc -o res\resources.o

gcc -std=c11 -Wall -Wextra -O2 -Isrc src\cJSON.c src\snippet.c src\storage.c src\gui.c src\main.c res\resources.o -o Nimbox.exe -mwindows -lcomctl32 -lshell32

if %ERRORLEVEL% equ 0 (
    echo.
    echo [SUCESSO] Nimbox compilado com exito! Executavel: Nimbox.exe
) else (
    echo.
    echo [ERRO] Ocorreu uma falha na compilacao.
)
