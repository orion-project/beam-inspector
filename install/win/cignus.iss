#define AppName      "Cignus"
#define AppExe       "cignus.exe"
#define AppPublisher "orion-project.org"
#define AppURL       "https://github.com/orion-project/cignus"
#define BaseDir      "..\..\out\redist"

#define FileHandle = FileOpen("..\..\release\version.txt")
#if FileHandle
  #define AppVerFull = FileRead(FileHandle)
  #expr FileClose(FileHandle)
#else
  #error "Unable to open version file"
#endif

[Setup]
AppId={{D800B8D4-FB2E-491A-898D-A001FC9127D0}
AppName={#AppName}
AppVerName={#AppName} {#AppVerFull}
AppVersion={#AppVerFull}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
SetupMutex={#AppName}_SetupMutex
CloseApplications=yes
DefaultDirName={autopf64}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
LicenseFile={#BaseDir}\..\..\LICENSE
;Output=no
OutputDir={#BaseDir}\..\install
OutputBaseFilename="cignus-setup-{#AppVerFull}"
;Compression=lzma
Compression=zip
SolidCompression=yes
PrivilegesRequired=lowest
ChangesAssociations=yes
VersionInfoVersion={#AppVerFull}.0
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

; Windows 10 (1809) is the minimum supported by Qt6
; https://doc.qt.io/qt-6/supported-platforms.html
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
Source: "{#BaseDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

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