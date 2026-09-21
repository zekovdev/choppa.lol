#ifndef SourceDir
  #error SourceDir is required
#endif

#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{B57E3608-7750-45AA-AFF8-91170A483F87}
AppName=choppa.lol
AppVersion=1.0.0
AppPublisher=zekovdev
AppPublisherURL=https://github.com/zekovdev/choppa.lol
AppSupportURL=https://github.com/zekovdev/choppa.lol/issues
DefaultDirName={localappdata}\Programs\choppa.lol
DefaultGroupName=choppa.lol
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=choppa.lol
SetupIconFile=..\resources\icon.ico
UninstallDisplayIcon={app}\choppa.lol.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
CloseApplications=yes
RestartApplications=no

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\choppa.lol"; Filename: "{app}\choppa.lol.exe"
Name: "{autodesktop}\choppa.lol"; Filename: "{app}\choppa.lol.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Run]
Filename: "{app}\choppa.lol.exe"; Description: "Open choppa.lol"; Flags: nowait postinstall skipifsilent
