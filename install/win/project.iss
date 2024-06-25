#define AppName      "Beam Inspector"
#define AppExe       "beam-inspector.exe"
#define AppPublisher "orion-project.org"
#define AppURL       "https://github.com/orion-project/beam-inspector"
#define BaseDir      "..\..\out\redist"
#define BinDir       "..\..\bin"

#define FileHandle = FileOpen("..\..\release\version.txt")
#if FileHandle
  #define AppVerFull = FileRead(FileHandle)
  #expr FileClose(FileHandle)
#else
  #error "Unable to open version file"
#endif

[Setup]
AppId={{1B0CEA8D-07D7-4E04-B506-82AD3DB85A2D}
AppName={#AppName}
AppVerName={#AppName} {#AppVerFull}
AppVersion={#AppVerFull}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
SetupMutex={#AppExe}_SetupMutex
CloseApplications=yes
DefaultDirName={autopf64}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
LicenseFile={#BaseDir}\..\..\LICENSE
;Output=no
OutputDir={#BaseDir}\..\install
OutputBaseFilename="beam-inspector-{#AppVerFull}"
;Compression=lzma
Compression=zip
SolidCompression=yes
PrivilegesRequired=lowest
ChangesAssociations=yes
VersionInfoVersion={#AppVerFull}.0
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
WizardSmallImageFile=logo.bmp

; Windows 10 (1809) is the minimum supported by Qt6
; https://doc.qt.io/qt-6/supported-platforms.html
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
Source: "{#BaseDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\{cm:ProgramOnTheWeb,{#AppName}}"; Filename: "{#AppURL}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[UninstallDelete]
Type: files;          Name: "{autoappdata}\{#AppPublisher}\{#AppName}.*"
Type: dirifempty;     Name: "{autoappdata}\{#AppPublisher}"
Type: filesandordirs; Name: "{localappdata}\{#AppPublisher}\{#AppName}"
Type: dirifempty;     Name: "{localappdata}\{#AppPublisher}"

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
english.ForAllUser=For all user
english.ForCurrentUser=For current user