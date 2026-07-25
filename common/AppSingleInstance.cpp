#include "AppSingleInstance.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTimer>

namespace {

QString LocalServerNameForKpn(const QString& abs_path) {
  const QByteArray key =
      QDir::toNativeSeparators(abs_path).toLower().toUtf8();
  const QByteArray hash =
      QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(24);
  return QStringLiteral("KeyPresserNext_kpn_%1").arg(QString::fromLatin1(hash));
}

bool ConnectToKpnServer(const QString& abs_path, QLocalSocket* socket, int timeout_ms) {
  if (!socket || abs_path.isEmpty()) {
    return false;
  }
  socket->connectToServer(LocalServerNameForKpn(abs_path));
  return socket->waitForConnected(timeout_ms);
}

}  // namespace

QString PendingKpnPathFromArgs(const QStringList& args) {
  for (int i = 1; i < args.size(); ++i) {
    const QString arg = args.at(i).trimmed();
    if (arg.isEmpty() || arg.startsWith(QLatin1Char('-'))) {
      continue;
    }
    if (IsKeyPresserNextProtocolUrl(arg)) {
      continue;
    }
    const QFileInfo fi(arg);
    if (!fi.exists() || !fi.isFile()) {
      continue;
    }
    const QString suf = fi.suffix().toLower();
    if (suf == QLatin1String("kpn")) {
      return NormalizeKpnPath(fi.absoluteFilePath());
    }
  }
  return {};
}

QString NormalizeKpnPath(const QString& path) {
  if (path.trimmed().isEmpty()) {
    return {};
  }
  const QFileInfo fi(path);
  const QString canonical = fi.canonicalFilePath();
  if (!canonical.isEmpty()) {
    return QDir::toNativeSeparators(canonical);
  }
  return QDir::toNativeSeparators(fi.absoluteFilePath());
}

bool ActivateExistingKpnInstance(const QString& path) {
  const QString abs = NormalizeKpnPath(path);
  if (abs.isEmpty()) {
    return false;
  }
  QLocalSocket socket;
  if (!ConnectToKpnServer(abs, &socket, 300)) {
    return false;
  }
  socket.write("raise\n");
  socket.flush();
  socket.waitForBytesWritten(300);
  socket.waitForReadyRead(300);
  socket.disconnectFromServer();
  return true;
}

bool IsKpnOpenInAnotherInstance(const QString& path) {
  const QString abs = NormalizeKpnPath(path);
  if (abs.isEmpty()) {
    return false;
  }
  QLocalSocket socket;
  if (!ConnectToKpnServer(abs, &socket, 150)) {
    return false;
  }
  socket.disconnectFromServer();
  return true;
}

bool StartNewProcessForKpn(const QString& path) {
  const QString abs = NormalizeKpnPath(path);
  if (abs.isEmpty()) {
    return false;
  }
  const QString exe = QCoreApplication::applicationFilePath();
  if (exe.isEmpty()) {
    return false;
  }
  return QProcess::startDetached(exe, QStringList{abs});
}

KpnOpenConflictResult PromptKpnAlreadyOpen(QWidget* parent, const QString& path) {
  const QString name = QFileInfo(NormalizeKpnPath(path)).fileName();
  QMessageBox box(parent);
  box.setIcon(QMessageBox::Question);
  box.setWindowTitle(QStringLiteral("文件已打开"));
  box.setText(QStringLiteral("「%1」已在其它窗口打开。").arg(name));
  box.setInformativeText(
      QStringLiteral("切换到已有窗口可避免重复编辑；再开一份则两个窗口使用同一配置"
                     "（注意保存时可能互相覆盖）。"));
  auto* switch_btn =
      static_cast<QAbstractButton*>(box.addButton(QStringLiteral("切换到已有窗口"), QMessageBox::AcceptRole));
  auto* another_btn =
      static_cast<QAbstractButton*>(box.addButton(QStringLiteral("再开一份"), QMessageBox::ActionRole));
  box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(qobject_cast<QPushButton*>(switch_btn));
  box.exec();
  const QAbstractButton* clicked = box.clickedButton();
  if (clicked == switch_btn) {
    return KpnOpenConflictResult::SwitchToExisting;
  }
  if (clicked == another_btn) {
    return KpnOpenConflictResult::OpenAnotherCopy;
  }
  return KpnOpenConflictResult::Cancel;
}

KpnInstanceHost::KpnInstanceHost(QObject* parent) : QObject(parent) {}

KpnInstanceHost::~KpnInstanceHost() {
  clear();
}

void KpnInstanceHost::clear() {
  if (server_) {
    const QString name = server_->serverName();
    server_->close();
    server_->deleteLater();
    server_ = nullptr;
    if (!name.isEmpty()) {
      QLocalServer::removeServer(name);
    }
  }
  path_.clear();
}

