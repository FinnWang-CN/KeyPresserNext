#include "common/AppSingleInstance.h"
#include "common/FileAssociation.h"
#include "engine/Theme.h"
#include "ui/KeyPresserNextWindow.h"
#include "appversion.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace {

bool LoadTranslator(QApplication* app, const QString& resource_path,
                    const QString& file_prefix) {
  auto* tr = new QTranslator(app);
  const QString qt_trans = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
  if (tr->load(resource_path) ||
      tr->load(QLocale(QLocale::Chinese, QLocale::China), file_prefix,
               QStringLiteral("_"), qt_trans)) {
    app->installTranslator(tr);
    return true;
  }
  delete tr;
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  // Match old KeyPresser DPI strategy for coordinate consistency with engine bind.
  QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  // Required by QWindowKit frameless title bar.
  QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

  QApplication app(argc, argv);
  QApplication::setOrganizationName(QStringLiteral("FinnSoft"));
  QApplication::setApplicationName(QStringLiteral("KeyPresserNextCommunity"));
  QApplication::setApplicationDisplayName(AppProductName());

  QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
  // 按钮文案在 qtbase（QPlatformTheme）；qt_zh_CN  alone 不够
  LoadTranslator(&app, QStringLiteral(":/i18n/qtbase_zh_CN.qm"),
                 QStringLiteral("qtbase"));
  LoadTranslator(&app, QStringLiteral(":/i18n/qt_zh_CN.qm"), QStringLiteral("qt"));

  Theme::ApplyDarkTheme(&app);

  EnsureKpnFileAssociation();
  EnsureCustomUrlProtocol();

  // 网页 keypressernext:// 唤起：若已有实例则置前并退出
  if (!KeyPresserNextProtocolUrlFromArgs(app.arguments()).isEmpty()) {
    if (ActivateRunningKeyPresserNext()) {
      return 0;
    }
  }

  const QString open_path = PendingKpnPathFromArgs(app.arguments());
  bool force_open_duplicate = false;
  if (!open_path.isEmpty() && IsKpnOpenInAnotherInstance(open_path)) {
    switch (PromptKpnAlreadyOpen(nullptr, open_path)) {
      case KpnOpenConflictResult::SwitchToExisting:
        ActivateExistingKpnInstance(open_path);
        return 0;
      case KpnOpenConflictResult::Cancel:
        return 0;
      case KpnOpenConflictResult::OpenAnotherCopy:
        force_open_duplicate = true;
        break;
    }
  }

  KeyPresserNextWindow window(open_path, force_open_duplicate);
  window.show();
  return app.exec();
}
