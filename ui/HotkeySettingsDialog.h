#pragma once

#include "../common/GlobalHotkeys.h"

#include <QDialog>
#include <QHash>
#include <QString>

class KeyCaptureEdit;

class HotkeySettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit HotkeySettingsDialog(QWidget* parent = nullptr);

  void SetStorage(GlobalHotkeys::Action action, const QString& text);
  QString Storage(GlobalHotkeys::Action action) const;
  void RestoreDefaults();

 signals:
  void hotkeysChanged();

 private:
  KeyCaptureEdit* MakeCapture();
  QWidget* MakeRow(GlobalHotkeys::Action action, KeyCaptureEdit** out_edit);
  void OnEditChanged(GlobalHotkeys::Action action);
  bool HasDuplicate(GlobalHotkeys::Action except, const QString& storage) const;

  KeyCaptureEdit* edits_[static_cast<int>(GlobalHotkeys::Action::Count)] = {};
  QHash<int, QString> last_valid_;
};
