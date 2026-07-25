#pragma once

#include "ActionRow.h"

#include <QString>
#include <QVector>

struct RecordEvent {
  int delay_ms = 0;
  QString type;    // key_down/key_up/mouse_down/mouse_up/mouse_wheel
  int vk = 0;
  QString button;  // left/right/middle
  int x = 0;
  int y = 0;
  int wheel = 0;
};

class RecordConverter {
 public:
  // Collapse raw events into editable ActionRows (drops pure mouse_move).
  static QVector<ActionRow> Convert(const QVector<RecordEvent>& events);
};
