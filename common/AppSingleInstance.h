#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QLocalServer;
class QWidget;

// 从命令行参数解析待打开的 .kpn 路径
QString PendingKpnPathFromArgs(const QStringList& args);

// 规范化为绝对路径（用于跨进程比较）
QString NormalizeKpnPath(const QString& path);

// 若已有进程打开该 .kpn，则激活其窗口并返回 true（调用方应退出）
bool ActivateExistingKpnInstance(const QString& path);

// 是否已有其它进程打开该 .kpn（不激活）
bool IsKpnOpenInAnotherInstance(const QString& path);

// 新起进程打开指定 .kpn
bool StartNewProcessForKpn(const QString& path);

/** 同一 .kpn 已被其它进程打开时的用户选择 */
enum class KpnOpenConflictResult {
  SwitchToExisting,  // 切换到已有窗口
  OpenAnotherCopy,   // 再开一份（当前/新进程继续加载）
  Cancel
};

KpnOpenConflictResult PromptKpnAlreadyOpen(QWidget* parent, const QString& path);

/**
 * 当前进程登记「正在打开的 .kpn」：其它进程可连接并请求置前。
 * 路径变化时调用 listenFor / clear。
 * 若该文件已被其它进程登记，listenFor 失败（仍可本地打开，只是不再作为置前目标）。
 */
class KpnInstanceHost : public QObject {
 public:
  explicit KpnInstanceHost(QObject* parent = nullptr);
  ~KpnInstanceHost() override;

  bool listenFor(const QString& path);
  void clear();
  QString currentPath() const { return path_; }

  void setActivationHandler(std::function<void()> handler) {
    activation_handler_ = std::move(handler);
  }

 private:
  void OnNewConnection();

  QLocalServer* server_ = nullptr;
  QString path_;
  std::function<void()> activation_handler_;
};

/** 是否为 keypressernext://… 自定义协议参数 */
bool IsKeyPresserNextProtocolUrl(const QString& arg);

/** 从命令行取出协议 URL（若有） */
QString KeyPresserNextProtocolUrlFromArgs(const QStringList& args);

/** 激活已在运行的 KeyPresserNext 主窗口；成功则返回 true（调用方应退出） */
bool ActivateRunningKeyPresserNext();

/**
 * 进程级置前通道：供 keypressernext:// 第二次启动时唤起已有窗口。
 * 多开时仅第一个成功 listen 的进程响应。
 */
class AppFocusHost : public QObject {
 public:
  explicit AppFocusHost(QObject* parent = nullptr);
  ~AppFocusHost() override;

  bool start();
  void stop();

  void setActivationHandler(std::function<void()> handler) {
    activation_handler_ = std::move(handler);
  }

 private:
  void OnNewConnection();

  QLocalServer* server_ = nullptr;
  std::function<void()> activation_handler_;
};
