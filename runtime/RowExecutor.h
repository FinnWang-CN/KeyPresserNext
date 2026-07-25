#pragma once

#include "../engine/NextCoreAdapter.h"
#include "../model/ActionRow.h"

#include <QObject>
#include <QTimer>
#include <QVector>
#include <atomic>

class RowExecutor : public QObject {
  Q_OBJECT

 public:
  explicit RowExecutor(NextCoreAdapter* core, QObject* parent = nullptr);

  void SetRows(const QVector<ActionRow>& rows);
  void SetRunMode(const QString& mode);  // independent | sequential
  /** 0 = 无限；>0 为完整轮次数（顺序：整表一轮；独立：每行执行次数） */
  void SetRunCount(int count);
  /** 顺序：每轮结束后额外等待；独立：加在每次执行后的等待上（毫秒） */
  void SetLoopIntervalMs(int ms);
  /** 速率倍率，1.0 = 100%，越大越快（等待越短） */
  void SetSpeed(double speed);
  /** 与原版一致：运行时每次动作前恢复最小化并保持目标窗置顶 */
  void SetKeepTopmost(bool keep);
  bool IsRunning() const { return running_.load(); }

 public slots:
  void Start();
  void Stop();
  /** 在执行器线程上配置并启动（跨线程安全入口） */
  void ConfigureAndStart(const QVector<ActionRow>& rows, const QString& mode, int run_count,
                         int loop_interval_ms, double speed, bool keep_topmost);

 signals:
  void runningChanged(bool running);
  void statusMessage(const QString& msg);
  /** 单次动作明细（按键 / 识图成败等），供日志窗口使用 */
  void actionLog(const QString& msg);

 private slots:
  void OnIndependentTimeout();
  void OnSequentialTimeout();

 private:
  bool ExecuteRow(int index, const ActionRow& row);
  bool ResolveTarget(const ActionRow& row, int* out_x, int* out_y, QString* fail_reason = nullptr);
  void PerformKey(const ActionRow& row);
  void PerformMouse(const ActionRow& row, int x, int y);
  void EnsureTopmostIfNeeded();
  int RandomInterval(int min_ms, int max_ms) const;
  int ScaledMs(int ms) const;
  void FinishIfComplete();
  void EmitProgressStatus();

  NextCoreAdapter* core_ = nullptr;
  QVector<ActionRow> rows_;
  QString run_mode_ = QStringLiteral("sequential");
  int run_count_ = 0;           // 0 = 无限
  int loop_interval_ms_ = 0;
  double speed_ = 1.0;
  bool keep_topmost_ = false;
  std::atomic<bool> running_{false};
  qint64 last_topmost_ms_ = 0;

  QVector<QTimer*> independent_timers_;
  QVector<bool> hold_active_;
  QVector<int> independent_done_;  // 每行已执行次数（非长按）
  QTimer* sequential_timer_ = nullptr;
  int sequential_index_ = 0;
  int rounds_done_ = 0;
  int enabled_in_round_ = 0;  // 本轮已执行的启用行数
  int enabled_total_ = 0;
};
