; -- zg_choir.iss --
; Script to create a Windows installer for ZGChoir using Inno Setup

[Setup]
AppName=ZGChoir
AppVersion={%CHOIR_VERSION|}
AppVerName=ZGChoir {%CHOIR_VERSION|}
AppPublisher=Jeremy Friesner
DefaultDirName={pf}\ZGChoir
DefaultGroupName=ZGChoir
UninstallDisplayIcon={app}\ZGChoir.exe
Compression=lzma2
SolidCompression=yes
OutputBaseFilename=ZGChoir_Installer
OutputDir=InnoSetup_Installer_Output

[Files]
Source: "ZGChoir_Dist\ZGChoir.exe"; DestDir: "{app}"
Source: "ZGChoir_Dist\Readme.html"; DestDir: "{app}"; Flags: isreadme
Source: "ZGChoir_Dist\Qt5Core.dll"; DestDir: "{app}"
Source: "ZGChoir_Dist\Qt5Gui.dll"; DestDir: "{app}"
Source: "ZGChoir_Dist\Qt5Multimedia.dll"; DestDir: "{app}"
Source: "ZGChoir_Dist\Qt5Widgets.dll"; DestDir: "{app}"
Source: "ZGChoir_Dist\vcruntime140.dll"; DestDir: "{app}"
Source: "ZGChoir_Dist\songs\*.*"; DestDir: "{app}\songs"
Source: "ZGChoir_Dist\images\*.*"; DestDir: "{app}\images"

[Icons]
Name: "{group}\ZGChoir"; Filename: "{app}\ZGChoir.exe"
Name: "{group}\README"; Filename: "{app}\Readme.html"
Name: "{group}\Uninstall ZGChoir"; Filename: "{uninstallexe}"
Name: "{commondesktop}\ZGChoir"; Filename: "{app}\ZGChoir.exe"
