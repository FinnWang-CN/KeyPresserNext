#include "FileAssociation.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QString>

#include <windows.h>
#include <shlobj.h>

namespace {

void WriteAssocRoot(QSettings& classes, const QString& exe_native) {
  // HKCU\Software\Classes — 当前用户即可生效，便于开发版与安装版
  classes.beginGroup(QStringLiteral(".kpn"));
  classes.setValue(QStringLiteral("."), QStringLiteral("KeyPresserNext.kpn"));
  classes.endGroup();

  // 清除旧版 .kpns 关联（若有）
  classes.remove(QStringLiteral(".kpns"));

  classes.beginGroup(QStringLiteral("KeyPresserNext.kpn"));
  classes.setValue(QStringLiteral("."), QStringLiteral("KeyPresserNext 配置"));
  classes.beginGroup(QStringLiteral("DefaultIcon"));
  classes.setValue(QStringLiteral("."), exe_native + QStringLiteral(",0"));
  classes.endGroup();
  classes.beginGroup(QStringLiteral("shell"));
  classes.beginGroup(QStringLiteral("open"));
  classes.beginGroup(QStringLiteral("command"));
  classes.setValue(QStringLiteral("."),
                   QStringLiteral("\"%1\" \"%2\"").arg(exe_native, QStringLiteral("%1")));
  classes.endGroup();
  classes.endGroup();
  classes.endGroup();
  classes.endGroup();
}

}  // namespace

void EnsureKpnFileAssociation() {
  const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  if (exe.isEmpty()) {
    return;
  }

  QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"), QSettings::NativeFormat);
  WriteAssocRoot(classes, exe);
  classes.sync();

  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void EnsureCustomUrlProtocol() {
  const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  if (exe.isEmpty()) {
    return;
  }

  QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"), QSettings::NativeFormat);
  classes.beginGroup(QStringLiteral("keypressernext"));
  classes.setValue(QStringLiteral("."), QStringLiteral("URL:KeyPresserNext Protocol"));
  classes.setValue(QStringLiteral("URL Protocol"), QString());
  classes.beginGroup(QStringLiteral("DefaultIcon"));
  classes.setValue(QStringLiteral("."), exe + QStringLiteral(",0"));
  classes.endGroup();
  classes.beginGroup(QStringLiteral("shell"));
  classes.beginGroup(QStringLiteral("open"));
  classes.beginGroup(QStringLiteral("command"));
  // "%1" 为完整 URL，例如 keypressernext://focus
  classes.setValue(QStringLiteral("."),
                   QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1")));
  classes.endGroup();
  classes.endGroup();
  classes.endGroup();
  classes.endGroup();
  classes.sync();

  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
