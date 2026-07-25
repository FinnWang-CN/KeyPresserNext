#include "SettingsStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

QString CopyImageToAssets(const QString& src_path, const QString& assets_dir, int index) {
  if (src_path.isEmpty() || !QFileInfo::exists(src_path)) {
    return QString();
  }
  QDir().mkpath(assets_dir);
  const QString name = QStringLiteral("row_%1.bmp").arg(index, 3, 10, QLatin1Char('0'));
  const QString dest = QDir(assets_dir).filePath(name);
  const QString src_abs = QFileInfo(src_path).absoluteFilePath();
  const QString dest_abs = QFileInfo(dest).absoluteFilePath();
  // 已在目标位置（再次保存同一 .kpn）：勿先删再拷，否则会删掉唯一文件
  if (QString::compare(src_abs, dest_abs, Qt::CaseInsensitive) == 0) {
    return name;
  }
  // 经临时文件写入，避免源与目标同目录时被 remove 误伤
  const QString tmp = dest + QStringLiteral(".tmp");
  QFile::remove(tmp);
  if (!QFile::copy(src_abs, tmp)) {
    return QString();
  }
  QFile::remove(dest);
  if (!QFile::rename(tmp, dest)) {
    if (!QFile::copy(tmp, dest) || !QFileInfo::exists(dest)) {
      QFile::remove(tmp);
      return QString();
    }
    QFile::remove(tmp);
  }
  return name;
}

}  // namespace

QString SettingsStore::ImagesDir() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
         QStringLiteral("/images");
}

QString SettingsStore::EnsureImagesDir() {
  const QString dir = ImagesDir();
  QDir().mkpath(dir);
  return dir;
}

QString SettingsStore::SessionFilePath() {
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/session.json");
}

AppSession SettingsStore::LoadSession() {
  AppSession session;
  QFile file(SessionFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return session;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (!doc.isObject()) {
    return session;
  }
  const QJsonObject obj = doc.object();
  session.last_path = obj.value(QStringLiteral("lastPath")).toString();
  session.last_changelog_version = obj.value(QStringLiteral("lastChangelogVersion")).toString();
  session.layout_idle_ms =
      static_cast<qint64>(obj.value(QStringLiteral("layoutIdleMs")).toDouble(0));
  const QJsonArray recent = obj.value(QStringLiteral("recentPaths")).toArray();
  for (const QJsonValue& v : recent) {
    const QString p = v.toString();
    if (!p.isEmpty()) {
      session.recent_paths.append(p);
    }
  }
  return session;
}

bool SettingsStore::SaveSession(const AppSession& session) {
  QJsonObject obj;
  obj.insert(QStringLiteral("lastPath"), session.last_path);
  obj.insert(QStringLiteral("lastChangelogVersion"), session.last_changelog_version);
  obj.insert(QStringLiteral("layoutIdleMs"), static_cast<double>(session.layout_idle_ms));
  QJsonArray recent;
  for (const QString& p : session.recent_paths) {
    if (!p.isEmpty()) {
      recent.append(p);
    }
  }
  obj.insert(QStringLiteral("recentPaths"), recent);
  QFile file(SessionFilePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
  file.close();
  return true;
}

void SettingsStore::RememberOpenedFile(const QString& kpns_path) {
  if (kpns_path.trimmed().isEmpty()) {
    return;
  }
  const QString abs = QFileInfo(kpns_path).absoluteFilePath();
  AppSession session = LoadSession();
  session.last_path = abs;
  session.recent_paths.removeAll(abs);
  session.recent_paths.prepend(abs);
  while (session.recent_paths.size() > kMaxRecentFiles) {
    session.recent_paths.removeLast();
  }
  SaveSession(session);
}

QString SettingsStore::AssetsDirForKpns(const QString& kpns_path) {
  const QFileInfo fi(kpns_path);
  return fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".assets");
}

bool SettingsStore::SaveToKpns(const QString& kpns_path, AppSettings* settings, QString* error) {
  if (!settings) {
    if (error) *error = QStringLiteral("null settings");
    return false;
  }
  const QString assets = AssetsDirForKpns(kpns_path);
  QDir().mkpath(assets);

  AppSettings packed = *settings;
  for (int i = 0; i < packed.rows.size(); ++i) {
    ActionRow& row = packed.rows[i];
    if (row.target.mode != TargetMode::FindPic) {
      continue;
    }
    QString abs = row.target.image_path;
    if (!QFileInfo(abs).isAbsolute()) {
      continue;
    }
    const QString rel = CopyImageToAssets(abs, assets, i);
    if (rel.isEmpty()) {
      if (error) {
        *error = QFileInfo::exists(abs)
                     ? QStringLiteral("无法复制截图: %1").arg(abs)
                     : QStringLiteral("截图文件不存在（可能已被覆盖删除，请重新截图）:\n%1")
                           .arg(abs);
      }
      return false;
    }
    row.target.image_path = rel;
  }

  const QByteArray json = QJsonDocument(packed.ToJson()).toJson(QJsonDocument::Indented);
  QFile file(kpns_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error) *error = QStringLiteral("无法写入 %1").arg(kpns_path);
    return false;
  }
  file.write(json);
  file.close();
  *settings = packed;
  for (ActionRow& row : settings->rows) {
    if (row.target.mode == TargetMode::FindPic && !row.target.image_path.isEmpty() &&
        !QFileInfo(row.target.image_path).isAbsolute()) {
      row.target.image_path = QDir(assets).filePath(row.target.image_path);
    }
  }
  return true;
}

bool SettingsStore::LoadFromKpns(const QString& kpns_path, AppSettings* settings, QString* error) {
  if (!settings) {
    return false;
  }
  QFile file(kpns_path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error) *error = QStringLiteral("无法打开 %1").arg(kpns_path);
    return false;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (!doc.isObject()) {
    if (error) *error = QStringLiteral("无效的 .kpn JSON");
    return false;
  }
  *settings = AppSettings::FromJson(doc.object());
  const QString assets = AssetsDirForKpns(kpns_path);
  QStringList missing;
  for (ActionRow& row : settings->rows) {
    if (row.target.mode != TargetMode::FindPic || row.target.image_path.isEmpty()) {
      continue;
    }
    if (!QFileInfo(row.target.image_path).isAbsolute()) {
      row.target.image_path = QDir(assets).filePath(row.target.image_path);
    }
    if (!QFileInfo::exists(row.target.image_path)) {
      missing << row.target.image_path;
    }
  }
  if (!missing.isEmpty() && error) {
    *error = QStringLiteral("缺少截图文件（请同时复制 .assets 目录）:\n%1")
                 .arg(missing.join(QLatin1Char('\n')));
  }
  return true;
}