bool KpnInstanceHost::listenFor(const QString& path) {
  const QString abs = NormalizeKpnPath(path);
  if (abs.isEmpty()) {
    clear();
    return false;
  }
  if (abs == path_ && server_ && server_->isListening()) {
    return true;
  }
  clear();
  path_ = abs;
  const QString name = LocalServerNameForKpn(path_);
  server_ = new QLocalServer(this);
  QObject::connect(server_, &QLocalServer::newConnection, this,
                   &KpnInstanceHost::OnNewConnection);
  if (server_->listen(name)) {
    return true;
  }
  // 可能是崩溃残留：确认无人占用后再清理重试
  if (!IsKpnOpenInAnotherInstance(abs)) {
    QLocalServer::removeServer(name);
    if (server_->listen(name)) {
      return true;
    }
  }
  // 已被其它进程占用：本窗口仍可打开该文件，只是不登记为置前目标
  server_->deleteLater();
  server_ = nullptr;
  path_.clear();
  return false;
}

void KpnInstanceHost::OnNewConnection() {
  if (!server_) {
    return;
  }
  QLocalSocket* client = server_->nextPendingConnection();
  if (!client) {
    return;
  }
  QObject::connect(client, &QLocalSocket::readyRead, this, [this, client]() {
    const QByteArray data = client->readAll();
    if (data.contains("raise")) {
      if (activation_handler_) {
        activation_handler_();
      }
    }
    client->write("ok\n");
    client->flush();
    client->disconnectFromServer();
  });
  // 探测连接（IsKpnOpen）可能不发数据：短暂后关掉
  QTimer::singleShot(200, client, [client]() {
    if (client->state() == QLocalSocket::ConnectedState) {
      client->disconnectFromServer();
    }
  });
  QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
}

namespace {

constexpr const char kAppFocusServerName[] = "KeyPresserNext_app_focus";

}  // namespace

bool IsKeyPresserNextProtocolUrl(const QString& arg) {
  return arg.startsWith(QStringLiteral("keypressernext:"), Qt::CaseInsensitive);
}

QString KeyPresserNextProtocolUrlFromArgs(const QStringList& args) {
  for (int i = 1; i < args.size(); ++i) {
    const QString arg = args.at(i).trimmed();
    if (IsKeyPresserNextProtocolUrl(arg)) {
      return arg;
    }
  }
  return {};
}

bool ActivateRunningKeyPresserNext() {
  QLocalSocket socket;
  socket.connectToServer(QString::fromLatin1(kAppFocusServerName));
  if (!socket.waitForConnected(400)) {
    return false;
  }
  socket.write("raise\n");
  socket.flush();
  socket.waitForBytesWritten(300);
  socket.waitForReadyRead(300);
  socket.disconnectFromServer();
  return true;
}

AppFocusHost::AppFocusHost(QObject* parent) : QObject(parent) {}

AppFocusHost::~AppFocusHost() {
  stop();
}

void AppFocusHost::stop() {
  if (server_) {
    const QString name = server_->serverName();
    server_->close();
    server_->deleteLater();
    server_ = nullptr;
    if (!name.isEmpty()) {
      QLocalServer::removeServer(name);
    }
  }
}

bool AppFocusHost::start() {
  if (server_ && server_->isListening()) {
    return true;
  }
  stop();
  const QString name = QString::fromLatin1(kAppFocusServerName);
  server_ = new QLocalServer(this);
  QObject::connect(server_, &QLocalServer::newConnection, this,
                   &AppFocusHost::OnNewConnection);
  if (server_->listen(name)) {
    return true;
  }
  // 崩溃残留：确认无人占用后再清理重试
  {
    QLocalSocket probe;
    probe.connectToServer(name);
    const bool busy = probe.waitForConnected(120);
    if (busy) {
      probe.disconnectFromServer();
      server_->deleteLater();
      server_ = nullptr;
      return false;
    }
  }
  QLocalServer::removeServer(name);
  if (server_->listen(name)) {
    return true;
  }
  server_->deleteLater();
  server_ = nullptr;
  return false;
}

void AppFocusHost::OnNewConnection() {
  if (!server_) {
    return;
  }
  QLocalSocket* client = server_->nextPendingConnection();
  if (!client) {
    return;
  }
  QObject::connect(client, &QLocalSocket::readyRead, this, [this, client]() {
    const QByteArray data = client->readAll();
    if (data.contains("raise")) {
      if (activation_handler_) {
        activation_handler_();
      }
    }
    client->write("ok\n");
    client->flush();
    client->disconnectFromServer();
  });
  QTimer::singleShot(200, client, [client]() {
    if (client->state() == QLocalSocket::ConnectedState) {
      client->disconnectFromServer();
    }
  });
  QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
}
