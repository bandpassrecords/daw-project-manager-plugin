; Inno Setup script for the DAW Project Manager plug-in (VST3).
;
; Version is injected by CI via the APP_VERSION environment variable, as the
; app's installer.iss does; the fallback keeps local builds working.
;   iscc packaging\windows\installer.iss
; Expects the Release build at build\DPMCompanion_artefacts\Release\VST3.
;
; The folder page picks where the plug-in goes. It starts on the standard
; VST3 folder, the one every DAW scans: C:\Program Files\Common Files\VST3
; for all users, or %LOCALAPPDATA%\Programs\Common\VST3 for "me only" (no
; admin rights needed; the VST3 spec's per-user folder). An update starts on
; the folder the previous install used.

#define APP_VERSION GetEnv('APP_VERSION')
#if APP_VERSION == ""
  #define APP_VERSION "0.0.0"
#endif

#define VST3_SOURCE "..\..\build\DPMCompanion_artefacts\Release\VST3\DAW Project Manager.vst3"
#define BUNDLE "DAW Project Manager.vst3"
; The bundle's name before the plug-in was renamed: an update removes it,
; or the DAW would list the plug-in twice.
#define OLD_BUNDLE "DPM Companion.vst3"

[Setup]
; Never change AppId: it is how an update finds the version it replaces.
AppId={{6E2C9B0A-4C1E-4D8B-9B2F-3A7E5C1D8F42}
AppName=DAW Project Manager Plugin
AppVersion={#APP_VERSION}
AppVerName=DAW Project Manager Plugin {#APP_VERSION}
AppPublisher=BandPass Records
AppPublisherURL=https://dpm.bandpassrecords.com
AppSupportURL=https://github.com/bandpassrecords/daw-project-manager/issues
AppUpdatesURL=https://github.com/bandpassrecords/daw-project-manager-plugin/releases
; {app} is the VST3 folder the bundle goes into.
DefaultDirName={code:DefaultVst3Dir}
AppendDefaultDirName=no
DirExistsWarning=no
DisableDirPage=no
DisableProgramGroupPage=yes
; The uninstaller stays out of the VST3 folder hosts scan. The folder keeps
; the plug-in's old name: an update finds the uninstaller it replaces there.
UninstallFilesDir={autopf}\DPM Companion
UninstallDisplayName=DAW Project Manager Plugin (VST3)
; The app's own icon (app_icon.ico in daw-project-manager).
SetupIconFile=icon.ico
UninstallDisplayIcon={uninstallexe}
; All users by default; "Install for me only" needs no admin rights.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\Output
OutputBaseFilename=DAW_Project_Manager_Plugin_Installer_v{#APP_VERSION}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\..\LICENSE
ShowLanguageDialog=auto

; The app's nine languages. Simplified Chinese is one of Inno Setup's
; unofficial translations, vendored from jrsoftware/issrc at is-6_7_3.
[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "zh"; MessagesFile: "languages\ChineseSimplified.isl"

[Messages]
en.SelectDirLabel3=Setup will install the DAW Project Manager VST3 plug-in into the following folder. Every DAW scans the standard VST3 folder; if you choose another one, add it to your DAW's plug-in folders.
de.SelectDirLabel3=Das Setup installiert das VST3-Plugin DAW Project Manager in den folgenden Ordner. Jede DAW durchsucht den Standard-VST3-Ordner; wenn Sie einen anderen wählen, fügen Sie ihn den Plugin-Ordnern Ihrer DAW hinzu.
es.SelectDirLabel3=El instalador instalará el plugin VST3 DAW Project Manager en la siguiente carpeta. Todas las DAW analizan la carpeta VST3 estándar; si eliges otra, añádela a las carpetas de plugins de tu DAW.
fr.SelectDirLabel3=L'installation placera le plugin VST3 DAW Project Manager dans le dossier suivant. Toutes les DAW analysent le dossier VST3 standard ; si vous en choisissez un autre, ajoutez-le aux dossiers de plugins de votre DAW.
it.SelectDirLabel3=L'installazione copierà il plugin VST3 DAW Project Manager nella cartella seguente. Tutte le DAW analizzano la cartella VST3 standard; se ne scegli un'altra, aggiungila alle cartelle dei plugin della tua DAW.
ja.SelectDirLabel3=DAW Project Manager の VST3 プラグインを次のフォルダーにインストールします。標準の VST3 フォルダーはすべての DAW が読み込みます。別のフォルダーを選ぶ場合は、DAW のプラグインフォルダーに追加してください。
pt.SelectDirLabel3=O instalador vai instalar o plugin VST3 DAW Project Manager na pasta a seguir. Toda DAW procura plugins na pasta VST3 padrão; se você escolher outra, adicione-a às pastas de plugins da sua DAW.
ru.SelectDirLabel3=Программа установит плагин VST3 DAW Project Manager в следующую папку. Все DAW сканируют стандартную папку VST3; если вы выберете другую, добавьте её в папки плагинов вашей DAW.
zh.SelectDirLabel3=安装程序将把 DAW Project Manager VST3 插件安装到以下文件夹。所有 DAW 都会扫描标准 VST3 文件夹；如果你选择其他文件夹，请将它添加到 DAW 的插件文件夹中。

[Files]
Source: "{#VST3_SOURCE}\*"; DestDir: "{app}\{#BUNDLE}"; Flags: recursesubdirs createallsubdirs ignoreversion

[InstallDelete]
Type: filesandordirs; Name: "{app}\{#OLD_BUNDLE}"

[UninstallDelete]
Type: filesandordirs; Name: "{app}\{#BUNDLE}"
Type: filesandordirs; Name: "{app}\{#OLD_BUNDLE}"

[Code]
function DefaultVst3Dir(Param: String): String;
begin
  if IsAdminInstallMode then
    Result := ExpandConstant('{commoncf64}\VST3')
  else
    Result := ExpandConstant('{localappdata}\Programs\Common\VST3');
end;

procedure InitializeWizard;
begin
  // Builds before the folder page kept the uninstaller's folder as {app};
  // an update from one would otherwise start on that folder instead of the
  // VST3 folder the plug-in is actually in.
  if SameText(WizardForm.DirEdit.Text, ExpandConstant('{autopf}\DPM Companion')) then
    WizardForm.DirEdit.Text := DefaultVst3Dir('');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  OldBundle: String;
begin
  // Moved to another folder on update: remove the old copy, under either
  // name, or the DAW would list the plug-in twice.
  if (CurStep = ssInstall) and (WizardForm.PrevAppDir <> '')
     and not SameText(WizardForm.PrevAppDir, WizardDirValue) then
  begin
    OldBundle := AddBackslash(WizardForm.PrevAppDir) + '{#BUNDLE}';
    if DirExists(OldBundle) then
      DelTree(OldBundle, True, True, True);
    OldBundle := AddBackslash(WizardForm.PrevAppDir) + '{#OLD_BUNDLE}';
    if DirExists(OldBundle) then
      DelTree(OldBundle, True, True, True);
  end;
end;
