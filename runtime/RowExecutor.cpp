#include "RowExecutor.h"

#include "../common/KeyBinding.h"
#include "../common/WindowSelectHelper.h"

#include <QDateTime>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QtGlobal>
#include <atomic>

namespace {

QString MouseButtonLabel(MouseButton b) {
  switch (b) {
    case MouseButton::Right:
      return QStringLiteral("右键单击");
    case MouseButton::Middle:
      return QStringLiteral("中键单击");
    case MouseButton::LeftDouble:
      return QStringLiteral("左键双击");
    case MouseButton::WheelUp:
      return QStringLiteral("滚轮上");
    case MouseButton::WheelDown:
      return QStringLiteral("滚轮下");
    case MouseButton::LeftDown:
      return QStringLiteral("左键按下");
    case MouseButton::LeftUp:
      return QStringLiteral("左键弹起");
    case MouseButton::RightDown:
      return QStringLiteral("右键按下");
    case MouseButton::RightUp:
      return QStringLiteral("右键弹起");
    default:
      return QStringLiteral("左键单击");
  }
}

QString KeyActionLabel(const ActionRow& row) {
  const QString key = row.shortcut.isEmpty() ? QStringLiteral("(空)") : row.shortcut;
  if (row.hold) {
    return QStringLiteral("长按 %1").arg(key);
  }
  switch (row.key_stroke) {
    case KeyStroke::Down:
      return QStringLiteral("按下 %1").arg(key);
    case KeyStroke::Up:
      return QStringLiteral("弹起 %1").arg(key);
    default:
      return QStringLiteral("按键 %1").arg(key);
  }
}

}  // namespace

RowExecutor::RowExecutor(NextCoreAdapter* core, QObject* parent) : QObject(parent), core_(core) {
  sequential_timer_ = new QTimer(this);
  sequential_timer_->setSingleShot(true);
  connect(sequential_timer_, &QTimer::timeout, this, &RowExecutor::OnSequentialTimeout);
}

void RowExecutor::SetRows(const QVector<ActionRow>& rows) {
  rows_ = rows;
}

void RowExecutor::SetRunMode(const QString& mode) {
  run_mode_ = mode;
}

void RowExecutor::SetRunCount(int count) {
  run_count_ = qMax(0, count);
}

void RowExecutor::SetLoopIntervalMs(int ms) {
  loop_interval_ms_ = qMax(0, ms);
}

void RowExecutor::SetSpeed(double speed) {
  speed_ = qBound(0.1, speed, 10.0);
}

void RowExecutor::SetKeepTopmost(bool keep) {
  keep_topmost_ = keep;
}

void RowExecutor::ConfigureAndStart(const QVector<ActionRow>& rows, const QString& mode, int run_count,
                                    int loop_interval_ms, double speed, bool keep_topmost) {
  SetRows(rows);
  SetRunMode(mode);
  SetRunCount(run_count);
  SetLoopIntervalMs(loop_interval_ms);
  SetSpeed(speed);
  SetKeepTopmost(keep_topmost);
  Start();
}

int RowExecutor::ScaledMs(int ms) const {
  return qMax(1, qRound(static_cast<double>(ms) / speed_));
}

void RowExecutor::EmitProgressStatus() {
  if (run_count_ <= 0) {
    emit statusMessage(QStringLiteral("运行中（无限）"));
    return;
  }
  int current = rounds_done_;
  if (run_mode_ == QLatin1String("sequential")) {
    // rounds_done_ = 已完成轮数；进行中显示下一轮序号
    current = qMin(rounds_done_ + 1, run_count_);
  } else {
    // 独立：rounds_done_ = 各行已完成次数的最小值
    current = qBound(0, rounds_done_, run_count_);
  }
  emit statusMessage(QStringLiteral("运行中 %1 / %2").arg(current).arg(run_count_));
}

