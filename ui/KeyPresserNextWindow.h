#pragma once

#include "../engine/NextCoreAdapter.h"
#include "../model/ActionRow.h"
#include "../model/RecordConverter.h"
#include "../runtime/RowExecutor.h"

#include <QMainWindow>
#include <QList>
#include <QVector>
#include <windows.h>

class QAction;
class QComboBox;
class QCheckBox;
class QEvent;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QSpinBox;
class QSplitter;
class QTimer;
class QThread;
class QVBoxLayout;
class QPushButton;
class ActionRowWidget;
class GlobalHotkeys;
class HotkeySettingsDialog;
class KeyCaptureEdit;
class KpnInstanceHost;
class AppFocusHost;
class KeyPresserNextWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit KeyPresserNextWindow(const QString& open_path = QString(),
                                bool force_open_duplicate = false,
                                QWidget* parent = nullptr);
  ~KeyPresserNextWindow() override;

 protected:
  void closeEvent(QCloseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void OnSelectWindow();
  void OnHighlightWindow();
  void OnTestCapture();
  void OnTestMouse();
  void OnTestKey();
  void OnSetTestMousePoint();
  void OnBindModesChanged();
  void OnAddKeyRow();
  void OnAddMouseRow();
  void OnBulkAddRows();
  void OnClearRows();
  void OnUndoRows();
  void OnRedoRows();
  void OnStart();
  void OnStop();
  void OnNewKpns();
  void OnSaveKpns();
  void OnSaveKpnsAs();
  void OnOpenKpns();
  void OnAbout();
  void OnShowChangelog();
  void OnOpenOfficialSite();
  void OnOpenHotkeySettings();
  void OnHotkeysChanged();
  void OnStartRecord();
  void OnStopRecord();
  void OnHotkeyStart();
  void OnHotkeyStop();
  void OnHotkeyRecord();
  void OnRowScreenshot(ActionRowWidget* row);
  void OnRowPickPoint(ActionRowWidget* row);
  void OnRunningChanged(bool running);

 private:
  void BuildUi();
  void BuildMenus();
  void RebuildRecentMenu();
  void PopulateModeCombos();
  void ApplySettingsToUi(const AppSettings& settings);
  AppSettings CollectSettingsFromUi() const;
  void ApplyHotkeysToEngine();
  void SyncHotkeyRestrictMode();
  void SyncHotkeyDialogFromSettings();
  void UpdateActionButtonTexts();
  void UpdateEditActionButtonsEnabled();
  static QString ButtonTextWithHotkey(const QString& base, const QString& hotkey_label);
  void RefreshRowIndexes();
  void RefreshHoldVisibility();
  void OnRunModeChanged();
  void SetRowsEditingEnabled(bool enabled);
  ActionRowWidget* AppendRowWidget(ActionKind kind);
  void WireRowWidget(ActionRowWidget* w);
  void ClearRowWidgets();
  void AppendRows(const QVector<ActionRow>& rows, bool append);
  QVector<ActionRow> CollectRows() const;
  void ApplyRowsSnapshot(const QVector<ActionRow>& rows);
  void PushRowsHistory();
  void ResetRowsHistory();
  void UpdateUndoRedoButtons();
  void NotifyRowsEdited();           // 字段编辑：防抖后入栈
  void NotifyRowsStructureEdited();  // 增删清空等：立即入栈
  void SchedulePushRowsHistory();
  void AppendLog(const QString& msg);
  void FlushPendingLogs();
  void SetLogCollapsed(bool collapsed);
  void NotifyStatus(const QString& msg, int timeoutMs = 0, bool appendLog = true);
  bool EnsureBound();
  // 校验窗口并用当前截图/键鼠模式绑定（测试截图、行截图、开始运行共用）
  bool EnsureBoundForCapture(QString* error = nullptr);
  // 勾选「置顶」时，与原版一样在动作前恢复并置顶目标窗
  void KeepTargetTopmostIfNeeded();
  void RefreshWindowTitleLabel();
  void SetBindControlsEnabled(bool enabled);
  void RefreshTestActionTips();
  void MarkDirty();
  void SetCurrentFile(const QString& path, bool dirty);
  void UpdateWindowTitle();
  bool SaveToPath(const QString& path);
  // 已有路径保存：窗口名与文件名不一致时询问覆盖 / 另存为
  bool SaveExistingWithWindowCheck();
  bool OpenFromPath(const QString& path, bool force_open_duplicate = false);
  void RaiseMainWindow();
  bool PromptSaveBeforeClose();
  QString CurrentDisplay() const;
  QString CurrentMouse() const;
  QString CurrentKeypad() const;
  QString HotkeyLabel(const QString& storage) const;

  // Recording helpers
  void InstallRecordHooks();
  void UninstallRecordHooks();
  static LRESULT CALLBACK RecordKeyboardProc(int code, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK RecordMouseProc(int code, WPARAM wparam, LPARAM lparam);
  void AppendRecordEvent(const RecordEvent& ev);
  bool BelongsToTarget(HWND hwnd) const;
  bool IsTargetForeground() const;
  bool ScreenToClientInTarget(int screen_x, int screen_y, int* client_x, int* client_y) const;
  bool IsRecordHotkeyVk(int vk) const;
  void ShowStopHotkeyHint(const QString& mode_text);
  void HideStopHotkeyHint();
  void RestoreMainWindow();
  QString StopHotkeyLabel() const;

  NextCoreAdapter core_;
  RowExecutor* executor_ = nullptr;
  QThread* executor_thread_ = nullptr;
  GlobalHotkeys* hotkeys_ = nullptr;
  HotkeySettingsDialog* hotkey_dialog_ = nullptr;
  KpnInstanceHost* kpn_host_ = nullptr;
  AppFocusHost* app_focus_host_ = nullptr;

  HWND target_hwnd_ = nullptr;
  HWINEVENTHOOK select_hook_ = nullptr;
  bool selecting_window_ = false;
  static KeyPresserNextWindow* select_instance_;
  static void CALLBACK SelectWinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG id_object,
                                          LONG id_child, DWORD thread, DWORD time);
  void FinishSelectWindow(HWND hwnd);
  void CancelSelectWindowHook();
  void StartPickPoint(ActionRowWidget* row);  // row==nullptr 表示测试区取点
  void CancelPickPointHook();
  void FinishPickPoint(int client_x, int client_y);
  static LRESULT CALLBACK PickMouseProc(int code, WPARAM wparam, LPARAM lparam);

  HHOOK pick_mouse_hook_ = nullptr;
  ActionRowWidget* pick_row_ = nullptr;  // 非空=动作行取点；空=测试区取点
  static KeyPresserNextWindow* pick_instance_;

  QLabel* window_label_ = nullptr;
  QComboBox* display_combo_ = nullptr;
  QComboBox* mouse_combo_ = nullptr;
  QComboBox* keypad_combo_ = nullptr;
  QComboBox* bind_mode_combo_ = nullptr;
  QComboBox* run_mode_combo_ = nullptr;
  QCheckBox* topmost_check_ = nullptr;
  QPushButton* select_btn_ = nullptr;
  QPushButton* highlight_btn_ = nullptr;
  QPushButton* test_capture_btn_ = nullptr;
  QLineEdit* test_mouse_point_edit_ = nullptr;
  QPushButton* test_mouse_point_btn_ = nullptr;
  QPushButton* test_mouse_btn_ = nullptr;
  KeyCaptureEdit* test_key_capture_ = nullptr;
  QPushButton* test_key_btn_ = nullptr;
  QPushButton* add_key_btn_ = nullptr;
  QPushButton* add_mouse_btn_ = nullptr;
  QPushButton* bulk_add_btn_ = nullptr;
  QPushButton* clear_btn_ = nullptr;
  QPushButton* undo_btn_ = nullptr;
  QPushButton* redo_btn_ = nullptr;
  QPushButton* start_btn_ = nullptr;  // 开始/停止运行同一按钮
  QPushButton* start_rec_btn_ = nullptr;
  QSpinBox* run_count_spin_ = nullptr;
  QSpinBox* run_gap_spin_ = nullptr;
  QSpinBox* run_speed_spin_ = nullptr;
  QVBoxLayout* rows_layout_ = nullptr;
  QVector<ActionRowWidget*> row_widgets_;
  QVector<QVector<ActionRow>> rows_history_;
  int rows_history_index_ = -1;
  bool applying_rows_history_ = false;
  QTimer* rows_history_timer_ = nullptr;
  QLabel* status_label_ = nullptr;
  QWidget* bottom_panel_ = nullptr;
  QSplitter* main_split_ = nullptr;
  QFrame* log_panel_ = nullptr;
  QPlainTextEdit* log_edit_ = nullptr;
  QPushButton* log_toggle_btn_ = nullptr;
  QPushButton* clear_log_btn_ = nullptr;
  bool log_collapsed_ = false;
  QList<int> log_split_sizes_;  // 折叠前分割条尺寸，展开时还原
  QString last_log_line_;
  QStringList pending_log_lines_;
  QTimer* log_flush_timer_ = nullptr;
  QString kpns_path_;
  QString window_title_full_;  // 选窗完整标题；显示区按宽度省略
  // 测试鼠标：未设置时用客户区中心
  bool test_mouse_point_set_ = false;
  int test_mouse_x_ = 0;
  int test_mouse_y_ = 0;
  // 测试键盘：存储文本，默认空格
  QString test_key_storage_ = QStringLiteral("A");
  bool settings_dirty_ = false;
  bool loading_settings_ = false;
  QMenu* recent_menu_ = nullptr;

  QString hotkey_start_ = QStringLiteral("F9");
  QString hotkey_stop_ = QStringLiteral("F9");  // 与 hotkey_start_ 共用
  QString hotkey_record_ = QStringLiteral("F8");
  QString hotkey_stop_record_ = QStringLiteral("F8");
  QString hotkey_select_window_;
  QString hotkey_highlight_window_;
  QString hotkey_add_key_;
  QString hotkey_add_mouse_;
  QString hotkey_clear_;
  QString hotkey_undo_ = QStringLiteral("Ctrl+Z");
  QString hotkey_redo_ = QStringLiteral("Ctrl+Y");

  bool recording_ = false;
  HHOOK record_kb_hook_ = nullptr;
  HHOOK record_mouse_hook_ = nullptr;
  QVector<RecordEvent> record_events_;
  qint64 last_record_tick_ = 0;
  static KeyPresserNextWindow* record_instance_;
};
