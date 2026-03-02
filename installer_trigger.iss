; Easy Trigger Installer – Inno Setup 6
; Filename: EasyTrigger_Setup_{#AppVersion}.exe

#define AppName "Easy Trigger"
#include "version.iss"
#define AppExe "Easy Trigger.exe"
#define AppPublisher "LUA"
#define BuildDir "build\windows-msvc\EasyTrigger_artefacts\Release"

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppId={{A7E3B2C1-5F4D-4B8A-8E0F-C1D2E3F4A5B6}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=EasyTrigger_Setup_{#AppVersion}
OutputDir=Installer
SetupIconFile=Icon\Icon Trigger.ico
UninstallDisplayIcon={app}\Icon\Icon Trigger.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableWelcomePage=no
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
; English only
ShowLanguageDialog=no
LanguageDetectionMethod=none

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
; Main executable
Source: "{#BuildDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion

; Fonts folder
Source: "Fonts\*"; DestDir: "{app}\Fonts"; Flags: ignoreversion recursesubdirs createallsubdirs

; Help folder
Source: "Help\*"; DestDir: "{app}\Help"; Flags: ignoreversion recursesubdirs createallsubdirs

; Icon folder (findUiBaseDirFromExe checks for "Icon" or "Icons" sibling to exe)
Source: "Icon\*"; DestDir: "{app}\Icon"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\Icon\Icon Trigger.ico"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\Icon\Icon Trigger.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
// Kill the running application before install
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Exec('taskkill.exe', '/F /IM "Easy Trigger.exe"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := '';
end;
