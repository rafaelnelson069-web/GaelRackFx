; Optional installer script for Inno Setup 6.
; Build the project and run package.ps1 first, then compile this script.
#define MyAppName "GaelRack Fx"
#define MyAppVersion "1.0.0"
#define MyPublisher "GaelRack"
#define MySourceDir "dist\GaelRackFx-Windows-x64"

[Setup]
AppId={{F13C3B12-5DA6-4C95-8E6A-7C5D84F98C11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
DefaultDirName={autopf}\Common Files\VST3
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=GaelRackFx-Setup-Windows-x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Uninstallable=yes
WizardStyle=modern

[Files]
Source: "{#MySourceDir}\GaelRack Fx.vst3\*"; DestDir: "{app}\GaelRack Fx.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#MySourceDir}\README_FR.md"; DestDir: "{app}\GaelRack Fx - Documentation"; Flags: ignoreversion

[Messages]
WelcomeLabel2=Ce programme installe GaëlRack Fx au format VST3. Fermez votre logiciel audio avant de continuer.
