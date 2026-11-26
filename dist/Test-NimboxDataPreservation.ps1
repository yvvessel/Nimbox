# Test-NimboxDataPreservation.ps1
# Valida que o instalador/desinstalador do Nimbox preserva %APPDATA%\Nimbox
#
# SEGURANCA: Este script NAO modifica %APPDATA%\Nimbox.
# Ele computa hashes antes e depois das operacoes e compara.
#
# Uso:
#   .\Test-NimboxDataPreservation.ps1
#   .\Test-NimboxDataPreservation.ps1 -TestUninstall
#
# Requer: NimboxSetup-v1.0.0.exe compilado em dist\

param(
    [switch]$TestUninstall,       # Executar tambem o teste de desinstalacao
    [string]$InstallDir = "$env:TEMP\NimboxTest_Install_$(Get-Random)"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$installer = "$PSScriptRoot\NimboxSetup-v1.0.0.exe"
$dataDir   = "$env:APPDATA\Nimbox"
$logFile   = "$PSScriptRoot\test-preservation.log"
$pass = 0; $fail = 0

function Log { param($msg, $color = "White")
    $ts = Get-Date -Format "HH:mm:ss"
    $line = "[$ts] $msg"
    Write-Host $line -ForegroundColor $color
    Add-Content $logFile $line
}

function Assert { param($desc, [bool]$condition)
    if ($condition) {
        Log "  [PASS] $desc" "Green"; $script:pass++
    } else {
        Log "  [FAIL] $desc" "Red";  $script:fail++
    }
}

function Get-DirHash {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $null }
    $files = Get-ChildItem $Path -Recurse -File | Sort-Object FullName
    $sb = [System.Text.StringBuilder]::new()
    foreach ($f in $files) {
        $h = (Get-FileHash $f.FullName -Algorithm SHA256).Hash
        [void]$sb.AppendLine("$($f.Name)|$($f.Length)|$h")
    }
    return $sb.ToString()
}

# =============================================================================
Set-Content $logFile "=== Nimbox Data Preservation Test $(Get-Date) ==="
Log "=== Nimbox — Teste de Preservacao de Dados ===" "Cyan"
Log "Installer : $installer"
Log "DataDir   : $dataDir"
Log "InstallDir: $InstallDir"
Log ""

# 1. Verificar pre-requisitos
if (-not (Test-Path $installer)) {
    Log "[ERRO] Installer nao encontrado: $installer" "Red"; exit 1
}
Log "[1] Pre-requisitos OK"

# 2. Snapshot dos dados existentes
Log ""
Log "[2] Snapshot dos dados existentes em %APPDATA%\Nimbox..."
$dataExisteAntes = Test-Path $dataDir
$hashAntes = Get-DirHash $dataDir
if ($dataExisteAntes) {
    $arquivos = Get-ChildItem $dataDir | Select-Object Name,Length
    Log "    Arquivos encontrados:"
    $arquivos | ForEach-Object { Log "      $($_.Name) ($($_.Length) bytes)" }
    Log "    Hash do diretorio: $($hashAntes.GetHashCode())"
} else {
    Log "    %APPDATA%\Nimbox nao existe (usuario sem dados previos)"
}

# 3. Instalacao silenciosa em diretorio de teste
Log ""
Log "[3] Instalando Nimbox em diretorio de teste: $InstallDir"
Log "    (modo /VERYSILENT — nao exibe janelas)"

$proc = Start-Process $installer `
    -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /DIR=`"$InstallDir`" /NOICONS /NORESTART" `
    -Wait -PassThru

Log "    Codigo de saida: $($proc.ExitCode)"
Assert "Instalacao concluida sem erro (exit 0)" ($proc.ExitCode -eq 0)

# 4. Verificar que o executavel foi instalado no diretorio correto
$installedExe = "$InstallDir\Nimbox.exe"
Assert "Nimbox.exe instalado em diretorio de teste" (Test-Path $installedExe)
if (Test-Path $installedExe) {
    Log "    Executavel: $(Get-Item $installedExe | Select-Object -ExpandProperty Length) bytes"
}

# 5. Verificar que %APPDATA%\Nimbox NAO foi modificado
Log ""
Log "[5] Verificando integridade de %APPDATA%\Nimbox apos instalacao..."
$hashAposInstalacao = Get-DirHash $dataDir
$dataExisteApos = Test-Path $dataDir

