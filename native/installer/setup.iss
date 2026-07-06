; M-Finlogs Native Installer Script (Inno Setup 6)
; Creates a single .exe installer that bundles the app + Qt DLLs + fonts + ODBC driver

#define MyAppName "M-Finlogs"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "M-Finlogs"
#define MyAppExeName "mfinlogs-native.exe"
#define MyAppURL "https://github.com/ekthar/M-Finlogs"
#define OdbcDriverMsi "msodbcsql_17.10.5.1_x64.msi"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=Output
OutputBaseFilename=MFinlogs-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
SetupIconFile=..\..\assets\finlogs.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startmenu"; Description: "Create Start Menu shortcut"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; Main executable and all Qt DLLs/plugins from the windeployqt package folder
Source: "..\build\package\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; Microsoft ODBC Driver 17 for SQL Server redistributable (bundled only if MSI is present)
#ifexist "msodbcsql_17.10.5.1_x64.msi"
Source: "{#OdbcDriverMsi}"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: not OdbcDriverInstalled
#endif

[Run]
; Install ODBC driver silently (only if not already present and MSI is available)
#ifexist "msodbcsql_17.10.5.1_x64.msi"
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\{#OdbcDriverMsi}"" /quiet IAcceptSQLServerODBCLicenseTerms=Yes /norestart"; StatusMsg: "Installing Microsoft ODBC Driver 17 for SQL Server..."; Flags: runhidden; Check: not OdbcDriverInstalled
#endif
; Launch the app after install
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Code]
function OdbcDriverInstalled: Boolean;
var
  Version: String;
begin
  Result := RegQueryStringValue(HKLM,
    'SOFTWARE\ODBC\ODBCINST.INI\ODBC Driver 17 for SQL Server',
    'Driver', Version);
  if not Result then
    Result := RegQueryStringValue(HKLM64,
      'SOFTWARE\ODBC\ODBCINST.INI\ODBC Driver 17 for SQL Server',
      'Driver', Version);
end;
