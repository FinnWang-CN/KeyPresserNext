#include "Changelog.h"

#include "LayoutMetrics.h"
#include "../appversion.h"
#include "../model/SettingsStore.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace Changelog {
namespace {

QString FindChangelogFile() {
  const QStringList candidates = {
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("docs/更新日志.md")),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../docs/更新日志.md")),
      QDir(QCoreApplication::applicationDirPath())
          .filePath(QStringLiteral("../../docs/更新日志.md")),
  };
  for (const QString& path : candidates) {
    if (QFileInfo::exists(path)) {
      return path;
    }
  }
  return QString();
}

QString LoadFromPath(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }
  return QString::fromUtf8(f.readAll());
}

}  // namespace

QString LoadMarkdown() {
  const QString path = FindChangelogFile();
  if (!path.isEmpty()) {
    const QString text = LoadFromPath(path);
    if (!text.isEmpty()) {
      return text;
    }
  }
  QFile res(QStringLiteral(":/docs/changelog.md"));
  if (res.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString::fromUtf8(res.readAll());
  }
  return QString();
}

void ShowDialog(QWidget* parent, bool force) {
  const QString md = LoadMarkdown();
  if (md.isEmpty()) {
    if (force) {
      QDialog dlg(parent);
      dlg.setWindowTitle(QStringLiteral("更新日志"));
      auto* lay = new QVBoxLayout(&dlg);
      lay->addWidget(new QLabel(QStringLiteral("未找到更新日志文件。"), &dlg));
      auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
      QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
      lay->addWidget(buttons);
      dlg.exec();
    }
    return;
  }

  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("更新日志 — %1 %2").arg(AppProductName(), AppVersion()));
  dlg.resize(560, 480);
  dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);

  auto* root = new QVBoxLayout(&dlg);
  root->setContentsMargins(14, 14, 14, 12);
  root->setSpacing(10);

  auto* tip = new QLabel(QStringLiteral("最近版本的改动如下（完整历史可向下滚动查看）。"), &dlg);
  tip->setWordWrap(true);
  tip->setStyleSheet(QStringLiteral("color: #9aa3ad;"));
  root->addWidget(tip);

  auto* browser = new QTextBrowser(&dlg);
  browser->setOpenExternalLinks(true);
  browser->setMarkdown(md);
  browser->setFrameShape(QFrame::NoFrame);
  root->addWidget(browser, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("知道了"));
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  root->addWidget(buttons);

  dlg.exec();
}

void MaybeShowAfterUpgrade(QWidget* parent) {
  AppSession session = SettingsStore::LoadSession();
  const QString current = AppVersion();
  if (session.last_changelog_version != current) {
    ShowDialog(parent, true);
    session.last_changelog_version = current;
    SettingsStore::SaveSession(session);
  }
  LayoutMetrics::SettleDeferred(parent);
}

}  // namespace Changelog
