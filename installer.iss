[Setup]
AppName=SampleGrab
AppVersion=1.0.0
AppPublisher=jerryrpt
DefaultDirName={commonappdata}\jerryrpt\SampleGrab
DefaultGroupName=SampleGrab
OutputBaseFilename=SampleGrab_Windows_Installer
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
DisableDirPage=yes
Uninstallable=yes
PrivilegesRequired=admin

[Files]
; Copy the entire VST3 package to the system Common Files directory
Source: "build\SampleGrab_artefacts\Release\VST3\SampleGrab.vst3\*"; DestDir: "{cf64}\VST3\SampleGrab.vst3\"; Flags: ignoreversion recursesubdirs createallsubdirs
; Copy the python backend to the hidden ProgramData folder
Source: "backend.py"; DestDir: "{commonappdata}\jerryrpt\SampleGrab"; Flags: ignoreversion
Source: "requirements.txt"; DestDir: "{commonappdata}\jerryrpt\SampleGrab"; Flags: ignoreversion

[Run]
; Run pip install in the background silently
Filename: "cmd.exe"; Parameters: "/C pip install -r ""{commonappdata}\jerryrpt\SampleGrab\requirements.txt"""; Flags: runhidden waituntilterminated
; Attempt to install ffmpeg quietly if it's missing (though winget might not work ideally in silent installers, it's a good fallback)
Filename: "winget.exe"; Parameters: "install -e --id Gyan.FFmpeg --accept-source-agreements --accept-package-agreements"; Flags: runhidden waituntilterminated

[Dirs]
Name: "{commonappdata}\jerryrpt\SampleGrab"
Name: "{userdocs}\SampleGrab"
