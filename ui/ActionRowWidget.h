#pragma once

#include "../model/ActionRow.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class KeyCaptureEdit;
class ScreenshotButton;

class ActionRowWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ActionRowWidget(ActionKind kind, QWidget* parent = nullptr);

  ActionRow ToRow() const;
  void FromRow(const ActionRow& row);
  ActionKind Kind() const { return kind_; }
  void SetIndex(int index);
  void SetEditingEnabled(bool enabled);
  void SetHoldVisible(bool visible);
  // 顺序运行显示「按键/按下/弹起」；独立运行显示「长按」
  void SetKeyStrokeVisible(bool visible);
  // 顺序运行显示鼠标「按下/弹起」选项；独立运行隐藏
  void SetMousePressReleaseVisible(bool visible);
  // 是否含顺序专用的按下/弹起（切到独立运行前需提示）
  bool HasSequentialPressRelease() const;
  // 将按下/弹起改为独立运行可用的「按键 / 左键 / 右键」
  void ConvertPressReleaseForIndependent();
  void SetImagePath(const QString& path);
  void SetPoint(int x, int y);
  void SetPointHintPending();
  QString ImagePath() const { return image_path_; }

 signals:
  void removeRequested();
  void changed();
  void screenshotRequested(ActionRowWidget* self);
  void pickPointRequested(ActionRowWidget* self);

 private slots:
  void OnTargetModeChanged();
  void EmitChanged();

 private:
  void RebuildTargetUi();

  ActionKind kind_;
  QLabel* index_label_ = nullptr;
  QCheckBox* enabled_check_ = nullptr;
  KeyCaptureEdit* key_capture_ = nullptr;
  QComboBox* key_stroke_combo_ = nullptr;
  QComboBox* mouse_button_combo_ = nullptr;
  QComboBox* target_mode_combo_ = nullptr;
  QLineEdit* point_edit_ = nullptr;
  QPushButton* pick_point_btn_ = nullptr;
  ScreenshotButton* screenshot_btn_ = nullptr;
  QLineEdit* interval_min_ = nullptr;
  QLineEdit* interval_max_ = nullptr;
  QCheckBox* hold_check_ = nullptr;
  QPushButton* remove_btn_ = nullptr;
  QString image_path_;
};
