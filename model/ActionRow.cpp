#include "ActionRow.h"

#include <QJsonArray>

QString ActionKindToString(ActionKind k) {
  return k == ActionKind::Mouse ? QStringLiteral("mouse") : QStringLiteral("key");
}

ActionKind ActionKindFromString(const QString& s) {
  return s.compare(QStringLiteral("mouse"), Qt::CaseInsensitive) == 0 ? ActionKind::Mouse
                                                                      : ActionKind::Key;
}

QString TargetModeToString(TargetMode m) {
  switch (m) {
    case TargetMode::Coordinate:
      return QStringLiteral("coordinate");
    case TargetMode::FindPic:
      return QStringLiteral("findpic");
    default:
      return QStringLiteral("none");
  }
}

TargetMode TargetModeFromString(const QString& s) {
  if (s.compare(QStringLiteral("coordinate"), Qt::CaseInsensitive) == 0) {
    return TargetMode::Coordinate;
  }
  if (s.compare(QStringLiteral("findpic"), Qt::CaseInsensitive) == 0) {
    return TargetMode::FindPic;
  }
  return TargetMode::None;
}

QString MouseButtonToString(MouseButton b) {
  switch (b) {
    case MouseButton::Right:
      return QStringLiteral("right");
    case MouseButton::Middle:
      return QStringLiteral("middle");
    case MouseButton::LeftDouble:
      return QStringLiteral("leftdouble");
    case MouseButton::WheelUp:
      return QStringLiteral("wheelup");
    case MouseButton::WheelDown:
      return QStringLiteral("wheeldown");
    case MouseButton::LeftDown:
      return QStringLiteral("leftdown");
    case MouseButton::LeftUp:
      return QStringLiteral("leftup");
    case MouseButton::RightDown:
      return QStringLiteral("rightdown");
    case MouseButton::RightUp:
      return QStringLiteral("rightup");
    default:
      return QStringLiteral("left");
  }
}

MouseButton MouseButtonFromString(const QString& s) {
  const QString t = s.toLower();
  if (t == QLatin1String("rightdown")) return MouseButton::RightDown;
  if (t == QLatin1String("rightup")) return MouseButton::RightUp;
  if (t == QLatin1String("leftdown")) return MouseButton::LeftDown;
  if (t == QLatin1String("leftup")) return MouseButton::LeftUp;
  if (t == QLatin1String("right")) return MouseButton::Right;
  if (t == QLatin1String("middle")) return MouseButton::Middle;
  if (t == QLatin1String("leftdouble")) return MouseButton::LeftDouble;
  if (t == QLatin1String("wheelup")) return MouseButton::WheelUp;
  if (t == QLatin1String("wheeldown")) return MouseButton::WheelDown;
  return MouseButton::Left;
}

QString KeyStrokeToString(KeyStroke s) {
  switch (s) {
    case KeyStroke::Down:
      return QStringLiteral("down");
    case KeyStroke::Up:
      return QStringLiteral("up");
    default:
      return QStringLiteral("press");
  }
}

KeyStroke KeyStrokeFromString(const QString& s) {
  const QString t = s.toLower();
  if (t == QLatin1String("down")) return KeyStroke::Down;
  if (t == QLatin1String("up")) return KeyStroke::Up;
  return KeyStroke::Press;
}

QJsonObject ActionTarget::ToJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("mode"), TargetModeToString(mode));
  if (mode == TargetMode::Coordinate) {
    obj.insert(QStringLiteral("x"), x);
    obj.insert(QStringLiteral("y"), y);
  } else if (mode == TargetMode::FindPic) {
    obj.insert(QStringLiteral("image"), image_path);
    obj.insert(QStringLiteral("sim"), sim);
  }
  return obj;
}

ActionTarget ActionTarget::FromJson(const QJsonObject& obj) {
  ActionTarget t;
  t.mode = TargetModeFromString(obj.value(QStringLiteral("mode")).toString(QStringLiteral("none")));
  t.x = obj.value(QStringLiteral("x")).toInt(0);
  t.y = obj.value(QStringLiteral("y")).toInt(0);
  t.image_path = obj.value(QStringLiteral("image")).toString();
  t.sim = obj.value(QStringLiteral("sim")).toDouble(0.9);
  return t;
}

QJsonObject ActionRow::ToJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("enabled"), enabled);
  obj.insert(QStringLiteral("kind"), ActionKindToString(kind));
  obj.insert(QStringLiteral("intervalMinMs"), interval_min_ms);
  obj.insert(QStringLiteral("intervalMaxMs"), interval_max_ms);
  obj.insert(QStringLiteral("target"), target.ToJson());
  if (kind == ActionKind::Key) {
    obj.insert(QStringLiteral("shortcut"), shortcut);
    obj.insert(QStringLiteral("hold"), hold);
    obj.insert(QStringLiteral("keyStroke"), KeyStrokeToString(key_stroke));
  } else {
    obj.insert(QStringLiteral("button"), MouseButtonToString(button));
  }
  return obj;
}

ActionRow ActionRow::FromJson(const QJsonObject& obj) {
  ActionRow row;
  row.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  row.kind = ActionKindFromString(obj.value(QStringLiteral("kind")).toString());
  row.interval_min_ms = obj.value(QStringLiteral("intervalMinMs")).toInt(1000);
  row.interval_max_ms = obj.value(QStringLiteral("intervalMaxMs")).toInt(row.interval_min_ms);
  row.shortcut = obj.value(QStringLiteral("shortcut")).toString();
  row.hold = obj.value(QStringLiteral("hold")).toBool(false);
  row.key_stroke = KeyStrokeFromString(
      obj.value(QStringLiteral("keyStroke")).toString(QStringLiteral("press")));
  row.button = MouseButtonFromString(obj.value(QStringLiteral("button")).toString());
  row.target = ActionTarget::FromJson(obj.value(QStringLiteral("target")).toObject());
  return row;
}