void RowExecutor::Start() {
  if (running_.load() || !core_ || !core_->IsReady()) {
    emit statusMessage(QStringLiteral("引擎未就绪或已在运行"));
    return;
  }
  if (!core_->IsBound()) {
    emit statusMessage(QStringLiteral("请先选择并绑定窗口"));
    return;
  }
  running_.store(true);
  rounds_done_ = 0;
  enabled_in_round_ = 0;
  enabled_total_ = 0;
  for (const ActionRow& row : rows_) {
    if (row.enabled) {
      ++enabled_total_;
    }
  }
  emit runningChanged(true);
  EmitProgressStatus();

  if (run_mode_ == QLatin1String("sequential")) {
    sequential_index_ = 0;
    sequential_timer_->setSingleShot(true);
    sequential_timer_->start(50);
  } else {
    qDeleteAll(independent_timers_);
    independent_timers_.clear();
    hold_active_.fill(false, rows_.size());
    independent_done_.fill(0, rows_.size());
    for (int i = 0; i < rows_.size(); ++i) {
      if (!rows_[i].enabled) {
        independent_timers_.append(nullptr);
        continue;
      }
      auto* timer = new QTimer(this);
      timer->setProperty("rowIndex", i);
      connect(timer, &QTimer::timeout, this, &RowExecutor::OnIndependentTimeout);
      independent_timers_.append(timer);
      // 先尽快执行一遍，之后才按本行间隔等待（与「做完再等」一致）
      timer->start(50);
    }
  }
}

void RowExecutor::Stop() {
  if (!running_.load()) {
    return;
  }
  running_.store(false);
  sequential_timer_->stop();
  for (QTimer* t : independent_timers_) {
    if (t) {
      t->stop();
      t->deleteLater();
    }
  }
  independent_timers_.clear();

  for (int i = 0; i < hold_active_.size(); ++i) {
    if (!hold_active_[i] || i >= rows_.size()) {
      continue;
    }
    const KeyBinding b = KeyBinding::fromDisplayText(rows_[i].shortcut);
    if (b.isValid()) {
      core_->KeyUp(windowsVkForKeyInput(b.mainVkCode));
      const auto mods = b.modifierSendCodes(KeySendProfile::WindowsVk);
      for (int m : mods) {
        core_->KeyUp(windowsVkForKeyInput(m));
      }
    }
  }
  hold_active_.clear();
  independent_done_.clear();
  emit runningChanged(false);
  emit statusMessage(QStringLiteral("已停止"));
}

void RowExecutor::FinishIfComplete() {
  if (!running_.load() || run_count_ <= 0 || run_mode_ != QLatin1String("independent")) {
    return;
  }
  bool has_countable = false;
  for (int i = 0; i < rows_.size(); ++i) {
    if (!rows_[i].enabled) {
      continue;
    }
    if (rows_[i].kind == ActionKind::Key && rows_[i].hold) {
      continue;
    }
    has_countable = true;
    if (i >= independent_done_.size() || independent_done_[i] < run_count_) {
      return;
    }
  }
  if (has_countable) {
    Stop();
  }
}

void RowExecutor::OnIndependentTimeout() {
  auto* timer = qobject_cast<QTimer*>(sender());
  if (!timer || !running_.load()) {
    return;
  }
  const int index = timer->property("rowIndex").toInt();
  if (index < 0 || index >= rows_.size()) {
    return;
  }
  const ActionRow& row = rows_[index];
  if (run_count_ > 0 && index < independent_done_.size() &&
      !(row.kind == ActionKind::Key && row.hold) && independent_done_[index] >= run_count_) {
    timer->stop();
    FinishIfComplete();
    return;
  }

  if (row.kind == ActionKind::Key && row.hold) {
    if (index < hold_active_.size() && hold_active_[index]) {
      // keep holding
    } else {
      ExecuteRow(index, row);
      if (index < hold_active_.size()) {
        hold_active_[index] = true;
      }
    }
  } else {
    ExecuteRow(index, row);
    if (index < independent_done_.size()) {
      ++independent_done_[index];
      if (run_count_ > 0) {
        rounds_done_ = 0;
        for (int n : independent_done_) {
          rounds_done_ = qMax(rounds_done_, n);
        }
        // 进度按各行完成的最小次数更合理
        int min_done = run_count_;
        for (int i = 0; i < rows_.size(); ++i) {
          if (!rows_[i].enabled || (rows_[i].kind == ActionKind::Key && rows_[i].hold)) {
            continue;
          }
          min_done = qMin(min_done, independent_done_[i]);
        }
        rounds_done_ = min_done;
        EmitProgressStatus();
      }
    }
  }

  if (run_count_ > 0 && index < independent_done_.size() &&
      !(row.kind == ActionKind::Key && row.hold) && independent_done_[index] >= run_count_) {
    timer->stop();
    FinishIfComplete();
    return;
  }

  const int base = RandomInterval(row.interval_min_ms, row.interval_max_ms) + loop_interval_ms_;
  timer->start(ScaledMs(base));
}

