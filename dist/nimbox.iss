; Nimbox Installer — Inno Setup Script
; Gera NimboxSetup-vX.Y.Z.exe
; Requisito: Inno Setup 6 ou 7 (iscc.exe no PATH)

#define AppName      "Nimbox"
#define AppVersion   "1.0.1"
#define AppPublisher "Nimbox"
#define AppURL       "https://github.com/thiagopollachini/nimbox"
#define AppExeName   "Nimbox.exe"
#define AppDataDir   "{userappdata}\Nimbox"

[Setup]
AppId={{F4A2C1B3-8D7E-4F9A-B2C5-1E3D6A7F8B9C}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
; Nao exige privilegios de administrador quando instalado em pasta do usuario
; PrivilegesRequired=lowest
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=.
OutputBaseFilename=NimboxSetup-v{#AppVersion}
SetupIconFile=..\res\app.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
; Metadados de versao no instalador
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Installer
; Unicode
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
; Nao criar chave de desinstalacao separada — usamos a padrao do Inno Setup
ChangesAssociations=no

[Languages]
Name: "portuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na Area de Trabalho"; GroupDescription: "Atalhos adicionais:"; Flags: unchecked

[Files]
; Executavel principal
Source: "..\Nimbox.exe"; DestDir: "{app}"; Flags: ignoreversion
; Licenca
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
; README
Source: "..\README.md"; DestDir: "{app}"; DestName: "README.md"; Flags: ignoreversion

[Icons]
; Atalho no Menu Iniciar
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"
Name: "{group}\Desinstalar {#AppName}"; Filename: "{uninstallexe}"
; Atalho na Area de Trabalho (opcional, desmarcado por padrao)
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; Oferecer execucao apos instalacao
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Garantir que o processo seja encerrado antes da desinstalacao
Filename: "{cmd}"; Parameters: "/C taskkill /F /IM {#AppExeName} 2>nul"; Flags: runhidden; RunOnceId: "KillNimbox"

[Code]
// =========================================================================
// Desinstalacao: perguntar se o usuario deseja remover dados pessoais
// =========================================================================

// Exibido antes da desinstalacao comecar
function InitializeUninstall(): Boolean;
var
  MsgResult: Integer;
  NL: String;
  DataDir: String;
  Msg1: String;
  Msg2: String;
begin
  Result := True;
  NL := Chr(13) + Chr(10);
  DataDir := ExpandConstant('{userappdata}\Nimbox');

  // Verificar se o diretorio de dados existe
  if DirExists(DataDir) then
  begin
    Msg1 := 'Deseja excluir tambem seus snippets e configuracoes pessoais?' + NL +
            NL +
            'Localizacao: ' + DataDir + NL +
            NL +
            'Clique em "Nao" para manter seus dados (recomendado).' + NL +
            'Clique em "Sim" para excluir permanentemente todos os snippets e configuracoes.';

    MsgResult := MsgBox(Msg1, mbConfirmation, MB_YESNO or MB_DEFBUTTON2);

    if MsgResult = IDYES then
    begin
      Msg2 := 'CONFIRMACAO FINAL' + NL +
              NL +
              'Seus snippets e configuracoes serao excluidos permanentemente.' + NL +
              'Esta acao nao pode ser desfeita.' + NL +
              NL +
              'Confirma a exclusao?';

      MsgResult := MsgBox(Msg2, mbError, MB_YESNO or MB_DEFBUTTON2);

      if MsgResult = IDYES then
      begin
        // Remover apenas o diretorio de dados do Nimbox
        DelTree(DataDir, True, True, True);
      end;
    end;
  end;
end;

// Verificacao de versao anterior instalada (para atualizacoes)
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  // Encerrar instancia em execucao antes de instalar
  Exec('cmd.exe', '/C taskkill /F /IM Nimbox.exe 2>nul', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;
