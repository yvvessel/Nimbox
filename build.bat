@echo off
echo =======================================
echo Compilando Nimbox (Teste de Armazenamento)...
echo =======================================

gcc -std=c11 -Wall -Wextra -O2 -Isrc src\cJSON.c src\snippet.c src\storage.c src\main_test.c -o test_storage.exe -lshell32

if %ERRORLEVEL% equ 0 (
    echo.
    echo [SUCESSO] Compilado com exito! Executavel gerado: test_storage.exe
    echo Executando teste...
    echo.
    test_storage.exe
) else (
    echo.
    echo [ERRO] Ocorreu uma falha na compilacao.
)
