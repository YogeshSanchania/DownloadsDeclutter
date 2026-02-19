; Inno Setup Script for Downloads Declutter
; -----------------------------------------

[Setup]
AppId={{9A1B2C3D-4E5F-6A7B-8C9D-0E1F2A3B4C5D}
AppName=Downloads Declutter
AppVersion=1.0
AppPublisher=Declutter Tools
DefaultDirName={autopf}\DownloadsDeclutter
DefaultGroupName=Downloads Declutter
OutputDir=.
OutputBaseFilename=DownloadsDeclutter_Installer
Compression=lzma
SolidCompression=yes
; Require Windows 8 or higher (approximate, usually fine on 7 too)
MinVersion=6.2
PrivilegesRequired=admin

;-- UI Improvements ---
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
[Files]
Source: "x64\Release\DownloadsDeclutter.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Downloads Declutter"; Filename: "{app}\DownloadsDeclutter.exe"
Name: "{group}\Uninstall"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Downloads Declutter"; Filename: "{app}\DownloadsDeclutter.exe"; Tasks: desktopicon

[Registry]
; Context Menu for Folders (Right click on a folder)
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DownloadsDeclutter"; ValueType: string; ValueName: ""; ValueData: "Scan for Declutter"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DownloadsDeclutter"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\DownloadsDeclutter.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DownloadsDeclutter\command"; ValueType: string; ValueName: ""; ValueData: """{app}\DownloadsDeclutter.exe"" ""%1"""; Flags: uninsdeletekey

; Context Menu for Background (Right click inside an empty space in a folder)
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DownloadsDeclutter"; ValueType: string; ValueName: ""; ValueData: "Scan for Declutter"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DownloadsDeclutter"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\DownloadsDeclutter.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DownloadsDeclutter\command"; ValueType: string; ValueName: ""; ValueData: """{app}\DownloadsDeclutter.exe"" ""%V"""; Flags: uninsdeletekey

[Run]
Filename: "{app}\DownloadsDeclutter.exe"; Description: "Launch Downloads Declutter"; Flags: nowait postinstall skipifsilent
; --- FIX FOR MINIMIZED / HIDDEN INSTALLER ---
[Code]
// Import the native Windows API function to force a window to the foreground
function SetForegroundWindow(hWnd: HWND): BOOL; external 'SetForegroundWindow@user32.dll stdcall';

procedure InitializeWizard();
begin
  // Force the installer wizard to the center and front of the screen immediately
  WizardForm.Position := poScreenCenter;
  SetForegroundWindow(WizardForm.Handle);
end;
