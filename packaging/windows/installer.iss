; Inno Setup script for DPM Companion (VST3).
;
; Version is injected by CI via the APP_VERSION environment variable, as the
; app's installer.iss does; the fallback keeps local builds working.
;   iscc packaging\windows\installer.iss
; Expects the Release build at build\DPMCompanion_artefacts\Release\VST3.

#define APP_VERSION GetEnv('APP_VERSION')
#if APP_VERSION == ""
  #define APP_VERSION "0.0.0"
#endif

#define VST3_SOURCE "..\..\build\DPMCompanion_artefacts\Release\VST3\DPM Companion.vst3"

[Setup]
; Never change AppId: it is how an update finds the version it replaces.
AppId={{6E2C9B0A-4C1E-4D8B-9B2F-3A7E5C1D8F42}
AppName=DPM Companion
AppVersion={#APP_VERSION}
AppVerName=DPM Companion {#APP_VERSION}
AppPublisher=BandPass Records
AppPublisherURL=https://dpm.bandpassrecords.com
AppSupportURL=https://github.com/bandpassrecords/daw-project-manager/issues
AppUpdatesURL=https://github.com/bandpassrecords/dpm-companion/releases
; The plug-in goes to the one standard VST3 folder (see [Files]); this
; directory only holds the uninstaller, kept out of the bundle hosts scan.
DefaultDirName={autopf}\DPM Companion
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=DPM Companion (VST3)
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\Output
OutputBaseFilename=DPM_Companion_Installer_v{#APP_VERSION}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\..\LICENSE

[Files]
Source: "{#VST3_SOURCE}\*"; DestDir: "{commoncf64}\VST3\DPM Companion.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\DPM Companion.vst3"
