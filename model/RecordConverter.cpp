#include "RecordConverter.h"

#include "../common/KeyBinding.h"

#include <QSet>
#include <windows.h>

namespace {

constexpr int kDefaultAfterMs = 1000;

bool IsModifier(int vk) {
  return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_CONTROL ||
         vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_MENU || vk == VK_LMENU ||
         vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN;
}

QString BuildShortcut(const QVector<int>& mods, int main_vk) {
  KeyBinding b;
  b.modifierVkCodes = mods;
  b.mainVkCode = main_vk;
  return b.storageText();
}

MouseButton PressReleaseButton(const QString& button, bool down) {
  const QString b = button.isEmpty() ? QStringLiteral("left") : button.toLower();
  if (b == QLatin1String("right")) {
    return down ? MouseButton::RightDown : MouseButton::RightUp;
  }
  return down ? MouseButton::LeftDown : MouseButton::LeftUp;
}

bool IsLeftOrRight(const QString& button) {
  const QString b = button.isEmpty() ? QStringLiteral("left") : button.toLower();
  return b == QLatin1String("left") || b == QLatin1String("right");
}

}  // namespace

QVector<ActionRow> RecordConverter::Convert(const QVector<RecordEvent>& events) {
  QVector<ActionRow> rows;
  QSet<int> down_mods;
  int pending_key = 0;
  QVector<int> pending_key_mods;
  // 中键仍合并为单击；左/右键拆成按下、弹起两行
  QString pending_middle_button;
  int pending_middle_x = 0;
  int pending_middle_y = 0;
  int pending_middle_delay = kDefaultAfterMs;

  // 回放语义：做完当前行 → 等本行间隔 → 下一行。
  // 录制事件的 delay = 距上一事件的时间，应写到「上一行」的间隔上。
  auto set_previous_after_wait = [&](int delay_ms) {
    if (rows.isEmpty()) {
      return;
    }
    const int d = qMax(50, delay_ms);
    rows.last().interval_min_ms = d;
    rows.last().interval_max_ms = d;
  };

  auto append_key = [&](KeyStroke stroke, const QVector<int>& mods, int main_vk, int delay_ms) {
    set_previous_after_wait(delay_ms);
    ActionRow row;
    row.kind = ActionKind::Key;
    row.enabled = true;
    row.interval_min_ms = kDefaultAfterMs;
    row.interval_max_ms = kDefaultAfterMs;
    row.key_stroke = stroke;
    row.shortcut = BuildShortcut(mods, main_vk);
    row.target.mode = TargetMode::None;
    rows.append(row);
  };

  auto flush_pending_key_up = [&](int delay_ms) {
    if (pending_key == 0) {
      return;
    }
    append_key(KeyStroke::Up, pending_key_mods, pending_key, delay_ms);
    pending_key = 0;
    pending_key_mods.clear();
  };

  auto append_mouse = [&](MouseButton button, int x, int y, int delay_ms) {
    set_previous_after_wait(delay_ms);
    ActionRow row;
    row.kind = ActionKind::Mouse;
    row.enabled = true;
    row.interval_min_ms = kDefaultAfterMs;
    row.interval_max_ms = kDefaultAfterMs;
    row.button = button;
    row.target.mode = TargetMode::Coordinate;
    row.target.x = x;
    row.target.y = y;
    rows.append(row);
  };

  auto flush_middle = [&]() {
    if (pending_middle_button.isEmpty()) {
      return;
    }
    append_mouse(MouseButton::Middle, pending_middle_x, pending_middle_y, pending_middle_delay);
    pending_middle_button.clear();
  };

  for (const RecordEvent& ev : events) {
    if (ev.type == QLatin1String("key_down")) {
      if (IsModifier(ev.vk)) {
        down_mods.insert(ev.vk);
      } else {
        flush_middle();
        QVector<int> mods;
        for (int m : down_mods) {
          mods.append(m);
        }
        if (pending_key != 0) {
          // 上一个主键未弹起又按下：先补弹起，间隔算在「按下→弹起」上
          flush_pending_key_up(qMax(50, ev.delay_ms));
          append_key(KeyStroke::Down, mods, ev.vk, 50);
        } else {
          append_key(KeyStroke::Down, mods, ev.vk, qMax(50, ev.delay_ms));
        }
        pending_key = ev.vk;
        pending_key_mods = mods;
      }
    } else if (ev.type == QLatin1String("key_up")) {
      if (IsModifier(ev.vk)) {
        // 修饰键抬起前，若主键还按着，先弹起主键行
        if (pending_key != 0) {
          flush_pending_key_up(qMax(50, ev.delay_ms));
        }
        down_mods.remove(ev.vk);
      } else if (pending_key == ev.vk) {
        flush_pending_key_up(qMax(50, ev.delay_ms));
      } else {
        // 孤立弹起：仍记一行弹起（无修饰）
        flush_middle();
        append_key(KeyStroke::Up, {}, ev.vk, qMax(50, ev.delay_ms));
      }
    } else if (ev.type == QLatin1String("mouse_down")) {
      const int d = qMax(50, ev.delay_ms);
      const int mouse_delay = (pending_key != 0) ? 50 : d;
      if (pending_key != 0) {
        flush_pending_key_up(d);
      }
      const QString btn = ev.button.isEmpty() ? QStringLiteral("left") : ev.button.toLower();
      if (IsLeftOrRight(btn)) {
        flush_middle();
        append_mouse(PressReleaseButton(btn, true), ev.x, ev.y, mouse_delay);
      } else {
        flush_middle();
        pending_middle_button = btn;
        pending_middle_x = ev.x;
        pending_middle_y = ev.y;
        pending_middle_delay = mouse_delay;
      }
    } else if (ev.type == QLatin1String("mouse_up") || ev.type == QLatin1String("mouse_click")) {
      const int d = qMax(50, ev.delay_ms);
      const int mouse_delay = (pending_key != 0) ? 50 : d;
      if (pending_key != 0) {
        flush_pending_key_up(d);
      }
      const QString btn = ev.button.isEmpty() ? QStringLiteral("left") : ev.button.toLower();
      if (IsLeftOrRight(btn)) {
        flush_middle();
        append_mouse(PressReleaseButton(btn, false), ev.x, ev.y, mouse_delay);
      } else {
        if (pending_middle_button.isEmpty()) {
          pending_middle_button = btn;
          pending_middle_x = ev.x;
          pending_middle_y = ev.y;
          pending_middle_delay = mouse_delay;
        }
        flush_middle();
      }
    } else if (ev.type == QLatin1String("mouse_wheel")) {
      const int d = qMax(50, ev.delay_ms);
      const int mouse_delay = (pending_key != 0) ? 50 : d;
      if (pending_key != 0) {
        flush_pending_key_up(d);
      }
      flush_middle();
      append_mouse(ev.wheel >= 0 ? MouseButton::WheelUp : MouseButton::WheelDown, ev.x, ev.y,
                   mouse_delay);
    }
    // mouse_move ignored
  }
  flush_pending_key_up(kDefaultAfterMs);
  flush_middle();
  return rows;
}