QJsonObject WindowSettings::ToJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("display"), display);
  obj.insert(QStringLiteral("mouse"), mouse);
  obj.insert(QStringLiteral("keypad"), keypad);
  obj.insert(QStringLiteral("bindMode"), bind_mode);
  obj.insert(QStringLiteral("topmost"), topmost);
  obj.insert(QStringLiteral("runMode"), run_mode);
  obj.insert(QStringLiteral("runCount"), run_count);
  obj.insert(QStringLiteral("runLoopIntervalMs"), run_loop_interval_ms);
  obj.insert(QStringLiteral("runSpeedPercent"), run_speed_percent);
  obj.insert(QStringLiteral("hotkeyStart"), hotkey_start);
  obj.insert(QStringLiteral("hotkeyStop"), hotkey_stop);
  obj.insert(QStringLiteral("hotkeyRecord"), hotkey_record);
  obj.insert(QStringLiteral("hotkeyStopRecord"), hotkey_stop_record);
  obj.insert(QStringLiteral("hotkeySelectWindow"), hotkey_select_window);
  obj.insert(QStringLiteral("hotkeyHighlightWindow"), hotkey_highlight_window);
  obj.insert(QStringLiteral("hotkeyAddKey"), hotkey_add_key);
  obj.insert(QStringLiteral("hotkeyAddMouse"), hotkey_add_mouse);
  obj.insert(QStringLiteral("hotkeyClear"), hotkey_clear);
  obj.insert(QStringLiteral("hotkeyUndo"), hotkey_undo);
  obj.insert(QStringLiteral("hotkeyRedo"), hotkey_redo);
  return obj;
}

WindowSettings WindowSettings::FromJson(const QJsonObject& obj) {
  WindowSettings w;
  w.display = obj.value(QStringLiteral("display")).toString(QStringLiteral("normal.auto"));
  w.mouse = obj.value(QStringLiteral("mouse")).toString(QStringLiteral("windows"));
  w.keypad = obj.value(QStringLiteral("keypad")).toString(QStringLiteral("windows"));
  w.bind_mode = obj.value(QStringLiteral("bindMode")).toInt(0);
  w.topmost = obj.value(QStringLiteral("topmost")).toBool(false);
  w.run_mode = obj.value(QStringLiteral("runMode")).toString(QStringLiteral("sequential"));
  w.run_count = obj.value(QStringLiteral("runCount")).toInt(0);
  w.run_loop_interval_ms = obj.value(QStringLiteral("runLoopIntervalMs")).toInt(0);
  w.run_speed_percent = obj.value(QStringLiteral("runSpeedPercent")).toInt(100);
  if (w.run_count < 0) {
    w.run_count = 0;
  }
  if (w.run_loop_interval_ms < 0) {
    w.run_loop_interval_ms = 0;
  }
  if (w.run_speed_percent < 10) {
    w.run_speed_percent = 10;
  } else if (w.run_speed_percent > 1000) {
    w.run_speed_percent = 1000;
  }
  w.hotkey_start = obj.value(QStringLiteral("hotkeyStart")).toString(QStringLiteral("F9"));
  // 开始/停止运行已共用同一键；忽略旧的独立停止运行配置
  w.hotkey_stop = w.hotkey_start;
  w.hotkey_record = obj.value(QStringLiteral("hotkeyRecord")).toString(QStringLiteral("F8"));
  // 开始/停止录制已共用同一键；忽略旧的独立停止录制配置
  w.hotkey_stop_record = w.hotkey_record;
  w.hotkey_select_window = obj.value(QStringLiteral("hotkeySelectWindow")).toString();
  w.hotkey_highlight_window = obj.value(QStringLiteral("hotkeyHighlightWindow")).toString();
  w.hotkey_add_key = obj.value(QStringLiteral("hotkeyAddKey")).toString();
  w.hotkey_add_mouse = obj.value(QStringLiteral("hotkeyAddMouse")).toString();
  w.hotkey_clear = obj.value(QStringLiteral("hotkeyClear")).toString();
  w.hotkey_undo = obj.value(QStringLiteral("hotkeyUndo")).toString(QStringLiteral("Ctrl+Z"));
  w.hotkey_redo = obj.value(QStringLiteral("hotkeyRedo")).toString(QStringLiteral("Ctrl+Y"));
  // 忽略旧文件中的 geometry 字段
  return w;
}

QJsonObject AppSettings::ToJson() const {
  QJsonObject root;
  root.insert(QStringLiteral("_comment"),
              QStringLiteral("KeyPresserNext settings. Edit carefully. Images live in *.assets/"));
  root.insert(QStringLiteral("version"), version);
  root.insert(QStringLiteral("window"), window.ToJson());
  QJsonArray arr;
  for (const ActionRow& row : rows) {
    arr.append(row.ToJson());
  }
  root.insert(QStringLiteral("rows"), arr);
  return root;
}

AppSettings AppSettings::FromJson(const QJsonObject& obj) {
  AppSettings s;
  s.version = obj.value(QStringLiteral("version")).toInt(1);
  s.window = WindowSettings::FromJson(obj.value(QStringLiteral("window")).toObject());
  const QJsonArray arr = obj.value(QStringLiteral("rows")).toArray();
  for (const QJsonValue& v : arr) {
    s.rows.append(ActionRow::FromJson(v.toObject()));
  }
  return s;
}
