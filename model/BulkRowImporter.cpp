#include "BulkRowImporter.h"

#include "../common/KeyBinding.h"

#include <QRegularExpression>

namespace BulkRowImporter {
namespace {

bool IsTruthy(const QString& value) {
  const QString v = value.trimmed().toLower();
  return v == QLatin1String("1") || v == QLatin1String("true") || v == QLatin1String("yes") ||
         v == QStringLiteral("是") || v == QStringLiteral("勾选");
}

bool ParseInterval(const QString& a, const QString& b, int* min_ms, int* max_ms) {
  bool ok1 = false;
  bool ok2 = false;
  const int mn = a.trimmed().toInt(&ok1);
  const int mx = b.trimmed().isEmpty() ? mn : b.trimmed().toInt(&ok2);
  if (!ok1) {
    return false;
  }
  *min_ms = mn > 0 ? mn : 1000;
  *max_ms = (ok2 && mx > 0) ? mx : *min_ms;
  return true;
}

bool LooksLikeNumber(const QString& s) {
  bool ok = false;
  s.trimmed().toInt(&ok);
  return ok;
}

bool ParseMouseButton(const QString& text, MouseButton* out) {
  const QString raw = text.trimmed();
  const QString t = raw.toLower();
  if (t.isEmpty()) {
    return false;
  }
  // 按下/弹起须先于「左键」「右键」匹配（避免「左键按下」被当成「左键」）
  if (t == QLatin1String("leftdown") || raw.contains(QStringLiteral("左键按下")) ||
      raw.contains(QStringLiteral("左按下"))) {
    *out = MouseButton::LeftDown;
    return true;
  }
  if (t == QLatin1String("leftup") || raw.contains(QStringLiteral("左键弹起")) ||
      raw.contains(QStringLiteral("左弹起"))) {
    *out = MouseButton::LeftUp;
    return true;
  }
  if (t == QLatin1String("rightdown") || raw.contains(QStringLiteral("右键按下")) ||
      raw.contains(QStringLiteral("右按下"))) {
    *out = MouseButton::RightDown;
    return true;
  }
  if (t == QLatin1String("rightup") || raw.contains(QStringLiteral("右键弹起")) ||
      raw.contains(QStringLiteral("右弹起"))) {
    *out = MouseButton::RightUp;
    return true;
  }
  if (t == QLatin1String("leftdouble") || raw.contains(QStringLiteral("双击")) ||
      raw.contains(QStringLiteral("左双"))) {
    *out = MouseButton::LeftDouble;
    return true;
  }
  if (t == QLatin1String("right") || raw.contains(QStringLiteral("右键"))) {
    *out = MouseButton::Right;
    return true;
  }
  if (t == QLatin1String("middle") || raw.contains(QStringLiteral("中键"))) {
    *out = MouseButton::Middle;
    return true;
  }
  if (t == QLatin1String("wheelup") || raw.contains(QStringLiteral("滚轮上")) ||
      raw.contains(QStringLiteral("上滚"))) {
    *out = MouseButton::WheelUp;
    return true;
  }
  if (t == QLatin1String("wheeldown") || raw.contains(QStringLiteral("滚轮下")) ||
      raw.contains(QStringLiteral("下滚"))) {
    *out = MouseButton::WheelDown;
    return true;
  }
  if (t == QLatin1String("left") || raw.contains(QStringLiteral("左键")) ||
      raw.contains(QStringLiteral("单击"))) {
    *out = MouseButton::Left;
    return true;
  }
  *out = MouseButtonFromString(t);
  return true;
}

QString ResolveKeyShortcut(const QString& shortcut_part, const QString& key_part) {
  if (shortcut_part.trimmed().isEmpty()) {
    const KeyBinding b = KeyBinding::fromDisplayText(key_part.trimmed());
    return b.isValid() ? b.storageText() : key_part.trimmed();
  }
  const KeyBinding b = KeyBinding::fromLegacySelection(shortcut_part.trimmed(), key_part.trimmed());
  return b.isValid() ? b.storageText() : key_part.trimmed();
}

}  // namespace

bool ParseKeyLine(const QString& line, ActionRow* out) {
  if (!out) {
    return false;
  }
  const QString trimmed = line.trimmed();
  if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')) ||
      trimmed.startsWith(QLatin1String("//"))) {
    return false;
  }

  const QStringList parts =
      trimmed.split(QRegularExpression(QStringLiteral("[,\\t]")), Qt::KeepEmptyParts);
  if (parts.isEmpty()) {
    return false;
  }

  ActionRow row;
  row.kind = ActionKind::Key;
  row.target.mode = TargetMode::None;

  if (parts.size() == 1) {
    row.enabled = true;
    row.shortcut = ResolveKeyShortcut(QString(), parts[0]);
    if (row.shortcut.isEmpty()) {
      return false;
    }
    *out = row;
    return true;
  }

