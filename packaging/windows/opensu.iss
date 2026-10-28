; The per-user openSU installer, packed by tools/package/windows: no administrator, installed under
; %LOCALAPPDATA%\Programs\openSU with a Start menu entry. ISCC gets AppVersion, StageDir (the
; `cmake --install` prefix: bin, lib, share), OutputDir, OutputName and LicenseFile with /D.

[Setup]
AppId={{C6B13793-159A-4E33-ACE5-8DE57C7A7351}
AppName=openSU
AppVersion={#AppVersion}
AppVerName=openSU {#AppVersion}
AppPublisher=openSU
AppPublisherURL=https://github.com/SomeoneIsWorking/opensu
AppSupportURL=https://github.com/SomeoneIsWorking/opensu/issues
DefaultDirName={autopf}\openSU
DefaultGroupName=openSU
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#LicenseFile}
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\opensu.exe
UninstallDisplayName=openSU

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\openSU"; Filename: "{app}\bin\opensu.exe"; WorkingDir: "{app}\bin"
Name: "{group}\Uninstall openSU"; Filename: "{uninstallexe}"
Name: "{autodesktop}\openSU"; Filename: "{app}\bin\opensu.exe"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\opensu.exe"; Description: "{cm:LaunchProgram,openSU}"; Flags: nowait postinstall skipifsilent
