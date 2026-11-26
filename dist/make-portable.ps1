# make-portable.ps1 — Gera o Portable ZIP do Nimbox
# Uso: .\dist\make-portable.ps1
# Requisito: Nimbox.exe compilado na raiz do projeto

param(
    [string]$Version = "1.0.1"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root   = Split-Path $PSScriptRoot -Parent
$dist   = $PSScriptRoot
$name   = "Nimbox-v$Version-Portable"
$tmpDir = Join-Path $dist $name
$zipOut = Join-Path $dist "$name.zip"

Write-Host "=== Nimbox Portable ZIP ===" -ForegroundColor Cyan
Write-Host "Versao : $Version"
Write-Host "Destino: $zipOut"
Write-Host ""

# 1. Verificar executavel
$exe = Join-Path $root "Nimbox.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Nimbox.exe nao encontrado em '$root'. Compile o projeto primeiro."
    exit 1
}

# 2. Limpar staging anterior
if (Test-Path $tmpDir) { Remove-Item $tmpDir -Recurse -Force }
New-Item -ItemType Directory -Path $tmpDir | Out-Null

# 3. Copiar arquivos do pacote
Copy-Item $exe                              (Join-Path $tmpDir "Nimbox.exe")
Copy-Item (Join-Path $root "LICENSE")       (Join-Path $tmpDir "LICENSE")  -ErrorAction SilentlyContinue
Copy-Item (Join-Path $dist "README-portable.md") (Join-Path $tmpDir "README.md")

# 4. Verificar conteudo
Write-Host "Conteudo do pacote:"
Get-ChildItem $tmpDir | ForEach-Object { Write-Host "  $_" }

# 5. Remover ZIP anterior se existir
if (Test-Path $zipOut) { Remove-Item $zipOut -Force }

# 6. Criar ZIP
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($tmpDir, $zipOut, [System.IO.Compression.CompressionLevel]::Optimal, $true)

# 7. Limpar staging
Remove-Item $tmpDir -Recurse -Force

# 8. Validar
$info = Get-Item $zipOut
Write-Host ""
Write-Host "[OK] ZIP gerado: $zipOut" -ForegroundColor Green
Write-Host "     Tamanho   : $([math]::Round($info.Length / 1KB, 1)) KB"

# 9. Listar conteudo do ZIP
Write-Host ""
Write-Host "Conteudo do ZIP:"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($zipOut)
$zip.Entries | ForEach-Object { Write-Host "  $($_.FullName)  ($([math]::Round($_.Length / 1KB, 1)) KB)" }
$zip.Dispose()
