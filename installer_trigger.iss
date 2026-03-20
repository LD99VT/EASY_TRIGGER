; Easy Trigger Installer – Inno Setup 6
; Filename: EasyTrigger_Setup_{#AppVersion}.exe

#define AppName "Easy Trigger"
#include "version.iss"
#define AppExe "Easy Trigger.exe"
#define AppPublisher "LUA"
#define BuildDir "build\windows-msvc\EasyTrigger_artefacts\Release"
#ifndef VcRuntimeDir
#define VcRuntimeDir "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT"
#endif
#ifndef VcRedistExe
#define VcRedistExe "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\v143\vc_redist.x64.exe"
#endif

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppId={{A7E3B2C1-5F4D-4B8A-8E0F-C1D2E3F4A5B6}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=EasyTrigger_Setup_{#AppVersion}_release270
OutputDir=Installer\repack
SetupIconFile=Source\Icon.ico
UninstallDisplayIcon={app}\{#AppExe}
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

; App-local MSVC runtime so the app starts on clean machines without VC++ redist installed
Source: "{#VcRuntimeDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion

; Official Microsoft Visual C++ Redistributable bundled for system-wide install on clean machines
Source: "{#VcRedistExe}"; DestDir: "{tmp}"; Flags: deleteafterinstall ignoreversion

; Fonts folder
Source: "Fonts\*"; DestDir: "{app}\Fonts"; Flags: ignoreversion recursesubdirs createallsubdirs

; Help folder
Source: "Help\*"; DestDir: "{app}\Help"; Flags: ignoreversion recursesubdirs createallsubdirs

; Icon folder (findUiBaseDirFromExe checks for "Icon" or "Icons" sibling to exe)
Source: "Icon\*"; DestDir: "{app}\Icon"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Microsoft Visual C++ Runtime..."; Flags: waituntilterminated skipifdoesntexist
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""Easy Trigger"" dir=in action=allow enable=yes program=""{app}\{#AppExe}"" profile=any"; StatusMsg: "Adding Windows Firewall rule..."; Flags: runhidden waituntilterminated
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""Easy Trigger Out"" dir=out action=allow enable=yes program=""{app}\{#AppExe}"" profile=any"; StatusMsg: "Adding Windows Firewall rule..."; Flags: runhidden waituntilterminated
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""Easy Trigger"""; Flags: runhidden waituntilterminated
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""Easy Trigger Out"""; Flags: runhidden waituntilterminated

[Code]
// Kill the running application before install
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Exec('taskkill.exe', '/F /IM "Easy Trigger.exe"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := '';
end;