Assert "Existencia de %APPDATA%\Nimbox igual antes e apos instalacao" `
    ($dataExisteAntes -eq $dataExisteApos)

if ($dataExisteAntes -and $dataExisteApos) {
    Assert "Hash de %APPDATA%\Nimbox identico (dados nao tocados)" `
        ($hashAntes -eq $hashAposInstalacao)
    if ($hashAntes -ne $hashAposInstalacao) {
        Log "    [!] HASH DIFERENTE — dados foram modificados!" "Red"
    }
} elseif (-not $dataExisteAntes -and $dataExisteApos) {
    Log "    [!] ATENCAO: diretorio criado pelo instalador (nao havia dados antes)" "Yellow"
    Assert "Diretorio criado sem arquivo (apenas estrutura, sem snippets)" `
        ((Get-ChildItem $dataDir).Count -eq 0)
}

# 6. Simular atualizacao (segunda instalacao sobre a primeira)
Log ""
Log "[6] Simulando atualizacao (segunda instalacao sobre a mesma pasta)..."

# Criar dados de teste em APPDATA apenas se nao existir (nao destruir dados reais)
$dadosTeste = $null
if (-not $dataExisteAntes) {
    Log "    Criando dados de teste em %APPDATA%\Nimbox para simular usuario com dados..."
    New-Item -ItemType Directory -Path $dataDir -Force | Out-Null
    $dadosTeste = @{
        snippets = @(
            @{ id=99; name="teste"; trigger="__teste__"; content="Dado de teste para validacao"; content_type="Text"; created="2026-01-01T00:00:00Z"; modified="2026-01-01T00:00:00Z" }
        )
    } | ConvertTo-Json -Depth 5
    Set-Content "$dataDir\snippets.json" $dadosTeste -Encoding UTF8
    Set-Content "$dataDir\settings.json" '{"theme":"dark","start_with_windows":false}' -Encoding UTF8
    Log "    Dados de teste criados."
}

$hashPreUpdate = Get-DirHash $dataDir

$proc2 = Start-Process $installer `
    -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /DIR=`"$InstallDir`" /NOICONS /NORESTART" `
    -Wait -PassThru

Log "    Codigo de saida: $($proc2.ExitCode)"
Assert "Re-instalacao (atualizacao) concluida sem erro" ($proc2.ExitCode -eq 0)

$hashPosUpdate = Get-DirHash $dataDir
Assert "Dados em %APPDATA%\Nimbox preservados apos atualizacao" `
    ($hashPreUpdate -eq $hashPosUpdate)

if ($hashPreUpdate -ne $hashPosUpdate) {
    Log "    [!] HASH DIFERENTE apos atualizacao!" "Red"
} else {
    Log "    Hash igual antes e depois da atualizacao — dados preservados." "Green"
}

# 7. Localizar e testar desinstalador
Log ""
if ($TestUninstall) {
    Log "[7] Teste de desinstalacao (INTERATIVO — requer acao do usuario)..."

    # Localizar uninstall.exe
    $uninstExe = Get-ChildItem "$InstallDir\unins*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $uninstExe) {
        # Verificar no registro
        $regPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
        $entry = Get-ChildItem $regPath | Get-ItemProperty | Where-Object { $_.DisplayName -match "Nimbox" } | Select-Object -First 1
        if ($entry) { $uninstExe = $entry.UninstallString }
    }

    if ($uninstExe) {
        Log "    Desinstalador: $($uninstExe.FullName)"
        Log ""
        Log "    *** ACAO NECESSARIA DO USUARIO ***" "Yellow"
        Log "    O desinstalador sera aberto. Quando aparecer a pergunta sobre excluir" "Yellow"
        Log "    snippets e configuracoes, clique em NAO (preservar dados)." "Yellow"
        Log "    Aguardando conclusao..." "Yellow"
        Log ""

        $hashPreUninstall = Get-DirHash $dataDir

        $proc3 = Start-Process $uninstExe.FullName -Wait -PassThru
        Log "    Codigo de saida: $($proc3.ExitCode)"

        $hashPosUninstall = Get-DirHash $dataDir
        Assert "Dados em %APPDATA%\Nimbox preservados apos desinstalacao (opcao NAO)" `
            ($hashPreUninstall -eq $hashPosUninstall)

        if ($hashPreUninstall -ne $hashPosUninstall) {
            Log "    [!] HASH DIFERENTE apos desinstalacao!" "Red"
        } else {
            Log "    Hash igual — dados preservados pela escolha NAO." "Green"
        }
    } else {
        Log "    Desinstalador nao encontrado em $InstallDir" "Yellow"
        Assert "Desinstalador localizado" $false
    }
} else {
    Log "[7] Teste de desinstalacao IGNORADO (use -TestUninstall para executar interativamente)"
}

# 8. Limpar instalacao de teste
Log ""
Log "[8] Limpando instalacao de teste..."
if (Test-Path $InstallDir) {
    # NOTA: Nao invocar unins*.exe silenciosamente — o instalador usa PrivilegesRequired=admin
    # e o uninstaller exige aprovacao UAC mesmo com /VERYSILENT, causando travamento.
    # Para limpeza de diretorio de teste, Remove-Item direto e suficiente e seguro.
    Log "    Removendo diretorio de instalacao de teste diretamente (sem invocar uninstaller)..."
    Remove-Item $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
    if (Test-Path $InstallDir) {
        Log "    [AVISO] Alguns arquivos nao puderam ser removidos (possivelmente em uso). Ignorar para fins de teste." "Yellow"
    } else {
        Log "    Diretorio de instalacao de teste removido."
    }
}

# Remover dados de teste se foram criados por este script
if ($dadosTeste -ne $null) {
    Log "    Removendo dados de teste criados por este script em %APPDATA%\Nimbox..."
    Remove-Item $dataDir -Recurse -Force -ErrorAction SilentlyContinue
    Log "    Dados de teste removidos."
}

# 9. Verificacao final de %APPDATA%\Nimbox
Log ""
Log "[9] Verificacao final de %APPDATA%\Nimbox..."
$hashFinal = Get-DirHash $dataDir
if ($dataExisteAntes) {
    Assert "%APPDATA%\Nimbox final identico ao baseline (dados reais preservados)" `
        ($hashAntes -eq $hashFinal)
    if ($hashAntes -ne $hashFinal) {
        Log "    [!] Os dados REAIS do usuario foram alterados! Investigar imediatamente." "Red"
    }
} else {
    $dataExisteFinal = Test-Path $dataDir
    Assert "%APPDATA%\Nimbox removido apos limpeza (nao existia antes)" (-not $dataExisteFinal)
}

# 10. Resultado
Log ""
Log "============================================" "Cyan"
Log "  RESULTADO: $pass PASS | $fail FAIL" $(if ($fail -eq 0) {"Green"} else {"Red"})
Log "  Log: $logFile" "Cyan"
Log "============================================" "Cyan"

if ($fail -gt 0) { exit 1 }
