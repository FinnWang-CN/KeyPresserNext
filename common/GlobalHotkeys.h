#pragma once

#include "KeyBinding.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <windows.h>

class GlobalHotkeys : public QObject {
  Q_OBJECT

 public:
  enum class Action {
    Start = 0,
    Stop,
    Record,
    StopRecord,
    SelectWindow,
    HighlightWindow,
    AddKey,
    AddMouse,
    Clear,
    Undo,
    Redo,
    Count
  };

  /** 运行/录制中只允许对应「停止」快捷键生效 */
  enum class RestrictMode {
    None = 0,
    OnlyStartStop,    // 运行中：仅开始/停止运行键
    OnlyRecordStop    // 录制中：仅开始/停止录制键
  };

  explicit GlobalHotkeys(QObject* parent = nullptr);
  ~GlobalHotkeys() override;

  bool Install();
  void Uninstall();

  void SetFromStorage(Action action, const QString& text);
  QString Storage(Action action) const;
  QString Label(Action action) const;
  int MainVk(Action action) const;

  void SetRestrictMode(RestrictMode mode);
  RestrictMode GetRestrictMode() const { return restrict_mode_; }
  bool IsActionAllowed(Action action) const;

  bool IsConfiguredHotkeyVk(int vk) const;

  static int VkFromName(const QString& name);
  static QString NameFromVk(int vk);
  static QString NormalizeStorage(const QString& text);
  static bool BindingsEqual(const QString& a, const QString& b);
  /** 出厂默认快捷键（存储文本，如 F9） */
  static QString DefaultStorage(Action action);

 signals:
  void startPressed();
  void stopPressed();
  void recordPressed();
  void stopRecordPressed();
  void selectWindowPressed();
  void highlightWindowPressed();
  void addKeyPressed();
  void addMousePressed();
  void clearPressed();
  void undoPressed();
  void redoPressed();

 private:
  static LRESULT CALLBACK LowLevelProc(int code, WPARAM wparam, LPARAM lparam);
  static bool BindingMatches(const KeyBinding& binding, unsigned int vk);
  void EmitAction(Action action);
  static GlobalHotkeys* instance_;

  HHOOK hook_ = nullptr;
  KeyBinding bindings_[static_cast<int>(Action::Count)];
  QSet<int> latch_;
  RestrictMode restrict_mode_ = RestrictMode::None;
};
