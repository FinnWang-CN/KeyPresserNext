; KeyPresserNext — Inno Setup 6/7
; 由 scripts\release.bat 调用；也可手动：ISCC.exe /DMyAppVersion=1.0.0 KeyPresserNext.iss
;
; 预定义（可由命令行 /D 覆盖）：
;   MyAppVersion  — 版本号
;   PackageDir    — 已 windeploy 的程序目录（相对本 iss 或绝对路径）

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#ifndef PackageDir
  #define PackageDir "..\dist\package"
#endif

#define MyAppName "KeyPresserNext Community"
#define MyAppPublisher "FinnSoft"
#define MyAppURL "https://keypresser.onepeak.cn/keypresser-next.html"
#define MyAppExeName "KeyPresserNextCommunity.exe"
; Community uninstall GUID (separate from the full product)
#define MyAppIdGuid "B8F4D3E2-0C5E-4F9B-AD2A-3E7C9F1B6A44"

[Setup]
AppId={{B8F4D3E2-0C5E-4F9B-AD2A-3E7C9F1B6A44}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\KeyPresserNextCommunity
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=
OutputDir=..\dist\installer
OutputBaseFilename=KeyPresserNextCommunity-{#MyAppVersion}-setup
SetupIconFile=..\resources\keypresser.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Setup
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

; 双击 .kpn 用本程序打开（系统级；卸载时删除）；顺带清掉旧 .kpns 关联
[Registry]
Root: HKCR; Subkey: ".kpns"; Flags: deletekey
Root: HKCR; Subkey: ".kpn"; ValueType: string; ValueName: ""; ValueData: "KeyPresserNextCommunity.kpn"; Flags: uninsdeletevalue
Root: HKCR; Subkey: "KeyPresserNextCommunity.kpn"; ValueType: string; ValueName: ""; ValueData: "KeyPresserNext Community 配置"; Flags: uninsdeletekey
Root: HKCR; Subkey: "KeyPresserNextCommunity.kpn\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKCR; Subkey: "KeyPresserNextCommunity.kpn\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

[Run]
; 程序 requireAdministrator；postinstall 默认降权会报 740，需与安装程序同级启动
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent runascurrentuser

[Code]
var
  GDeleteUserConfig: Boolean;

function GetUninstallString: String;
var
  UninstKey: String;
  Uninstall: String;
  UninsExe: String;
begin
  Result := '';
  Uninstall := '';
  // 用 Chr 拼花括号，避免 Pascal 把 {GUID} 当注释/常量
  UninstKey := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\' +
    Chr(123) + '{#MyAppIdGuid}' + Chr(125) + '_is1';

  if IsWin64 then
  begin
    if RegQueryStringValue(HKLM64, UninstKey, 'UninstallString', Uninstall) then
    begin
      Result := Uninstall;
      Exit;
    end;
    if RegQueryStringValue(HKCU64, UninstKey, 'UninstallString', Uninstall) then
    begin
      Result := Uninstall;
      Exit;
    end;
  end;
  if RegQueryStringValue(HKLM, UninstKey, 'UninstallString', Uninstall) then
  begin
    Result := Uninstall;
    Exit;
  end;
  if RegQueryStringValue(HKCU, UninstKey, 'UninstallString', Uninstall) then
  begin
    Result := Uninstall;
    Exit;
  end;

  // 兜底：安装目录仍有卸载程序
  UninsExe := ExpandConstant('{autopf}\{#MyAppName}\unins000.exe');
  if FileExists(UninsExe) then
    Result := '"' + UninsExe + '"';
end;

function IsAppInstalled: Boolean;
begin
  Result := (GetUninstallString() <> '') or
    FileExists(ExpandConstant('{autopf}\{#MyAppName}\{#MyAppExeName}'));
end;

function UninstallExisting: Boolean;
var
  Uninstall: String;
  ResultCode: Integer;
  Params: String;
begin
  Result := False;
  Uninstall := GetUninstallString;
  if Uninstall = '' then
  begin
    // 仅有程序文件、无卸载项：视为可覆盖安装
    Result := True;
    Exit;
  end;

  Uninstall := RemoveQuotes(Uninstall);
  // 升级前静默卸载：不带 DELETECONFIG，保留用户配置
  Params := '/VERYSILENT /NORESTART /SUPPRESSMSGBOXES _?="' + ExtractFilePath(Uninstall) + '"';
  if Exec(Uninstall, Params, '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
    Result := (ResultCode = 0)
  else
    MsgBox('无法启动卸载程序，请先在「应用和功能」中手动卸载后再安装。', mbError, MB_OK);
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  if not IsAppInstalled then
    Exit;

  case MsgBox(
    '检测到本机已安装 {#MyAppName}。' + #13#10 + #13#10 +
    '是否先卸载旧版本，再继续安装？' + #13#10 +
    '选择「否」将取消本次安装。',
    mbConfirmation, MB_YESNO) of
    IDYES:
      begin
        if GetUninstallString() = '' then
          Exit; // 无卸载项则直接覆盖安装
        if not UninstallExisting then
        begin
          MsgBox('卸载未完成，已取消安装。', mbError, MB_OK);
          Result := False;
        end;
      end;
    else
      Result := False;
  end;
end;

procedure DeleteUserConfigData;
begin
  // Qt AppDataLocation：会话、截图缓存等
  DelTree(ExpandConstant('{userappdata}\FinnSoft\KeyPresserNext'), True, True, True);
  DelTree(ExpandConstant('{localappdata}\FinnSoft\KeyPresserNext'), True, True, True);
  // QSettings（授权等）
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\FinnSoft\KeyPresserNext');
  RegDeleteKeyIfEmpty(HKCU, 'Software\FinnSoft');
end;

function AskUninstallOptions: Boolean;
var
  Form: TSetupForm;
  InfoLabel: TNewStaticText;
  CheckBox: TNewCheckBox;
  OKButton, CancelButton: TNewButton;
  ButtonTop: Integer;
begin
  GDeleteUserConfig := False;
  Result := False;

  Form := CreateCustomForm(ScaleX(420), ScaleY(168), False, False);
  try
    Form.Caption := '{#MyAppName} 卸载';

    InfoLabel := TNewStaticText.Create(Form);
    InfoLabel.Parent := Form;
    InfoLabel.Left := ScaleX(16);
    InfoLabel.Top := ScaleY(16);
    InfoLabel.Width := Form.ClientWidth - ScaleX(32);
    InfoLabel.Height := ScaleY(40);
    InfoLabel.AutoSize := False;
    InfoLabel.WordWrap := True;
    InfoLabel.Caption := '确定要卸载 {#MyAppName} 吗？';

    CheckBox := TNewCheckBox.Create(Form);
    CheckBox.Parent := Form;
    CheckBox.Left := ScaleX(16);
    CheckBox.Top := ScaleY(64);
    CheckBox.Width := Form.ClientWidth - ScaleX(32);
    CheckBox.Caption := '同时删除用户配置（窗口设置、最近文件、授权缓存等）';
    CheckBox.Checked := False;

    ButtonTop := Form.ClientHeight - ScaleY(44);
    OKButton := TNewButton.Create(Form);
    OKButton.Parent := Form;
    OKButton.Width := ScaleX(100);
    OKButton.Height := ScaleY(28);
    OKButton.Left := Form.ClientWidth - ScaleX(220);
    OKButton.Top := ButtonTop;
    OKButton.Caption := '卸载';
    OKButton.ModalResult := mrOk;
    OKButton.Default := True;

    CancelButton := TNewButton.Create(Form);
    CancelButton.Parent := Form;
    CancelButton.Width := ScaleX(100);
    CancelButton.Height := ScaleY(28);
    CancelButton.Left := Form.ClientWidth - ScaleX(110);
    CancelButton.Top := ButtonTop;
    CancelButton.Caption := '取消';
    CancelButton.ModalResult := mrCancel;
    CancelButton.Cancel := True;

    if Form.ShowModal = mrOk then
    begin
      GDeleteUserConfig := CheckBox.Checked;
      Result := True;
    end;
  finally
    Form.Free;
  end;
end;

function InitializeUninstall: Boolean;
var
  ResultCode: Integer;
  Params: String;
  UninsPath: String;
begin
  // 静默：由 /DELETECONFIG=1 控制是否删配置；升级重装默认不带此参数
  if UninstallSilent then
  begin
    GDeleteUserConfig := (ExpandConstant('{param:DELETECONFIG|0}') = '1');
    Result := True;
    Exit;
  end;

  if not AskUninstallOptions then
  begin
    Result := False;
    Exit;
  end;

  // 用静默模式真正执行，避免再弹一次系统确认框
  UninsPath := ExpandConstant('{uninstallexe}');
  Params := '/VERYSILENT /NORESTART /SUPPRESSMSGBOXES';
  if GDeleteUserConfig then
    Params := Params + ' /DELETECONFIG=1';
  Params := Params + ' _?="' + ExtractFilePath(UninsPath) + '"';
  Exec(UninsPath, Params, '', SW_SHOW, ewWaitUntilTerminated, ResultCode);
  Result := False;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and GDeleteUserConfig then
    DeleteUserConfigData;
end;
