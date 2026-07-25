#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>
#include <QVector>

enum class ActionKind { Key, Mouse };

enum class TargetMode { None, Coordinate, FindPic };

enum class MouseButton {
  Left,
  Right,
  Middle,
  LeftDouble,
  WheelUp,
  WheelDown,
  LeftDown,
  LeftUp,
  RightDown,
  RightUp
};

// 顺序运行：按键动作类型（独立运行的「长按」仍用 hold）
enum class KeyStroke { Press, Down, Up };

struct ActionTarget {
  TargetMode mode = TargetMode::None;
  int x = 0;
  int y = 0;
  QString image_path;
  double sim = 0.9;

  QJsonObject ToJson() const;
  static ActionTarget FromJson(const QJsonObject& obj);
  bool operator==(const ActionTarget& o) const {
    return mode == o.mode && x == o.x && y == o.y && image_path == o.image_path &&
           qFuzzyCompare(sim + 1.0, o.sim + 1.0);
  }
  bool operator!=(const ActionTarget& o) const { return !(*this == o); }
};

struct ActionRow {
  bool enabled = true;
  ActionKind kind = ActionKind::Key;
  int interval_min_ms = 1000;
  int interval_max_ms = 1000;

  // Key
  QString shortcut;  // KeyBinding storage text
  bool hold = false;  // 独立运行：长按保持
  KeyStroke key_stroke = KeyStroke::Press;  // 顺序运行：按键 / 按下 / 弹起

  // Mouse
  MouseButton button = MouseButton::Left;

  ActionTarget target;

  QJsonObject ToJson() const;
  static ActionRow FromJson(const QJsonObject& obj);
  bool operator==(const ActionRow& o) const {
    return enabled == o.enabled && kind == o.kind && interval_min_ms == o.interval_min_ms &&
           interval_max_ms == o.interval_max_ms && shortcut == o.shortcut && hold == o.hold &&
           key_stroke == o.key_stroke && button == o.button && target == o.target;
  }
  bool operator!=(const ActionRow& o) const { return !(*this == o); }
};

struct WindowSettings {
  QString display = QStringLiteral("normal.auto");
  QString mouse = QStringLiteral("windows");
  QString keypad = QStringLiteral("windows");
  int bind_mode = 0;
  bool topmost = false;
  QString run_mode = QStringLiteral("sequential");  // independent | sequential
  int run_count = 0;              // 0 = 无限
  int run_loop_interval_ms = 0;   // 轮次/每次额外间隔
  int run_speed_percent = 100;    // 100 = 正常速率
  QString hotkey_start = QStringLiteral("F9");
  QString hotkey_stop = QStringLiteral("F9");  // 与 hotkey_start 共用
  QString hotkey_record = QStringLiteral("F8");
  QString hotkey_stop_record = QStringLiteral("F8");  // 与 hotkey_record 共用
  QString hotkey_select_window;
  QString hotkey_highlight_window;
  QString hotkey_add_key;
  QString hotkey_add_mouse;
  QString hotkey_clear;
  QString hotkey_undo = QStringLiteral("Ctrl+Z");
  QString hotkey_redo = QStringLiteral("Ctrl+Y");

  QJsonObject ToJson() const;
  static WindowSettings FromJson(const QJsonObject& obj);
};

struct AppSettings {
  int version = 1;
  WindowSettings window;
  QVector<ActionRow> rows;

  QJsonObject ToJson() const;
  static AppSettings FromJson(const QJsonObject& obj);
};

QString ActionKindToString(ActionKind k);
ActionKind ActionKindFromString(const QString& s);
QString TargetModeToString(TargetMode m);
TargetMode TargetModeFromString(const QString& s);
QString MouseButtonToString(MouseButton b);
MouseButton MouseButtonFromString(const QString& s);
QString KeyStrokeToString(KeyStroke s);
KeyStroke KeyStrokeFromString(const QString& s);
