#pragma once

#include "ActionRow.h"

#include <QString>
#include <QStringList>

struct AppSession {
  QString last_path;
  QStringList recent_paths;  // 最近打开，最新在前
  /** 用户已看过更新日志的版本号；与 AppVersion() 不同则首次启动弹窗 */
  QString last_changelog_version;
  /** 布局空闲结算时间戳（毫秒）；供 LayoutMetrics 使用 */
  qint64 layout_idle_ms = 0;
};

class SettingsStore {
 public:
  static constexpr int kMaxRecentFiles = 10;

  static QString ImagesDir();
  static QString EnsureImagesDir();

  /** 应用会话（最近文件等），存于 AppData 的 session.json，不用注册表 */
  static QString SessionFilePath();
  static AppSession LoadSession();
  static bool SaveSession(const AppSession& session);
  static void RememberOpenedFile(const QString& kpns_path);

  // Writes pretty JSON + packs findpic images into <base>.assets/
  static bool SaveToKpns(const QString& kpns_path, AppSettings* settings, QString* error);
  static bool LoadFromKpns(const QString& kpns_path, AppSettings* settings, QString* error);

  static QString AssetsDirForKpns(const QString& kpns_path);
};