  if (parts.size() == 3) {
    row.enabled = IsTruthy(parts[0]);
    row.shortcut = ResolveKeyShortcut(QString(), parts[1]);
    if (row.shortcut.isEmpty() ||
        !ParseInterval(parts[2], parts[2], &row.interval_min_ms, &row.interval_max_ms)) {
      return false;
    }
    *out = row;
    return true;
  }

  if (parts.size() == 4) {
    row.enabled = IsTruthy(parts[0]);
    row.shortcut = ResolveKeyShortcut(QString(), parts[1]);
    if (row.shortcut.isEmpty() ||
        !ParseInterval(parts[2], parts[3], &row.interval_min_ms, &row.interval_max_ms)) {
      return false;
    }
    *out = row;
    return true;
  }

  if (parts.size() == 5) {
    row.enabled = IsTruthy(parts[0]);
    row.shortcut = ResolveKeyShortcut(parts[1], parts[2]);
    if (row.shortcut.isEmpty() ||
        !ParseInterval(parts[3], parts[4], &row.interval_min_ms, &row.interval_max_ms)) {
      return false;
    }
    *out = row;
    return true;
  }

  if (parts.size() >= 6) {
    row.enabled = IsTruthy(parts[0]);
    row.shortcut = ResolveKeyShortcut(parts[1], parts[2]);
    row.hold = IsTruthy(parts[3]);
    if (row.shortcut.isEmpty() ||
        !ParseInterval(parts[4], parts[5], &row.interval_min_ms, &row.interval_max_ms)) {
      return false;
    }
    *out = row;
    return true;
  }

  return false;
}

bool ParseMouseLine(const QString& line, ActionRow* out) {
  if (!out) {
    return false;
  }
  const QString trimmed = line.trimmed();
  if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')) ||
      trimmed.startsWith(QLatin1String("//"))) {
    return false;
  }

  const QStringList parts =
      trimmed.split(QRegularExpression(QStringLiteral("[,\\t]")), Qt::KeepEmptyParts);
  if (parts.isEmpty()) {
    return false;
  }

  ActionRow row;
  row.kind = ActionKind::Mouse;
  row.target.mode = TargetMode::Coordinate;
  row.target.x = 0;
  row.target.y = 0;

  if (parts.size() == 1) {
    row.enabled = true;
    if (!ParseMouseButton(parts[0], &row.button)) {
      return false;
    }
    *out = row;
    return true;
  }

  if (parts.size() == 3) {
    row.enabled = IsTruthy(parts[0]);
    if (!ParseMouseButton(parts[1], &row.button)) {
      return false;
    }
    if (!ParseInterval(parts[2], parts[2], &row.interval_min_ms, &row.interval_max_ms)) {
      return false;
    }
    *out = row;
    return true;
  }

  if (parts.size() == 4) {
    row.enabled = IsTruthy(parts[0]);
    if (!ParseMouseButton(parts[1], &row.button)) {
      return false;
    }
    if (!ParseInterval(parts[2], parts[3], &row.interval_min_ms, &row.interval_max_ms)) {
      return false;
    }
    *out = row;
    return true;
  }

  if (parts.size() == 5) {
    row.enabled = IsTruthy(parts[0]);
    if (!ParseMouseButton(parts[1], &row.button)) {
      return false;
    }
    const QString path = parts[2].trimmed();
    if (!path.isEmpty()) {
      row.target.mode = TargetMode::FindPic;
      row.target.image_path = path;
    }
    if (!ParseInterval(parts[3], parts[4], &row.interval_min_ms, &row.interval_max_ms)) {
      return false;
    }
    *out = row;
    return true;
  }

  if (parts.size() >= 6) {
    row.enabled = IsTruthy(parts[0]);
    if (!ParseMouseButton(parts[1], &row.button)) {
      return false;
    }
    // 1,左键,100,200,1000,1000  或  1,左键,图片路径,,1000,1000
    if (LooksLikeNumber(parts[2]) && LooksLikeNumber(parts[3])) {
      row.target.mode = TargetMode::Coordinate;
      row.target.x = parts[2].trimmed().toInt();
      row.target.y = parts[3].trimmed().toInt();
      if (!ParseInterval(parts[4], parts[5], &row.interval_min_ms, &row.interval_max_ms)) {
        return false;
      }
    } else {
      const QString path = parts[2].trimmed();
      if (!path.isEmpty()) {
        row.target.mode = TargetMode::FindPic;
        row.target.image_path = path;
      }
      if (!ParseInterval(parts[4], parts[5], &row.interval_min_ms, &row.interval_max_ms)) {
        return false;
      }
    }
    *out = row;
    return true;
  }

  return false;
}

QVector<ActionRow> ParseText(const QString& text, ActionKind kind) {
  const QStringList lines =
      text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
  QVector<ActionRow> rows;
  rows.reserve(lines.size());
  for (const QString& line : lines) {
    ActionRow row;
    const bool ok =
        kind == ActionKind::Key ? ParseKeyLine(line, &row) : ParseMouseLine(line, &row);
    if (ok) {
      rows.append(row);
    }
  }
  return rows;
}

}  // namespace BulkRowImporter
