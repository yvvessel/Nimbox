# make-installer.ps1 — Compila o instalador EXE do Nimbox via Inno Setup
# Uso: .\dist\make-installer.ps1
# Requisito: Inno Setup 6 ou 7 instalado (iscc.exe no PATH ou no local padrao)

param(
    [string]$Version = "1.0.1"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$dist   = $PSScriptRoot
$issFile = Join-Path $dist "nimbox.iss"

Write-Host "=== Nimbox Installer EXE ===" -ForegroundColor Cyan
Write-Host "Versao : $Version"
Write-Host "Script : $issFile"
Write-Host ""

# 1. Localizar iscc.exe (Inno Setup 6 ou 7)
$iscc = $null
$candidates = @(
    "iscc",
    "C:\Program Files\Inno Setup 7\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 7\ISCC.exe",
    "C:\Program Files\Inno Setup 6\iscc.exe",
    "C:\Program Files (x86)\Inno Setup 6\iscc.exe"
)
foreach ($c in $candidates) {
    if (Test-Path $c -ErrorAction SilentlyContinue) {
        $iscc = $c; break
    }
    if (Get-Command $c -ErrorAction SilentlyContinue) {
        $iscc = $c; break
    }
}

if (-not $iscc) {
    Write-Error "Inno Setup nao encontrado. Instale o Inno Setup 6 ou 7 e adicione iscc.exe ao PATH."
    exit 1
}

Write-Host "iscc encontrado: $iscc"

# 2. Verificar executavel Nimbox.exe
$root = Split-Path $dist -Parent
$exe  = Join-Path $root "Nimbox.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Nimbox.exe nao encontrado em '$root'. Compile o projeto primeiro."
    exit 1
}

# 3. Compilar instalador
Write-Host ""
Write-Host "Compilando instalador..." -ForegroundColor Yellow
& $iscc $issFile /DAppVersion=$Version
if ($LASTEXITCODE -ne 0) {
    Write-Error "Falha ao compilar o instalador (codigo $LASTEXITCODE)."
    exit $LASTEXITCODE
}

# 4. Validar saida
$installer = Join-Path $dist "NimboxSetup-v$Version.exe"
if (Test-Path $installer) {
    $info = Get-Item $installer
    Write-Host ""
    Write-Host "[OK] Instalador gerado: $installer" -ForegroundColor Green
    Write-Host "     Tamanho          : $([math]::Round($info.Length / 1MB, 2)) MB"
} else {
    Write-Warning "Instalador nao encontrado no caminho esperado: $installer"
}