void RowExecutor::OnSequentialTimeout() {
  if (!running_.load()) {
    return;
  }
  if (rows_.isEmpty() || enabled_total_ == 0) {
    if (run_count_ > 0) {
      Stop();
    } else {
      sequential_timer_->start(200);
    }
    return;
  }

  int guard = 0;
  while (guard < rows_.size() && !rows_[sequential_index_ % rows_.size()].enabled) {
    sequential_index_ = (sequential_index_ + 1) % rows_.size();
    ++guard;
  }
  if (guard >= rows_.size()) {
    sequential_timer_->start(200);
    return;
  }

  const int index = sequential_index_ % rows_.size();
  const ActionRow& row = rows_[index];
  ExecuteRow(index, row);
  sequential_index_ = (index + 1) % rows_.size();
  ++enabled_in_round_;

  if (enabled_in_round_ >= enabled_total_) {
    enabled_in_round_ = 0;
    ++rounds_done_;
    EmitProgressStatus();
    if (run_count_ > 0 && rounds_done_ >= run_count_) {
      Stop();
      return;
    }
    const int gap = ScaledMs(qMax(1, loop_interval_ms_ > 0 ? loop_interval_ms_ : 1));
    // 轮间：有间隔用间隔；无间隔则紧接用该行间隔收尾感，仍走下一行的行间隔
    if (loop_interval_ms_ > 0) {
      sequential_timer_->start(gap);
      return;
    }
  }

  sequential_timer_->start(
      ScaledMs(RandomInterval(row.interval_min_ms, row.interval_max_ms)));
}

bool RowExecutor::ResolveTarget(const ActionRow& row, int* out_x, int* out_y,
                                QString* fail_reason) {
  if (row.target.mode == TargetMode::None) {
    return true;
  }
  if (row.target.mode == TargetMode::Coordinate) {
    if (out_x) *out_x = row.target.x;
    if (out_y) *out_y = row.target.y;
    return true;
  }
  if (row.target.mode == TargetMode::FindPic) {
    // 与测试截图 / 行截图同一绑定会话：在客户区 (0,0)-(w,h) 内找图
    HWND hwnd = core_->BindHwnd();
    int w = 0;
    int h = 0;
    if (!hwnd || !core_->GetClientSize(hwnd, &w, &h) || w <= 0 || h <= 0) {
      if (fail_reason) {
        *fail_reason = QStringLiteral("无法获取窗口客户区");
      }
      return false;
    }
    QPoint pos;
    if (!core_->FindPic(0, 0, w, h, row.target.image_path, row.target.sim, &pos)) {
      if (fail_reason) {
        *fail_reason = QStringLiteral("未匹配到图片（相似度 %1）")
                           .arg(row.target.sim, 0, 'f', 2);
      }
      return false;
    }
    if (out_x) *out_x = pos.x();
    if (out_y) *out_y = pos.y();
    return true;
  }
  if (fail_reason) {
    *fail_reason = QStringLiteral("未知目标模式");
  }
  return false;
}

void RowExecutor::PerformKey(const ActionRow& row) {
  KeyBinding b = KeyBinding::fromDisplayText(row.shortcut);
  if (!b.isValid()) {
    return;
  }
  const auto mods = b.modifierSendCodes(KeySendProfile::WindowsVk);
  const int main_vk = windowsVkForKeyInput(b.mainSendCode(KeySendProfile::WindowsVk));

  if (row.hold || row.key_stroke == KeyStroke::Down) {
    for (int m : mods) {
      core_->KeyDown(windowsVkForKeyInput(m));
    }
    core_->KeyDown(main_vk);
    return;
  }
  if (row.key_stroke == KeyStroke::Up) {
    core_->KeyUp(main_vk);
    for (int i = mods.size() - 1; i >= 0; --i) {
      core_->KeyUp(windowsVkForKeyInput(mods[i]));
    }
    return;
  }
  // Press：完整按下再弹起
  for (int m : mods) {
    core_->KeyDown(windowsVkForKeyInput(m));
  }
  core_->KeyPress(main_vk);
  for (int i = mods.size() - 1; i >= 0; --i) {
    core_->KeyUp(windowsVkForKeyInput(mods[i]));
  }
}

