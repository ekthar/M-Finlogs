; Inno Setup Script for M-Finlogs Desktop
; Requires Inno Setup 6.x or later

#define MyAppName "M-Finlogs"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "M-Finlogs"
#define MyAppURL "https://github.com/ekthar/M-Finlogs"
#define MyAppExeName "MFinlogs.Desktop.exe"

[Setup]
AppId={{B7F2A3C4-D5E6-4F78-9ABC-DEF012345678}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\output
OutputBaseFilename=MFinlogs-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\MFinlogs.Desktop\Assets\finlogs.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
LicenseFile=
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startupicon"; Description: "Start with Windows"; GroupDescription: "Additional options:"; Flags: unchecked

[Files]
; Main application files (publish output)
Source: "..\publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; WebView2 bootstrapper (installs runtime if not present)
Source: "MicrosoftEdgeWebview2Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Auto-start on Windows login (optional)
Root: HKCU; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "MFinlogs"; ValueData: """{app}\{#MyAppExeName}"" --minimized"; Flags: uninsdeletevalue; Tasks: startupicon

; File association for .mfinlogs backup files
Root: HKCU; Subkey: "SOFTWARE\Classes\.mfinlogs"; ValueType: string; ValueData: "MFinlogs.Backup"; Flags: uninsdeletekey
Root: HKCU; Subkey: "SOFTWARE\Classes\MFinlogs.Backup"; ValueType: string; ValueData: "M-Finlogs Backup"; Flags: uninsdeletekey
Root: HKCU; Subkey: "SOFTWARE\Classes\MFinlogs.Backup\DefaultIcon"; ValueType: string; ValueData: "{app}\{#MyAppExeName},0"; Flags: uninsdeletekey
Root: HKCU; Subkey: "SOFTWARE\Classes\MFinlogs.Backup\shell\open\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" --restore ""%1"""; Flags: uninsdeletekey

[Run]
; Install WebView2 Runtime if not already present
Filename: "{tmp}\MicrosoftEdgeWebview2Setup.exe"; Parameters: "/silent /install"; StatusMsg: "Installing WebView2 Runtime..."; Check: NeedsWebView2Runtime; Flags: waituntilterminated

; Launch after install
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\MFinlogs"

[Code]
function NeedsWebView2Runtime: Boolean;
var
  ResultCode: Integer;
begin
  // Check if WebView2 runtime is installed
  Result := not RegKeyExists(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}')
        and not RegKeyExists(HKCU, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}')
        and not RegKeyExists(HKLM, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}');
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  // Check Windows version (requires Windows 10 1809+)
  if not IsWin64 then
  begin
    MsgBox('M-Finlogs requires a 64-bit version of Windows 10 or later.', mbError, MB_OK);
    Result := False;
  end;
end;
