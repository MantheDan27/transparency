; Transparencii Installer
; Supports /SILENT and /VERYSILENT for unattended / enterprise deployment.
;
; Usage:
;   Interactive:            Transparencii-Setup-x.x.x.exe
;   Silent (progress bar):  Transparencii-Setup-x.x.x.exe /SILENT
;   Fully silent:           Transparencii-Setup-x.x.x.exe /VERYSILENT /SUPPRESSMSGBOXES
;
; Defines supplied by the build (iscc /DAppVersion=... /DSourceDir=...):
;   AppVersion  — e.g. 4.35.2
;   SourceDir   — absolute path to the repo root

#define AppName      "Transparencii"
#define AppExe       "Transparencii.exe"
#define Publisher    "MantheDan27"
#define RepoURL      "https://github.com/MantheDan27/transparency"

[Setup]
AppId={{3F8A2C1D-74B6-4E9A-BC02-F1D347E82A56}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#RepoURL}
AppSupportURL={#RepoURL}/issues
AppUpdatesURL={#RepoURL}/releases

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
DisableDirPage=yes
DisableProgramGroupPage=yes
ShowLanguageDialog=no
WizardStyle=modern

; Output
OutputDir=Output
OutputBaseFilename={#AppName}-Setup-{#AppVersion}

; Compression
Compression=lzma2/ultra64
SolidCompression=yes

; Privileges — requires admin to write to Program Files
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\build\Release\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";         Filename: "{app}\{#AppExe}"
Name: "{commondesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; \
  Description: "Launch {#AppName} now"; \
  Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