void RowExecutor::PerformMouse(const ActionRow& row, int x, int y) {
  if (row.target.mode != TargetMode::None) {
    core_->MoveTo(x, y);
  }
  switch (row.button) {
    case MouseButton::Right:
      core_->RightClick();
      break;
    case MouseButton::Middle:
      core_->MiddleClick();
      break;
    case MouseButton::LeftDouble:
      core_->LeftDoubleClick();
      break;
    case MouseButton::WheelUp:
      core_->WheelUp();
      break;
    case MouseButton::WheelDown:
      core_->WheelDown();
      break;
    case MouseButton::LeftDown:
      core_->LeftDown();
      break;
    case MouseButton::LeftUp:
      core_->LeftUp();
      break;
    case MouseButton::RightDown:
      core_->RightDown();
      break;
    case MouseButton::RightUp:
      core_->RightUp();
      break;
    default:
      core_->LeftClick();
      break;
  }
}

bool RowExecutor::ExecuteRow(int index, const ActionRow& row) {
  if (!row.enabled) {
    return false;
  }
  EnsureTopmostIfNeeded();
  int x = 0;
  int y = 0;
  QString fail_reason;
  if (!ResolveTarget(row, &x, &y, &fail_reason)) {
    if (row.target.mode == TargetMode::FindPic) {
      const QString pic = QFileInfo(row.target.image_path).fileName();
      emit actionLog(QStringLiteral("#%1 识图失败：%2 — %3")
                         .arg(index + 1)
                         .arg(pic.isEmpty() ? QStringLiteral("(无图片)") : pic, fail_reason));
    } else {
      emit actionLog(QStringLiteral("#%1 定位失败：%2").arg(index + 1).arg(fail_reason));
    }
    return false;
  }

  if (row.target.mode == TargetMode::FindPic) {
    const QString pic = QFileInfo(row.target.image_path).fileName();
    emit actionLog(QStringLiteral("#%1 识图成功：%2 → (%3,%4)")
                       .arg(index + 1)
                       .arg(pic.isEmpty() ? QStringLiteral("(无图片)") : pic)
                       .arg(x)
                       .arg(y));
  }

  if (row.kind == ActionKind::Key) {
    if (row.target.mode != TargetMode::None) {
      emit actionLog(
          QStringLiteral("#%1 鼠标移动到 (%2,%3)").arg(index + 1).arg(x).arg(y));
      core_->MoveTo(x, y);
    }
    emit actionLog(QStringLiteral("#%1 %2").arg(index + 1).arg(KeyActionLabel(row)));
    PerformKey(row);
  } else {
    if (row.target.mode != TargetMode::None) {
      emit actionLog(
          QStringLiteral("#%1 鼠标移动到 (%2,%3)").arg(index + 1).arg(x).arg(y));
    }
    emit actionLog(
        QStringLiteral("#%1 鼠标 %2").arg(index + 1).arg(MouseButtonLabel(row.button)));
    PerformMouse(row, x, y);
  }
  return true;
}

void RowExecutor::EnsureTopmostIfNeeded() {
  if (!keep_topmost_ || !core_) {
    return;
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (last_topmost_ms_ > 0 && (now - last_topmost_ms_) < 500) {
    return;
  }
  last_topmost_ms_ = now;
  WindowSelectHelper::setTargetWindowTopmost(core_->BindHwnd(), true);
}

int RowExecutor::RandomInterval(int min_ms, int max_ms) const {
  if (max_ms < min_ms) {
    qSwap(min_ms, max_ms);
  }
  min_ms = qMax(1, min_ms);
  max_ms = qMax(min_ms, max_ms);
  if (min_ms == max_ms) {
    return min_ms;
  }
  return QRandomGenerator::global()->bounded(min_ms, max_ms + 1);
}
