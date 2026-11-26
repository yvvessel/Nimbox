# build-all.ps1 — Compila o Nimbox e gera os dois pacotes de distribuicao
# Uso: .\dist\build-all.ps1
# Deve ser executado a partir do diretorio raiz do projeto ou de dist/

param(
    [string]$Version = "1.0.1",
    [switch]$SkipBuild,
    [switch]$SkipPortable,
    [switch]$SkipInstaller
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$dist = $PSScriptRoot
$root = Split-Path $dist -Parent

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Nimbox — Build e Distribuicao v$Version  " -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# 1. Compilar aplicacao
if (-not $SkipBuild) {
    Write-Host "[1/3] Compilando Nimbox..." -ForegroundColor Yellow
    Push-Location $root
    try {
        & mingw32-make all 2>&1 | Write-Host
        if ($LASTEXITCODE -ne 0) { throw "Build falhou (codigo $LASTEXITCODE)" }
    } finally {
        Pop-Location
    }
    Write-Host "[1/3] Build concluido." -ForegroundColor Green
} else {
    Write-Host "[1/3] Build ignorado (--SkipBuild)." -ForegroundColor DarkGray
}

Write-Host ""

# 2. Gerar Portable ZIP
if (-not $SkipPortable) {
    Write-Host "[2/3] Gerando Portable ZIP..." -ForegroundColor Yellow
    & "$dist\make-portable.ps1" -Version $Version
    Write-Host "[2/3] Portable ZIP concluido." -ForegroundColor Green
} else {
    Write-Host "[2/3] Portable ZIP ignorado (--SkipPortable)." -ForegroundColor DarkGray
}

Write-Host ""

# 3. Gerar Installer EXE
if (-not $SkipInstaller) {
    Write-Host "[3/3] Gerando Installer EXE..." -ForegroundColor Yellow
    & "$dist\make-installer.ps1" -Version $Version
    Write-Host "[3/3] Installer EXE concluido." -ForegroundColor Green
} else {
    Write-Host "[3/3] Installer EXE ignorado (--SkipInstaller)." -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Distribuicao concluida!                  " -ForegroundColor Cyan
Write-Host "  Artefatos em: $dist"                        -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
