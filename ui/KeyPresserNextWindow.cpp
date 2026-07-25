#include "KeyPresserNextWindow.h"

#include "ActionRowWidget.h"
#include "BulkAddRowsDialog.h"
#include "HotkeySettingsDialog.h"
#include "../common/AppSingleInstance.h"
#include "../common/DarkWindowChrome.h"
#include "../appversion.h"
#include "../common/KeyBinding.h"
#include "../common/KeyCaptureEdit.h"
#include "../common/GlobalHotkeys.h"
#include "../common/StopHotkeyHint.h"
#include "../common/WindowCoordHelper.h"
#include "../common/WindowSelectHelper.h"
#include "../common/Changelog.h"
#include "../common/LayoutMetrics.h"
#include "../common/screenshotwidget.h"
#include "../model/SettingsStore.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QThread>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QStatusBar>
#include <QTimer>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QFontMetrics>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QPixmap>
#include <QMetaObject>
#include <QProxyStyle>
#include <QPushButton>
#include <QScreen>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSettings>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStatusBar>
#include <QStyle>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

KeyPresserNextWindow* KeyPresserNextWindow::record_instance_ = nullptr;
KeyPresserNextWindow* KeyPresserNextWindow::select_instance_ = nullptr;
KeyPresserNextWindow* KeyPresserNextWindow::pick_instance_ = nullptr;

namespace {

// Qt5 QMenu 无 setIconSize，通过样式放大菜单小图标
class MenuIconStyle : public QProxyStyle {
 public:
  explicit MenuIconStyle(QStyle* base) : QProxyStyle(base) {}

  int pixelMetric(PixelMetric metric, const QStyleOption* option,
                  const QWidget* widget) const override {
    if (metric == QStyle::PM_SmallIconSize) {
      return 16;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
  }
};

void PrepMenuIcons(QMenu* menu) {
  if (!menu) {
    return;
  }
  auto* style = new MenuIconStyle(menu->style());
  style->setParent(menu);
  menu->setStyle(style);
}

/** 动作行与下方区域之间的分割条：居中细线，上下留白一致 */
class MainSplitHandle : public QSplitterHandle {
 public:
  MainSplitHandle(Qt::Orientation orientation, QSplitter* parent)
      : QSplitterHandle(orientation, parent) {}

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), QColor(0x20, 0x20, 0x20));
    // 与界面描边接近的浅灰分隔线，避免纯黑过冲
    const int y = height() / 2;
    p.setPen(QPen(QColor(0x48, 0x48, 0x48), 1));
    p.drawLine(0, y, width(), y);
  }
};

class MainSplitter : public QSplitter {
 public:
  explicit MainSplitter(QWidget* parent = nullptr) : QSplitter(Qt::Vertical, parent) {}

 protected:
  QSplitterHandle* createHandle() override {
    return new MainSplitHandle(orientation(), this);
  }
};

namespace {

// 选窗 / 取点期间强制十字光标（覆盖其它窗口上的箭头/工字光标等），结束后还原
int g_cross_cursor_depth = 0;
HCURSOR g_cross_template = nullptr;  // 自有副本，避免 LoadCursor 共享资源被改坏
HHOOK g_cross_cursor_hook = nullptr;

// OCR_* 数值（无需 OEMRESOURCE）
constexpr DWORD kOcrIds[] = {
    32512,  // OCR_NORMAL
    32513,  // OCR_IBEAM
    32514,  // OCR_WAIT
    32515,  // OCR_CROSS
    32516,  // OCR_UP
    32642,  // OCR_SIZENWSE
    32643,  // OCR_SIZENESW
    32644,  // OCR_SIZEWE
    32645,  // OCR_SIZENS
    32646,  // OCR_SIZEALL
    32648,  // OCR_NO
    32649,  // OCR_HAND
    32650,  // OCR_APPSTARTING
    32651,  // OCR_HELP
};

LRESULT CALLBACK CrossCursorMouseProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code >= 0 && g_cross_template) {
    SetCursor(g_cross_template);
  }
  return CallNextHookEx(g_cross_cursor_hook, code, wparam, lparam);
}

void BeginCrossCursor() {
  if (g_cross_cursor_depth++ > 0) {
    return;
  }

  QGuiApplication::setOverrideCursor(Qt::CrossCursor);

  // 先从干净的系统表取出十字，再复制成模板（此后不再 LoadCursor(IDC_CROSS)）
  const HCURSOR stock = LoadCursorW(nullptr, IDC_CROSS);
  if (!stock) {
    return;
  }
  g_cross_template = CopyCursor(stock);
  if (!g_cross_template) {
    return;
  }

  // 每种系统光标各自 Copy 一份再写入，避免句柄被 SetSystemCursor 销毁后互相串改
  for (DWORD id : kOcrIds) {
    if (HCURSOR c = CopyCursor(g_cross_template)) {
      SetSystemCursor(c, id);
    }
  }

  if (!g_cross_cursor_hook) {
    g_cross_cursor_hook =
        SetWindowsHookExW(WH_MOUSE_LL, CrossCursorMouseProc, GetModuleHandleW(nullptr), 0);
  }
}

void EndCrossCursor() {
  if (g_cross_cursor_depth <= 0) {
    return;
  }
  if (--g_cross_cursor_depth > 0) {
    return;
  }

  if (g_cross_cursor_hook) {
    UnhookWindowsHookEx(g_cross_cursor_hook);
    g_cross_cursor_hook = nullptr;
  }
  QGuiApplication::restoreOverrideCursor();
  SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
  if (g_cross_template) {
    DestroyCursor(g_cross_template);
    g_cross_template = nullptr;
  }
}

}  // namespace

void FitComboWidth(QComboBox* box) {
  if (!box || box->count() == 0) {
    return;
  }
  box->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  box->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

QString SanitizeFileName(QString name) {
  static const QString kInvalid = QStringLiteral("\\/:*?\"<>|");
  for (const QChar c : kInvalid) {
    name.replace(c, QLatin1Char('_'));
  }
  name.remove(QChar(0));
  name = name.trimmed();
  while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) {
    name.chop(1);
  }
  if (name.isEmpty()) {
    return QStringLiteral("未选择窗口");
  }
  if (name.size() > 80) {
    name = name.left(80);
  }
  return name;
}

QString CurrentWindowTitleForSave(HWND hwnd, const QLabel* window_label) {
  if (hwnd && IsWindow(hwnd)) {
    wchar_t title[512] = {};
    GetWindowTextW(hwnd, title, 511);
    const QString t = QString::fromWCharArray(title).trimmed();
    if (!t.isEmpty()) {
      return t;
    }
  }
  if (window_label) {
    const QString tip = window_label->toolTip().trimmed();
    if (!tip.isEmpty()) {
      return tip;
    }
  }
  return QStringLiteral("未选择窗口");
}

QString SuggestedKpnsSavePath(const QString& existing_path, HWND hwnd,
                              const QLabel* window_label) {
  const QString stamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
  const QString base =
      stamp + QLatin1Char('+') +
      SanitizeFileName(CurrentWindowTitleForSave(hwnd, window_label)) +
      QStringLiteral(".kpn");

  QString dir;
  if (!existing_path.isEmpty()) {
    dir = QFileInfo(existing_path).absolutePath();
  } else {
    const AppSession session = SettingsStore::LoadSession();
    if (!session.last_path.isEmpty()) {
      dir = QFileInfo(session.last_path).absolutePath();
    } else {
      dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
  }
  if (dir.isEmpty()) {
    return base;
  }
  return QDir(dir).filePath(base);
}

// 从「时间戳+窗口名.kpn」取出窗口名；不符合约定则返回空
QString WindowNameFromKpnsFileName(const QString& path) {
  const QString base = QFileInfo(path).completeBaseName();
  const int plus = base.indexOf(QLatin1Char('+'));
  if (plus < 0 || plus + 1 >= base.size()) {
    return QString();
  }
  return base.mid(plus + 1).trimmed();
}

bool WindowNameMismatchesKpnsFile(const QString& path, HWND hwnd, const QLabel* window_label,
                                  QString* out_file_name, QString* out_current_name) {
  const QString from_file = WindowNameFromKpnsFileName(path);
  if (from_file.isEmpty()) {
    return false;
  }
  const QString current = SanitizeFileName(CurrentWindowTitleForSave(hwnd, window_label));
  if (out_file_name) {
    *out_file_name = from_file;
  }
  if (out_current_name) {
    *out_current_name = current;
  }
  return QString::compare(from_file, current, Qt::CaseInsensitive) != 0;
}

QString ModeLabel(const QString& value) { return value; }

QString DisplayModeTip(const QString& value) {
  if (value == QLatin1String("normal")) {
    return QStringLiteral("正常截图：前台截屏，窗口需可见。");
  }
  if (value == QLatin1String("normal.auto")) {
    return QStringLiteral("推荐：自动选择可用截图方式，兼容性较好。");
  }
  if (value == QLatin1String("normal.dxgi")) {
    return QStringLiteral("DXGI 桌面复制截图，适合部分全屏/游戏场景。");
  }
  if (value == QLatin1String("normal.wgc")) {
    return QStringLiteral("Windows Graphics Capture 截图（Win10+）。");
  }
  if (value == QLatin1String("gdi")) {
    return QStringLiteral("GDI 截图：适合 GDI 刷新窗口；CPU 占用较高。");
  }
  if (value == QLatin1String("gdi2")) {
    return QStringLiteral("GDI2：兼容性更强，速度比 gdi 慢；gdi 后台不刷新时可试。");
  }
  if (value == QLatin1String("dx")) {
    return QStringLiteral("DX 图色：2D/3D 窗口；使用时建议关闭全屏抗锯齿。");
  }
  if (value == QLatin1String("dx2")) {
    return QStringLiteral("DX2：dx 易崩溃时可试；部分系统需窗口露出屏幕外。");
  }
  if (value.startsWith(QLatin1String("dx.d3d"))) {
    return QStringLiteral("按指定 Direct3D 版本截图（%1）。").arg(value);
  }
  if (value == QLatin1String("opengl")) {
    return QStringLiteral("OpenGL 截图，适合 OpenGL 渲染的窗口。");
  }
  return QStringLiteral("截图获取方式：%1").arg(value);
}

QString MouseModeTip(const QString& value) {
  if (value == QLatin1String("normal")) {
    return QStringLiteral("正常模式：前台真实鼠标移动/点击。");
  }
  if (value == QLatin1String("windows")) {
    return QStringLiteral("Windows 模式：用窗口消息模拟鼠标（常用后台）。");
  }
  if (value == QLatin1String("dx")) {
    return QStringLiteral("DX 模式：模拟 DX 鼠标；windows 无效时可试。");
  }
  return QStringLiteral("鼠标仿真：%1").arg(value);
}

QString KeypadModeTip(const QString& value) {
  if (value == QLatin1String("normal")) {
    return QStringLiteral("正常模式：前台真实键盘输入。");
  }
  if (value == QLatin1String("normal.hd")) {
    return QStringLiteral("正常高延迟兼容模式：部分对输入敏感的程序可试。");
  }
  if (value == QLatin1String("windows")) {
    return QStringLiteral("Windows 模式：用窗口消息模拟按键（常用后台）。");
  }
  if (value == QLatin1String("dx")) {
    return QStringLiteral("DX 模式：模拟 DX 键盘；windows 无效时可试。");
  }
  return QStringLiteral("键盘仿真：%1").arg(value);
}

QString BindModeTip(int mode) {
  if (mode == 0) {
    return QStringLiteral("mode 0：常规绑定（推荐）。");
  }
  if (mode == 1) {
    return QStringLiteral("mode 1：增强绑定；mode 0 无效或异常时可试。");
  }
  return QStringLiteral("绑定模式 %1").arg(mode);
}

void BindComboTip(QComboBox* combo, const std::function<QString(const QVariant&)>& tip_for_data) {
  auto refresh = [combo, tip_for_data]() {
    combo->setToolTip(tip_for_data(combo->currentData()));
  };
  for (int i = 0; i < combo->count(); ++i) {
    combo->setItemData(i, tip_for_data(combo->itemData(i)), Qt::ToolTipRole);
  }
  QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), combo, refresh);
  refresh();
}

}  // namespace

KeyPresserNextWindow::KeyPresserNextWindow(const QString& open_path, bool force_open_duplicate,
                                           QWidget* parent)
    : QMainWindow(parent) {
  setWindowTitle(AppProductName());
  setWindowIcon(QIcon(QStringLiteral(":/icons/keypresser.ico")));
  resize(650, 820);

  if (!core_.Initialize()) {
    QMessageBox::critical(this, AppProductName(),
                          QStringLiteral("无法加载 NextCore.dll\n%1").arg(core_.LastError()));
  }

  executor_thread_ = new QThread(this);
  executor_ = new RowExecutor(&core_);  // 无 parent，归属执行线程
  executor_->moveToThread(executor_thread_);
  connect(executor_thread_, &QThread::finished, executor_, &QObject::deleteLater);
  connect(executor_, &RowExecutor::runningChanged, this, &KeyPresserNextWindow::OnRunningChanged,
          Qt::QueuedConnection);
  connect(executor_, &RowExecutor::statusMessage, this, [this](const QString& m) {
    if (status_label_) {
      status_label_->setText(m);
    }
    NotifyStatus(m, 3000);
  }, Qt::QueuedConnection);
  connect(executor_, &RowExecutor::actionLog, this, &KeyPresserNextWindow::AppendLog,
          Qt::QueuedConnection);
  executor_thread_->start();

  log_flush_timer_ = new QTimer(this);
  log_flush_timer_->setSingleShot(true);
  log_flush_timer_->setInterval(150);
  connect(log_flush_timer_, &QTimer::timeout, this, &KeyPresserNextWindow::FlushPendingLogs);

  kpn_host_ = new KpnInstanceHost(this);
  kpn_host_->setActivationHandler([this]() { RaiseMainWindow(); });

  app_focus_host_ = new AppFocusHost(this);
  app_focus_host_->setActivationHandler([this]() { RaiseMainWindow(); });
  app_focus_host_->start();

  hotkeys_ = new GlobalHotkeys(this);
  hotkeys_->Install();
  connect(hotkeys_, &GlobalHotkeys::startPressed, this, &KeyPresserNextWindow::OnHotkeyStart);
  connect(hotkeys_, &GlobalHotkeys::stopPressed, this, &KeyPresserNextWindow::OnHotkeyStop);
  connect(hotkeys_, &GlobalHotkeys::recordPressed, this, &KeyPresserNextWindow::OnHotkeyRecord);
  connect(hotkeys_, &GlobalHotkeys::selectWindowPressed, this, &KeyPresserNextWindow::OnSelectWindow);
  connect(hotkeys_, &GlobalHotkeys::highlightWindowPressed, this,
          &KeyPresserNextWindow::OnHighlightWindow);
  connect(hotkeys_, &GlobalHotkeys::addKeyPressed, this, &KeyPresserNextWindow::OnAddKeyRow);
  connect(hotkeys_, &GlobalHotkeys::addMousePressed, this, &KeyPresserNextWindow::OnAddMouseRow);
  connect(hotkeys_, &GlobalHotkeys::clearPressed, this, &KeyPresserNextWindow::OnClearRows);
  connect(hotkeys_, &GlobalHotkeys::undoPressed, this, &KeyPresserNextWindow::OnUndoRows);
  connect(hotkeys_, &GlobalHotkeys::redoPressed, this, &KeyPresserNextWindow::OnRedoRows);

  hotkey_dialog_ = new HotkeySettingsDialog(this);
  connect(hotkey_dialog_, &HotkeySettingsDialog::hotkeysChanged, this,
          &KeyPresserNextWindow::OnHotkeysChanged);

  BuildMenus();
  {
    DarkWindowChrome::Options chrome;
    chrome.minimize = true;
    chrome.maximize = true;
    chrome.close = true;
    chrome.menu_bar = menuBar();
    DarkWindowChrome::Install(this, chrome);
  }
  BuildUi();
  PopulateModeCombos();

  const AppSession session = SettingsStore::LoadSession();
  if (!open_path.isEmpty() && QFileInfo::exists(open_path)) {
    OpenFromPath(open_path, force_open_duplicate);
  } else if (!session.last_path.isEmpty() && QFileInfo::exists(session.last_path) &&
             !IsKpnOpenInAnotherInstance(session.last_path)) {
    OpenFromPath(session.last_path);
  } else {
    SetCurrentFile(QString(), false);
    ResetRowsHistory();
  }
  RebuildRecentMenu();

  NotifyStatus(
      QStringLiteral("版本 %1  |  提示：悬停查看说明").arg(AppVersion()),
      10000);
  // 新版本首次启动弹出更新日志（稍晚于窗口显示，避免抢焦点）
  QTimer::singleShot(500, this, [this]() { Changelog::MaybeShowAfterUpgrade(this); });
  LayoutMetrics::SettleDeferred(this);
}

KeyPresserNextWindow::~KeyPresserNextWindow() {
  CancelSelectWindowHook();
  CancelPickPointHook();
  UninstallRecordHooks();
  if (executor_) {
    QMetaObject::invokeMethod(executor_, "Stop", Qt::BlockingQueuedConnection);
  }
  if (executor_thread_) {
    executor_thread_->quit();
    executor_thread_->wait(3000);
    executor_thread_ = nullptr;
    executor_ = nullptr;
  }
  core_.UnBindWindow();
  if (select_instance_ == this) {
    select_instance_ = nullptr;
  }
  if (pick_instance_ == this) {
    pick_instance_ = nullptr;
  }
}

void KeyPresserNextWindow::BuildMenus() {
  // 菜单标题用 (&X)：Win 上显示为「文件(F)」，Alt+字母打开/激活
  auto* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
  PrepMenuIcons(file_menu);

  auto* new_act = file_menu->addAction(QIcon(QStringLiteral(":/png/new.png")),
                                       QStringLiteral("新建(&N)"), this,
                                       &KeyPresserNextWindow::OnNewKpns);
  new_act->setShortcut(QKeySequence::New);
  new_act->setShortcutContext(Qt::WindowShortcut);

  auto* open_act = file_menu->addAction(QIcon(QStringLiteral(":/png/open.png")),
                                        QStringLiteral("打开(&O)…"), this,
                                        &KeyPresserNextWindow::OnOpenKpns);
  open_act->setShortcut(QKeySequence::Open);
  open_act->setShortcutContext(Qt::WindowShortcut);

  auto* save_act = file_menu->addAction(QIcon(QStringLiteral(":/png/save.png")),
                                        QStringLiteral("保存(&S)"), this,
                                        &KeyPresserNextWindow::OnSaveKpns);
  save_act->setShortcut(QKeySequence::Save);
  save_act->setShortcutContext(Qt::WindowShortcut);

  auto* save_as_act = file_menu->addAction(QIcon(QStringLiteral(":/png/save_as.png")),
                                           QStringLiteral("另存为(&A)…"), this,
                                           &KeyPresserNextWindow::OnSaveKpnsAs);
  // 显式指定：部分平台上 QKeySequence::SaveAs 为空，菜单不会显示快捷键
  save_as_act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
  save_as_act->setShortcutContext(Qt::WindowShortcut);

  file_menu->addSeparator();
  recent_menu_ = file_menu->addMenu(QStringLiteral("最近打开(&R)"));
  RebuildRecentMenu();
  file_menu->addSeparator();
  auto* quit_act = file_menu->addAction(QStringLiteral("退出(&X)"), this, &QWidget::close);
  quit_act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
  quit_act->setShortcutContext(Qt::WindowShortcut);

  auto* settings_menu = menuBar()->addMenu(QStringLiteral("设置(&S)"));
  PrepMenuIcons(settings_menu);
  settings_menu->addAction(QIcon(QStringLiteral(":/png/timing.png")), QStringLiteral("快捷键(&K)…"),
                           this, &KeyPresserNextWindow::OnOpenHotkeySettings);

  auto* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
  PrepMenuIcons(help_menu);
  help_menu->addAction(QIcon(QStringLiteral(":/png/changelog.png")), QStringLiteral("更新日志(&C)…"), this,
                       &KeyPresserNextWindow::OnShowChangelog);
  help_menu->addAction(QIcon(QStringLiteral(":/png/update.png")), QStringLiteral("访问官网(&W)…"), this,
                       &KeyPresserNextWindow::OnOpenOfficialSite);
  help_menu->addSeparator();
  help_menu->addAction(QIcon(QStringLiteral(":/png/about.png")), QStringLiteral("关于(&B)"), this,
                       &KeyPresserNextWindow::OnAbout);
}

void KeyPresserNextWindow::RebuildRecentMenu() {
  if (!recent_menu_) {
    return;
  }
  recent_menu_->clear();
  const AppSession session = SettingsStore::LoadSession();
  if (session.recent_paths.isEmpty()) {
    auto* empty = recent_menu_->addAction(QStringLiteral("（无最近文件）"));
    empty->setEnabled(false);
    return;
  }
  for (const QString& path : session.recent_paths) {
    auto* act = recent_menu_->addAction(QFileInfo(path).fileName());
    act->setToolTip(path);
    act->setData(path);
    connect(act, &QAction::triggered, this, [this, path]() {
      if (!PromptSaveBeforeClose()) {
        return;
      }
      OpenFromPath(path);
    });
  }
}

void KeyPresserNextWindow::BuildUi() {
  auto* central = new QWidget(this);
  setCentralWidget(central);
  auto* root = new QVBoxLayout(central);

  auto* bind_box = new QGroupBox(QStringLiteral("窗口与仿真"), this);
  bind_box->setObjectName(QStringLiteral("bindBox"));
  auto* bind_root = new QVBoxLayout(bind_box);
  bind_root->setContentsMargins(6, 8, 6, 6);
  bind_root->setSpacing(6);

  select_btn_ = new QPushButton(bind_box);
  highlight_btn_ = new QPushButton(QStringLiteral("凸显所选窗口"), bind_box);
  highlight_btn_->setToolTip(QStringLiteral("用红色边框闪烁提示当前已选目标窗口。"));
  window_label_ = new QLabel(QStringLiteral("未选择窗口"), bind_box);
  window_label_->setObjectName(QStringLiteral("windowValue"));
  window_label_->setProperty("bound", false);
  // Ignored：长标题不撑开整窗；由布局分配剩余宽度后再省略显示
  window_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  window_label_->setMinimumWidth(80);
  window_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  display_combo_ = new QComboBox(bind_box);
  mouse_combo_ = new QComboBox(bind_box);
  keypad_combo_ = new QComboBox(bind_box);
  bind_mode_combo_ = new QComboBox(bind_box);
  bind_mode_combo_->addItem(QStringLiteral("mode 0"), 0);
  bind_mode_combo_->addItem(QStringLiteral("mode 1"), 1);
  topmost_check_ = new QCheckBox(QStringLiteral("置顶"), bind_box);
  run_mode_combo_ = new QComboBox(bind_box);
  run_mode_combo_->addItem(QStringLiteral("顺序运行"), QStringLiteral("sequential"));
  run_mode_combo_->addItem(QStringLiteral("独立运行"), QStringLiteral("independent"));
  run_mode_combo_->setCurrentIndex(0);  // 默认顺序运行

  FitComboWidth(bind_mode_combo_);
  FitComboWidth(run_mode_combo_);

  bind_mode_combo_->setToolTip(QStringLiteral("窗口绑定模式；一般用 mode 0。"));
  run_mode_combo_->setToolTip(
      QStringLiteral("顺序：按列表轮流，先执行再等间隔。\n"
                     "独立：各行并行，先各执行一遍再等各自间隔。\n"
                     "快捷键请到「设置 → 快捷键」。"));
  topmost_check_->setToolTip(
      QStringLiteral("保持选择窗口激活置顶。\n"
                     "部分应用不支持后台键鼠，需勾选此项：会恢复最小化窗口并置顶；\n"
                     "运行、测试截图/鼠标/键盘、取点时也会尽量保持目标窗在前台。"));

  // 上行：目标窗口
  auto* win_row = new QHBoxLayout();
  win_row->setSpacing(8);
  win_row->addWidget(select_btn_);
  win_row->addWidget(window_label_, 1);
  win_row->addWidget(highlight_btn_);
  win_row->addWidget(topmost_check_);
  bind_root->addLayout(win_row);

  auto* bind_sep = new QFrame(bind_box);
  bind_sep->setObjectName(QStringLiteral("bindSep"));
  bind_sep->setFrameShape(QFrame::HLine);
  bind_sep->setFixedHeight(1);
  bind_root->addWidget(bind_sep);

  // 下行：仿真模式 + 运行前测试（同宽对齐）
  auto* modes_stack = new QWidget(bind_box);
  modes_stack->setObjectName(QStringLiteral("modesStack"));
  modes_stack->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  auto* modes_col = new QVBoxLayout(modes_stack);
  modes_col->setContentsMargins(0, 0, 0, 0);
  modes_col->setSpacing(6);

  auto* mode_box = new QGroupBox(QStringLiteral("仿真与运行模式"), modes_stack);
  mode_box->setObjectName(QStringLiteral("modeBox"));
  mode_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  mode_box->setToolTip(
      QStringLiteral("截图 / 鼠标 / 键盘 / 绑定决定如何注入目标窗口；\n"
                     "运行模式决定动作行如何执行。可先改模式，再用下方测试验证。"));
  auto* mode_box_layout = new QVBoxLayout(mode_box);
  mode_box_layout->setContentsMargins(4, 8, 4, 4);
  mode_box_layout->setSpacing(2);

  auto* modes_row1 = new QHBoxLayout();
  auto* modes_row2 = new QHBoxLayout();
  modes_row1->setSpacing(10);
  modes_row2->setSpacing(10);
  auto add_field = [&](QHBoxLayout* row, const QString& name, QWidget* editor,
                       const QString& tip) {
    auto* field = new QWidget(mode_box);
    field->setObjectName(QStringLiteral("bindField"));
    field->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    auto* fl = new QHBoxLayout(field);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setSpacing(5);
    auto* lab = new QLabel(name, field);
    lab->setObjectName(QStringLiteral("fieldLabel"));
    lab->setToolTip(tip);
    if (editor) {
      editor->setToolTip(tip);
      editor->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    fl->addWidget(lab);
    fl->addWidget(editor);
    row->addWidget(field, 0, Qt::AlignLeft);
  };
  add_field(modes_row1, QStringLiteral("截图"), display_combo_,
            QStringLiteral("窗口画面截取方式，影响识图/截图是否成功。"));
  add_field(modes_row1, QStringLiteral("鼠标"), mouse_combo_,
            QStringLiteral("鼠标动作如何注入到目标窗口。"));
  add_field(modes_row1, QStringLiteral("键盘"), keypad_combo_,
            QStringLiteral("键盘按键如何注入到目标窗口。"));
  add_field(modes_row2, QStringLiteral("绑定"), bind_mode_combo_,
            bind_mode_combo_->toolTip());
  add_field(modes_row2, QStringLiteral("运行"), run_mode_combo_,
            run_mode_combo_->toolTip());
  modes_row1->addStretch(1);
  modes_row2->addStretch(1);
  mode_box_layout->addLayout(modes_row1);
  mode_box_layout->addLayout(modes_row2);

  auto* test_box = new QGroupBox(QStringLiteral("运行前测试，选择合适模式"), modes_stack);
  test_box->setObjectName(QStringLiteral("testBox"));
  test_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  test_box->setToolTip(
      QStringLiteral("先用下方测试确认截图、鼠标、键盘是否生效，再开始运行。\n"
                     "若无响应，可切换上方的截图 / 鼠标 / 键盘 / 绑定模式后重试。"));
  auto* test_box_layout = new QHBoxLayout(test_box);
  test_box_layout->setContentsMargins(4, 8, 4, 4);
  test_box_layout->setSpacing(4);

  test_capture_btn_ = new QPushButton(QStringLiteral("测试截图"), test_box);

  // 鼠标测试组：坐标框 + 取点（与动作行一致）+ 测试鼠标
  auto* mouse_test_group = new QFrame(test_box);
  mouse_test_group->setObjectName(QStringLiteral("testActionGroup"));
  mouse_test_group->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  auto* mouse_gl = new QHBoxLayout(mouse_test_group);
  mouse_gl->setContentsMargins(3, 1, 3, 1);
  mouse_gl->setSpacing(4);
  test_mouse_point_edit_ = new QLineEdit(mouse_test_group);
  test_mouse_point_edit_->setPlaceholderText(QStringLiteral("x,y"));
  test_mouse_point_edit_->setReadOnly(true);
  test_mouse_point_edit_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  test_mouse_point_edit_->setToolTip(
      QStringLiteral("测试鼠标用的客户区坐标（x,y）。只读，请用「取点」获取；未取点则点中心。"));
  {
    const QFontMetrics fm(test_mouse_point_edit_->font());
    const int w = fm.horizontalAdvance(QStringLiteral("999,888")) + 16;
    test_mouse_point_edit_->setFixedWidth(w);
  }
  test_mouse_point_btn_ = new QPushButton(QStringLiteral("取点"), mouse_test_group);
  test_mouse_point_btn_->setIcon(QIcon(QStringLiteral(":/png/selPt.png")));
  test_mouse_point_btn_->setIconSize(QSize(16, 16));
  test_mouse_btn_ = new QPushButton(QStringLiteral("测试鼠标"), mouse_test_group);
  mouse_gl->addWidget(test_mouse_point_edit_);
  mouse_gl->addWidget(test_mouse_point_btn_);
  mouse_gl->addWidget(test_mouse_btn_);

  // 键盘测试组：按键捕获（与按键行一致）+ 测试键盘
  auto* key_test_group = new QFrame(test_box);
  key_test_group->setObjectName(QStringLiteral("testActionGroup"));
  key_test_group->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  auto* key_gl = new QHBoxLayout(key_test_group);
  key_gl->setContentsMargins(3, 1, 3, 1);
  key_gl->setSpacing(4);
  test_key_capture_ = new KeyCaptureEdit(key_test_group);
  test_key_capture_->setFromStorageText(test_key_storage_);
  test_key_capture_->setToolTip(
      QStringLiteral("点击后按下要测试的按键（可含组合键），与动作行按键设置相同。"));
  test_key_btn_ = new QPushButton(QStringLiteral("测试键盘"), key_test_group);
  key_gl->addWidget(test_key_capture_);
  key_gl->addWidget(test_key_btn_);

  test_box_layout->addWidget(test_capture_btn_);
  test_box_layout->addWidget(mouse_test_group);
  test_box_layout->addWidget(key_test_group);
  test_box_layout->addStretch(1);

  modes_col->addWidget(mode_box);
  modes_col->addWidget(test_box);
  bind_root->addWidget(modes_stack, 0, Qt::AlignLeft);
  root->addWidget(bind_box);

  RefreshTestActionTips();

  connect(select_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnSelectWindow);
  connect(highlight_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnHighlightWindow);
  connect(test_capture_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnTestCapture);
  connect(test_mouse_point_btn_, &QPushButton::clicked, this,
          &KeyPresserNextWindow::OnSetTestMousePoint);
  connect(test_mouse_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnTestMouse);
  connect(test_key_capture_, &KeyCaptureEdit::bindingChanged, this, [this]() {
    if (test_key_capture_ && test_key_capture_->hasBinding()) {
      test_key_storage_ = test_key_capture_->storageText();
    }
    RefreshTestActionTips();
  });
  connect(test_key_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnTestKey);
  connect(display_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &KeyPresserNextWindow::OnBindModesChanged);
  connect(mouse_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &KeyPresserNextWindow::OnBindModesChanged);
  connect(keypad_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &KeyPresserNextWindow::OnBindModesChanged);
  connect(bind_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &KeyPresserNextWindow::OnBindModesChanged);
  connect(topmost_check_, &QCheckBox::toggled, this, [this](bool on) {
    // 与原版一致：取消勾选立即取消置顶；勾选则恢复最小化并置顶
    if (target_hwnd_) {
      WindowSelectHelper::setTargetWindowTopmost(target_hwnd_, on);
    }
    if (executor_) {
      executor_->SetKeepTopmost(on);
    }
    MarkDirty();
  });
  connect(run_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() { OnRunModeChanged(); });

  auto* tool_bar = new QFrame(this);
  tool_bar->setObjectName(QStringLiteral("actionToolBar"));
  auto* tool_row = new QHBoxLayout(tool_bar);
  tool_row->setContentsMargins(6, 4, 6, 4);
  tool_row->setSpacing(6);

  add_key_btn_ = new QPushButton(tool_bar);
  add_mouse_btn_ = new QPushButton(tool_bar);
  bulk_add_btn_ = new QPushButton(QStringLiteral("批量添加"), tool_bar);
  clear_btn_ = new QPushButton(tool_bar);
  undo_btn_ = new QPushButton(QStringLiteral("撤销"), tool_bar);
  redo_btn_ = new QPushButton(QStringLiteral("重做"), tool_bar);
  start_rec_btn_ = new QPushButton(tool_bar);

  // 左侧：编辑行；清空右侧为撤销/重做；最右：录制
  tool_row->addWidget(add_key_btn_);
  tool_row->addWidget(add_mouse_btn_);
  tool_row->addWidget(bulk_add_btn_);
  tool_row->addWidget(clear_btn_);
  tool_row->addWidget(undo_btn_);
  tool_row->addWidget(redo_btn_);

  auto* tool_sep = new QFrame(tool_bar);
  tool_sep->setObjectName(QStringLiteral("toolSep"));
  tool_sep->setFrameShape(QFrame::VLine);
  tool_sep->setFrameShadow(QFrame::Plain);
  tool_sep->setFixedWidth(1);
  tool_row->addSpacing(4);
  tool_row->addWidget(tool_sep);
  tool_row->addSpacing(4);
  tool_row->addStretch(1);
  tool_row->addWidget(start_rec_btn_);
  root->addWidget(tool_bar);

  connect(add_key_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnAddKeyRow);
  connect(add_mouse_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnAddMouseRow);
  connect(bulk_add_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnBulkAddRows);
  connect(clear_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnClearRows);
  connect(undo_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnUndoRows);
  connect(redo_btn_, &QPushButton::clicked, this, &KeyPresserNextWindow::OnRedoRows);
  connect(start_rec_btn_, &QPushButton::clicked, this, [this]() {
    if (recording_) {
      OnStopRecord();
    } else {
      OnStartRecord();
    }
  });

  ResetRowsHistory();

  auto* scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("rowsScroll"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::StyledPanel);
  scroll->setLineWidth(1);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* rows_host = new QWidget(scroll);
  rows_host->setObjectName(QStringLiteral("rowsHost"));
  rows_host->setAttribute(Qt::WA_StyledBackground, true);
  rows_layout_ = new QVBoxLayout(rows_host);
  rows_layout_->setContentsMargins(0, 0, 0, 0);
  rows_layout_->setSpacing(0);
  rows_layout_->addStretch();
  scroll->setWidget(rows_host);
  scroll->setMinimumHeight(120);

  auto* bottom_panel = new QWidget(this);
  bottom_panel_ = bottom_panel;
  bottom_panel->setObjectName(QStringLiteral("bottomPanel"));
  bottom_panel->setMinimumHeight(140);
  auto* bottom_layout = new QVBoxLayout(bottom_panel);
  bottom_layout->setContentsMargins(0, 0, 0, 0);
  bottom_layout->setSpacing(6);

  auto* run_row = new QHBoxLayout();
  run_row->setSpacing(8);
  start_btn_ = new QPushButton(bottom_panel);

  auto* count_label = new QLabel(QStringLiteral("次数"), bottom_panel);
  run_count_spin_ = new QSpinBox(bottom_panel);
  run_count_spin_->setRange(0, 999999);
  run_count_spin_->setSpecialValueText(QStringLiteral("无限"));
  run_count_spin_->setValue(0);
  run_count_spin_->setToolTip(
      QStringLiteral("运行次数。0（无限）表示一直跑到手动停止。\n"
                     "顺序运行：完整扫完启用行算 1 次。\n"
                     "独立运行：每个启用行各执行这么多次（长按除外）。"));

  auto* gap_label = new QLabel(QStringLiteral("间隔"), bottom_panel);
  run_gap_spin_ = new QSpinBox(bottom_panel);
  run_gap_spin_->setRange(0, 3600000);
  run_gap_spin_->setSuffix(QStringLiteral(" ms"));
  run_gap_spin_->setValue(0);
  run_gap_spin_->setToolTip(
      QStringLiteral("额外等待（毫秒），加在行间隔之外。\n"
                     "顺序运行：一整轮结束后再等待该时间。\n"
                     "独立运行：加在每次「做完本行后」的行间隔上。"));

  auto* speed_label = new QLabel(QStringLiteral("速率"), bottom_panel);
  run_speed_spin_ = new QSpinBox(bottom_panel);
  run_speed_spin_->setRange(10, 1000);
  run_speed_spin_->setSuffix(QStringLiteral(" %"));
  run_speed_spin_->setValue(100);
  run_speed_spin_->setSingleStep(10);
  run_speed_spin_->setToolTip(
      QStringLiteral("整体运行速率。100% 为正常；越大等待越短（动作更快）。"));

  status_label_ = new QLabel(bottom_panel);
  run_row->addWidget(count_label);
  run_row->addWidget(run_count_spin_);
  run_row->addWidget(gap_label);
  run_row->addWidget(run_gap_spin_);
  run_row->addWidget(speed_label);
  run_row->addWidget(run_speed_spin_);
  run_row->addWidget(status_label_, 1);
  run_row->addWidget(start_btn_);
  bottom_layout->addLayout(run_row);

  auto* log_panel = new QFrame(bottom_panel);
  log_panel_ = log_panel;
  log_panel->setObjectName(QStringLiteral("logPanel"));
  log_panel->setAttribute(Qt::WA_StyledBackground, true);
  auto* log_layout = new QVBoxLayout(log_panel);
  log_layout->setContentsMargins(8, 6, 8, 6);
  log_layout->setSpacing(4);
  auto* log_header = new QHBoxLayout();
  log_header->setContentsMargins(0, 0, 0, 0);
  log_header->setSpacing(6);
  log_toggle_btn_ = new QPushButton(QStringLiteral("▾"), log_panel);
  log_toggle_btn_->setObjectName(QStringLiteral("logToggleBtn"));
  log_toggle_btn_->setFixedSize(22, 22);
  log_toggle_btn_->setCursor(Qt::PointingHandCursor);
  log_toggle_btn_->setToolTip(QStringLiteral("折叠日志"));
  log_toggle_btn_->setFocusPolicy(Qt::NoFocus);
  auto* log_title = new QLabel(QStringLiteral("日志"), log_panel);
  log_title->setObjectName(QStringLiteral("logTitle"));
  log_title->setCursor(Qt::PointingHandCursor);
  log_title->setToolTip(QStringLiteral("点击折叠 / 展开日志"));
  clear_log_btn_ = new QPushButton(QStringLiteral("清空"), log_panel);
  clear_log_btn_->setObjectName(QStringLiteral("logClearBtn"));
  clear_log_btn_->setFixedHeight(24);
  clear_log_btn_->setCursor(Qt::PointingHandCursor);
  clear_log_btn_->setToolTip(QStringLiteral("清空日志"));
  log_header->addWidget(log_toggle_btn_);
  log_header->addWidget(log_title);
  log_header->addStretch(1);
  log_header->addWidget(clear_log_btn_);
  log_edit_ = new QPlainTextEdit(log_panel);
  log_edit_->setObjectName(QStringLiteral("logEdit"));
  log_edit_->setReadOnly(true);
  log_edit_->setUndoRedoEnabled(false);
  log_edit_->setMaximumBlockCount(800);
  log_edit_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  log_edit_->setMinimumHeight(64);
  log_edit_->setPlaceholderText(QStringLiteral("运行状态与操作记录会显示在这里…"));
  log_layout->addLayout(log_header);
  log_layout->addWidget(log_edit_, 1);
  bottom_layout->addWidget(log_panel, 1);
  connect(clear_log_btn_, &QPushButton::clicked, this, [this]() {
    if (log_edit_) {
      log_edit_->clear();
    }
    last_log_line_.clear();
  });
  const auto toggle_log = [this]() { SetLogCollapsed(!log_collapsed_); };
  connect(log_toggle_btn_, &QPushButton::clicked, this, toggle_log);
  log_title->installEventFilter(this);

  auto* main_split = new MainSplitter(this);
  main_split_ = main_split;
  main_split->setObjectName(QStringLiteral("mainSplit"));
  main_split->setChildrenCollapsible(false);
  main_split->setHandleWidth(20);
  main_split->addWidget(scroll);
  main_split->addWidget(bottom_panel);
  main_split->setStretchFactor(0, 3);
  main_split->setStretchFactor(1, 1);
  main_split->setSizes({420, 200});
  root->addWidget(main_split, 1);

  auto mark_run_opts = [this]() { MarkDirty(); };
  connect(run_count_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, mark_run_opts);
  connect(run_gap_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, mark_run_opts);
  connect(run_speed_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, mark_run_opts);

  connect(start_btn_, &QPushButton::clicked, this, [this]() {
    if (executor_ && executor_->IsRunning()) {
      OnStop();
    } else {
      OnStart();
    }
  });

  const QSize icon_size(18, 18);
  auto set_btn_icon = [&](QPushButton* btn, const QString& path) {
    btn->setIcon(QIcon(path));
    btn->setIconSize(icon_size);
  };
  set_btn_icon(select_btn_, QStringLiteral(":/png/window_recorder.png"));
  set_btn_icon(add_key_btn_, QStringLiteral(":/png/keyboard.png"));
  set_btn_icon(add_mouse_btn_, QStringLiteral(":/png/mouse.png"));
  set_btn_icon(bulk_add_btn_, QStringLiteral(":/png/import.png"));
  set_btn_icon(clear_btn_, QStringLiteral(":/png/clear.png"));
  set_btn_icon(undo_btn_, QStringLiteral(":/png/undo.png"));
  set_btn_icon(redo_btn_, QStringLiteral(":/png/redo.png"));
  set_btn_icon(start_rec_btn_, QStringLiteral(":/png/wr_record.png"));
  set_btn_icon(start_btn_, QStringLiteral(":/png/wr_play.png"));
  bulk_add_btn_->setToolTip(
      QStringLiteral("从文本或文件批量添加按键/鼠标行，支持追加或替换。"));

  UpdateActionButtonTexts();

  {
    QSettings settings;
    SetLogCollapsed(settings.value(QStringLiteral("ui/logCollapsed"), false).toBool());
  }
}

void KeyPresserNextWindow::PopulateModeCombos() {
  const QStringList displays = {QStringLiteral("normal"),        QStringLiteral("normal.auto"),
                                QStringLiteral("normal.dxgi"),   QStringLiteral("normal.wgc"),
                                QStringLiteral("gdi"),           QStringLiteral("gdi2"),
                                QStringLiteral("dx"),            QStringLiteral("dx2"),
                                QStringLiteral("dx.d3d9"),       QStringLiteral("dx.d3d10"),
                                QStringLiteral("dx.d3d11"),      QStringLiteral("dx.d3d12"),
                                QStringLiteral("opengl")};
  for (const QString& d : displays) {
    display_combo_->addItem(ModeLabel(d), d);
  }
  for (const QString& m : {QStringLiteral("normal"), QStringLiteral("windows"), QStringLiteral("dx")}) {
    mouse_combo_->addItem(m, m);
  }
  for (const QString& k :
       {QStringLiteral("normal"), QStringLiteral("normal.hd"), QStringLiteral("windows"),
        QStringLiteral("dx")}) {
    keypad_combo_->addItem(k, k);
  }
  display_combo_->setCurrentIndex(display_combo_->findData(QStringLiteral("normal.auto")));
  mouse_combo_->setCurrentIndex(mouse_combo_->findData(QStringLiteral("windows")));
  keypad_combo_->setCurrentIndex(keypad_combo_->findData(QStringLiteral("windows")));

  BindComboTip(display_combo_, [](const QVariant& d) { return DisplayModeTip(d.toString()); });
  BindComboTip(mouse_combo_, [](const QVariant& d) { return MouseModeTip(d.toString()); });
  BindComboTip(keypad_combo_, [](const QVariant& d) { return KeypadModeTip(d.toString()); });
  BindComboTip(bind_mode_combo_, [](const QVariant& d) { return BindModeTip(d.toInt()); });
  BindComboTip(run_mode_combo_, [](const QVariant& d) {
    if (d.toString() == QLatin1String("independent")) {
      return QStringLiteral(
          "独立运行：各启用行互不影响。\n"
          "开始后各行先尽快执行一遍，再按各自「间隔」循环等待。\n"
          "按键行可显示「长按」。");
    }
    return QStringLiteral(
        "顺序运行：启用行按列表从上到下轮流执行。\n"
        "开始后先尽快执行第一行，再等该行间隔后执行下一行。\n"
        "不显示「长按」。");
  });

  FitComboWidth(display_combo_);
  FitComboWidth(mouse_combo_);
  FitComboWidth(keypad_combo_);
  FitComboWidth(bind_mode_combo_);
  FitComboWidth(run_mode_combo_);
}

QString KeyPresserNextWindow::CurrentDisplay() const {
  return display_combo_->currentData().toString();
}
QString KeyPresserNextWindow::CurrentMouse() const { return mouse_combo_->currentData().toString(); }
QString KeyPresserNextWindow::CurrentKeypad() const {
  return keypad_combo_->currentData().toString();
}

void KeyPresserNextWindow::OnHighlightWindow() {
  if (!target_hwnd_ || !IsWindow(target_hwnd_)) {
    QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择一个窗口！"));
    return;
  }
  WindowSelectHelper::highlightTargetWindow(target_hwnd_, this);
}

void KeyPresserNextWindow::CancelSelectWindowHook() {
  const bool was_selecting = selecting_window_ || select_hook_;
  if (select_hook_) {
    UnhookWinEvent(select_hook_);
    select_hook_ = nullptr;
  }
  selecting_window_ = false;
  if (select_instance_ == this) {
    select_instance_ = nullptr;
  }
  if (was_selecting) {
    EndCrossCursor();
  }
}

void KeyPresserNextWindow::OnSelectWindow() {
  // 与原版一致：监听前台窗口切换，用户点到目标窗即完成选窗（无倒计时弹窗）
  CancelSelectWindowHook();
  select_instance_ = this;
  selecting_window_ = true;
  window_label_->setText(QStringLiteral("请点击目标窗口…"));
  window_label_->setToolTip(QString());
  window_title_full_.clear();
  window_label_->setProperty("bound", false);
  window_label_->style()->unpolish(window_label_);
  window_label_->style()->polish(window_label_);
  NotifyStatus(QStringLiteral("请切换到目标窗口…"));

  select_hook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                 SelectWinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
  if (!select_hook_) {
    selecting_window_ = false;
    select_instance_ = nullptr;
    window_label_->setText(QStringLiteral("未选择窗口"));
    window_label_->setToolTip(QString());
    window_title_full_.clear();
    NotifyStatus(QStringLiteral("无法开始选窗"), 3000);
    return;
  }
  BeginCrossCursor();
}

void CALLBACK KeyPresserNextWindow::SelectWinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                                      LONG id_object, LONG id_child, DWORD thread,
                                                      DWORD time) {
  Q_UNUSED(event);
  Q_UNUSED(id_object);
  Q_UNUSED(id_child);
  Q_UNUSED(thread);
  Q_UNUSED(time);
  if (!hwnd || !select_instance_ || !select_instance_->selecting_window_) {
    return;
  }

  KeyPresserNextWindow* self = select_instance_;
  if (hwnd == reinterpret_cast<HWND>(self->winId())) {
    return;
  }
  if (self->hotkey_dialog_ && hwnd == reinterpret_cast<HWND>(self->hotkey_dialog_->winId())) {
    return;
  }
  if (QWidget* hint = StopHotkeyHint::overlayWidget()) {
    if (hwnd == reinterpret_cast<HWND>(hint->winId())) {
      return;
    }
  }
  // 忽略本进程其它顶层窗（登录/批量添加等）
  for (QWidget* w : QApplication::topLevelWidgets()) {
    if (w && w->isWindow() && hwnd == reinterpret_cast<HWND>(w->winId())) {
      return;
    }
  }

  wchar_t title[512] = {};
  GetWindowTextW(hwnd, title, 511);
  const QString title_str = QString::fromWCharArray(title);
  if (title_str.isEmpty()) {
    return;
  }

  HWND root = GetAncestor(hwnd, GA_ROOT);
  if (root) {
    hwnd = root;
  }
  if (hwnd == reinterpret_cast<HWND>(self->winId())) {
    return;
  }

  // 与原版一致：先卸钩，再回 UI 线程完成绑定并恢复光标
  UnhookWinEvent(hook);
  self->select_hook_ = nullptr;
  self->selecting_window_ = false;
  self->select_instance_ = nullptr;

  QMetaObject::invokeMethod(
      self,
      [self, hwnd]() {
        EndCrossCursor();
        self->FinishSelectWindow(hwnd);
      },
      Qt::QueuedConnection);
}

void KeyPresserNextWindow::FinishSelectWindow(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }

  wchar_t title[512] = {};
  GetWindowTextW(hwnd, title, 511);
  const QString title_text = QString::fromWCharArray(title);

  target_hwnd_ = hwnd;
  window_title_full_ = title_text.isEmpty() ? QStringLiteral("(无标题窗口)") : title_text;
  window_label_->setToolTip(window_title_full_);
  window_label_->setProperty("bound", true);
  window_label_->style()->unpolish(window_label_);
  window_label_->style()->polish(window_label_);
  RefreshWindowTitleLabel();

  EnsureBound();
  if (topmost_check_ && topmost_check_->isChecked()) {
    WindowSelectHelper::setTargetWindowTopmost(target_hwnd_, true);
  }
  MarkDirty();
  NotifyStatus(QStringLiteral("已选择窗口：%1").arg(window_title_full_), 2000);
  WindowSelectHelper::scheduleBringToFront(this);
}

bool KeyPresserNextWindow::EnsureBound() {
  QString error;
  const bool ok = EnsureBoundForCapture(&error);
  if (!ok) {
    NotifyStatus(error.isEmpty() ? QStringLiteral("绑定失败") : error);
  } else {
    NotifyStatus(
        QStringLiteral("绑定成功（截图 %1）").arg(CurrentDisplay()), 2000);
  }
  return ok;
}

bool KeyPresserNextWindow::EnsureBoundForCapture(QString* error) {
  auto set_err = [&](const QString& msg) {
    if (error) {
      *error = msg;
    }
    return false;
  };
  if (!target_hwnd_ || !IsWindow(target_hwnd_)) {
    return set_err(QStringLiteral("请先选择窗口"));
  }
  if (!core_.IsReady()) {
    return set_err(QStringLiteral("引擎未就绪：%1").arg(core_.LastError()));
  }
  const bool ok =
      core_.BindWindow(target_hwnd_, CurrentDisplay(), CurrentMouse(), CurrentKeypad(),
                     bind_mode_combo_->currentData().toInt());
  if (!ok) {
    return set_err(QStringLiteral("绑定失败（截图模式：%1）\n%2")
                       .arg(CurrentDisplay(),
                            core_.LastError().isEmpty() ? QStringLiteral("未知错误")
                                                     : core_.LastError()));
  }
  return true;
}

void KeyPresserNextWindow::KeepTargetTopmostIfNeeded() {
  if (topmost_check_ && topmost_check_->isChecked()) {
    WindowSelectHelper::setTargetWindowTopmost(target_hwnd_, true);
  }
}

void KeyPresserNextWindow::RefreshWindowTitleLabel() {
  if (!window_label_ || window_title_full_.isEmpty()) {
    return;
  }
  // 等布局分配宽度后再省略；首次可能仍为 0，下一帧再刷
  const int avail = window_label_->contentsRect().width();
  if (avail <= 8) {
    QTimer::singleShot(0, this, [this]() { RefreshWindowTitleLabel(); });
    return;
  }
  window_label_->setText(
      window_label_->fontMetrics().elidedText(window_title_full_, Qt::ElideRight, avail));
}

void KeyPresserNextWindow::SetBindControlsEnabled(bool enabled) {
  if (select_btn_) {
    select_btn_->setEnabled(enabled);
  }
  if (highlight_btn_) {
    highlight_btn_->setEnabled(enabled);
  }
  if (test_capture_btn_) {
    test_capture_btn_->setEnabled(enabled);
  }
  if (test_mouse_point_edit_) {
    test_mouse_point_edit_->setEnabled(enabled);
  }
  if (test_mouse_point_btn_) {
    test_mouse_point_btn_->setEnabled(enabled);
  }
  if (test_mouse_btn_) {
    test_mouse_btn_->setEnabled(enabled);
  }
  if (test_key_capture_) {
    test_key_capture_->setEnabled(enabled);
  }
  if (test_key_btn_) {
    test_key_btn_->setEnabled(enabled);
  }
  if (display_combo_) {
    display_combo_->setEnabled(enabled);
  }
  if (mouse_combo_) {
    mouse_combo_->setEnabled(enabled);
  }
  if (keypad_combo_) {
    keypad_combo_->setEnabled(enabled);
  }
  if (bind_mode_combo_) {
    bind_mode_combo_->setEnabled(enabled);
  }
  if (topmost_check_) {
    topmost_check_->setEnabled(enabled);
  }
}

void KeyPresserNextWindow::OnBindModesChanged() {
  if (target_hwnd_) {
    EnsureBound();
  }
  RefreshTestActionTips();
  MarkDirty();
}

void KeyPresserNextWindow::OnTestCapture() {
  QString error;
  if (!EnsureBoundForCapture(&error)) {
    QMessageBox::warning(this, QStringLiteral("测试截图"), error);
    return;
  }
  KeepTargetTopmostIfNeeded();

  int w = 0;
  int h = 0;
  if (!core_.GetClientSize(target_hwnd_, &w, &h) || w <= 0 || h <= 0) {
    QMessageBox::warning(this, QStringLiteral("测试截图"),
                         QStringLiteral("无法获取窗口客户区尺寸"));
    return;
  }

  const QString dir = SettingsStore::EnsureImagesDir();
  const QString path = QDir(dir).filePath(
      QStringLiteral("test_capture_%1.bmp")
          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"))));
  if (!core_.Capture(0, 0, w, h, path) || !QFileInfo::exists(path)) {
    QMessageBox::warning(
        this, QStringLiteral("测试截图"),
        QStringLiteral("截图失败（模式：%1）\n%2")
            .arg(CurrentDisplay(), core_.LastError().isEmpty()
                                       ? QStringLiteral("请检查窗口是否可见、截图方式是否匹配")
                                       : core_.LastError()));
    return;
  }

  NotifyStatus(
      QStringLiteral("测试截图成功（%1） %2×%3").arg(CurrentDisplay()).arg(w).arg(h), 4000);

  QPixmap pix(path);
  auto* dlg = new QDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle(QStringLiteral("测试截图 — %1").arg(CurrentDisplay()));
  dlg->setWindowFlags(dlg->windowFlags() & ~Qt::WindowContextHelpButtonHint);

  auto* layout = new QVBoxLayout(dlg);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  const QSize img_size = pix.isNull() ? QSize(w, h) : pix.size();
  auto* info = new QLabel(
      QStringLiteral("%1×%2 · %3")
          .arg(img_size.width())
          .arg(img_size.height())
          .arg(QDir::toNativeSeparators(path)),
      dlg);
  info->setWordWrap(true);
  info->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(info);

  auto* scroll = new QScrollArea(dlg);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(false);
  scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  auto* preview = new QLabel(scroll);
  preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  if (!pix.isNull()) {
    preview->setPixmap(pix);  // 1:1 原图，不缩放
    preview->setFixedSize(pix.size());
  } else {
    preview->setText(QStringLiteral("已保存，但无法预览该图片格式"));
  }
  scroll->setWidget(preview);
  layout->addWidget(scroll, 1);

  auto* buttons = new QDialogButtonBox(dlg);
  auto* open_btn = buttons->addButton(QStringLiteral("打开文件"), QDialogButtonBox::ActionRole);
  buttons->addButton(QDialogButtonBox::Close);
  layout->addWidget(buttons);
  connect(open_btn, &QPushButton::clicked, dlg, [path]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  });
  connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
  connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, dlg, &QDialog::close);

  // 按图片实际大小调窗口；超出屏幕可用区域时钳制并出现滚动条
  QScreen* scr = screen() ? screen() : QGuiApplication::primaryScreen();
  const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1280, 720);
  constexpr int kMargin = 24;
  const int chrome_w = 16;
  const int chrome_h = info->sizeHint().height() + buttons->sizeHint().height() + 28;
  const int max_w = qMax(320, avail.width() - kMargin * 2);
  const int max_h = qMax(240, avail.height() - kMargin * 2);
  const int need_w = img_size.width() + chrome_w;
  const int need_h = img_size.height() + chrome_h;
  const bool fits = need_w <= max_w && need_h <= max_h;
  if (fits) {
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dlg->resize(need_w, need_h);
  } else {
    dlg->resize(qMin(need_w, max_w), qMin(need_h, max_h));
  }
  dlg->move(avail.x() + (avail.width() - dlg->width()) / 2,
            avail.y() + (avail.height() - dlg->height()) / 2);
  dlg->show();
}

void KeyPresserNextWindow::RefreshTestActionTips() {
  if (test_capture_btn_) {
    test_capture_btn_->setToolTip(
        QStringLiteral("用当前截图模式（%1）截取目标窗口客户区并预览。\n"
                       "用于确认绑定/画面是否正常；失败可换 gdi、normal.dxgi、dx 等再试。\n"
                       "行内截图与运行识图使用同一截图模式。")
            .arg(CurrentDisplay()));
  }
  if (test_mouse_point_edit_) {
    if (test_mouse_point_set_) {
      test_mouse_point_edit_->setText(
          QStringLiteral("%1,%2").arg(test_mouse_x_).arg(test_mouse_y_));
    } else {
      test_mouse_point_edit_->clear();
    }
    const QFontMetrics fm(test_mouse_point_edit_->font());
    const QString sample = test_mouse_point_edit_->text().isEmpty()
                               ? QStringLiteral("999,888")
                               : test_mouse_point_edit_->text();
    test_mouse_point_edit_->setFixedWidth(fm.horizontalAdvance(sample) + 16);
  }
  if (test_mouse_point_btn_) {
    test_mouse_point_btn_->setText(QStringLiteral("取点"));
    test_mouse_point_btn_->setToolTip(
        test_mouse_point_set_
            ? QStringLiteral("当前测试坐标：(%1,%2)\n点击后，在目标位置再点一下左键完成取点。")
                  .arg(test_mouse_x_)
                  .arg(test_mouse_y_)
            : QStringLiteral("获取测试鼠标点击的客户区坐标，填入左侧；未取点时默认点客户区中心。\n"
                             "点击后，在目标位置再点一下左键即可（与原版一致，无需等待）。"));
  }
  if (test_mouse_btn_) {
    if (test_mouse_point_set_) {
      test_mouse_btn_->setToolTip(
          QStringLiteral("用当前鼠标模式（%1），在取点坐标 (%2,%3) 点击左键。\n"
                         "请观察目标窗口是否有响应；无反应可换 windows / dx / normal 再试。")
              .arg(CurrentMouse())
              .arg(test_mouse_x_)
              .arg(test_mouse_y_));
    } else {
      test_mouse_btn_->setToolTip(
          QStringLiteral("用当前鼠标模式（%1），在客户区中心点击左键。\n"
                         "可先「取点」指定位置；请观察目标窗口是否有响应。\n"
                         "无反应可换 windows / dx / normal 再试。")
              .arg(CurrentMouse()));
    }
  }

  QString key_label = QStringLiteral("A");
  if (test_key_capture_ && test_key_capture_->hasBinding()) {
    key_label = test_key_capture_->displayText();
    test_key_storage_ = test_key_capture_->storageText();
  } else {
    const KeyBinding key = KeyBinding::fromDisplayText(test_key_storage_);
    if (key.isValid()) {
      key_label = key.displayText();
    }
  }
  if (test_key_capture_) {
    test_key_capture_->setToolTip(
        QStringLiteral("点击后按下要测试的按键（可含组合键），与动作行按键设置相同。\n"
                       "当前：%1")
            .arg(key_label));
  }
  if (test_key_btn_) {
    test_key_btn_->setToolTip(
        QStringLiteral("用当前键盘模式（%1），向目标窗口发送「%2」。\n"
                       "请观察目标窗口是否有响应；无反应可换 windows / dx / normal 再试。")
            .arg(CurrentKeypad(), key_label));
  }
}

void KeyPresserNextWindow::OnSetTestMousePoint() {
  StartPickPoint(nullptr);
}

void KeyPresserNextWindow::StartPickPoint(ActionRowWidget* row) {
  if (!target_hwnd_ || !IsWindow(target_hwnd_)) {
    QMessageBox::information(this, QStringLiteral("取点"), QStringLiteral("请先选择窗口"));
    return;
  }
  if (recording_ || (executor_ && executor_->IsRunning())) {
    QMessageBox::warning(this, QStringLiteral("取点"),
                         QStringLiteral("运行或录制中无法取点，请先停止。"));
    return;
  }

  CancelPickPointHook();
  pick_row_ = row;
  pick_instance_ = this;
  KeepTargetTopmostIfNeeded();
  pick_mouse_hook_ =
      SetWindowsHookExW(WH_MOUSE_LL, PickMouseProc, GetModuleHandleW(nullptr), 0);
  if (!pick_mouse_hook_) {
    pick_instance_ = nullptr;
    pick_row_ = nullptr;
    QMessageBox::warning(this, QStringLiteral("取点"), QStringLiteral("无法安装鼠标钩子"));
    return;
  }
  BeginCrossCursor();

  if (row) {
    // 与原版一致：编辑框提示等待点击
    row->SetPointHintPending();
  } else if (test_mouse_point_edit_) {
    test_mouse_point_edit_->setText(QStringLiteral("请点击目标位置"));
  }
  NotifyStatus(QStringLiteral("请点击目标位置取点…"));
}

void KeyPresserNextWindow::CancelPickPointHook() {
  const bool was_picking = pick_mouse_hook_ || pick_instance_ == this;
  if (pick_mouse_hook_) {
    UnhookWindowsHookEx(pick_mouse_hook_);
    pick_mouse_hook_ = nullptr;
  }
  if (pick_instance_ == this) {
    pick_instance_ = nullptr;
  }
  pick_row_ = nullptr;
  if (was_picking) {
    EndCrossCursor();
  }
}

void KeyPresserNextWindow::FinishPickPoint(int client_x, int client_y) {
  ActionRowWidget* row = pick_row_;
  CancelPickPointHook();
  if (row) {
    row->SetPoint(client_x, client_y);
  } else {
    test_mouse_x_ = client_x;
    test_mouse_y_ = client_y;
    test_mouse_point_set_ = true;
    RefreshTestActionTips();
  }
  NotifyStatus(QStringLiteral("已取点 (%1,%2)").arg(client_x).arg(client_y), 3000);
}

LRESULT CALLBACK KeyPresserNextWindow::PickMouseProc(int code, WPARAM wparam, LPARAM lparam) {
  HHOOK hook = pick_instance_ ? pick_instance_->pick_mouse_hook_ : nullptr;
  if (code == HC_ACTION && pick_instance_ && wparam == WM_LBUTTONDOWN) {
    KeyPresserNextWindow* inst = pick_instance_;
    if (inst->target_hwnd_ && IsWindow(inst->target_hwnd_)) {
      const auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lparam);
      int x = 0;
      int y = 0;
      if (!WindowCoordHelper::ScreenToClientDpiAware(inst->target_hwnd_, ms->pt.x, ms->pt.y, &x,
                                                     &y)) {
        return CallNextHookEx(hook, code, wparam, lparam);
      }
      ActionRowWidget* row = inst->pick_row_;
      // 与原版一致：立刻卸钩，再回 UI 填坐标
      if (inst->pick_mouse_hook_) {
        UnhookWindowsHookEx(inst->pick_mouse_hook_);
        inst->pick_mouse_hook_ = nullptr;
      }
      pick_instance_ = nullptr;
      inst->pick_row_ = nullptr;
      EndCrossCursor();
      QTimer::singleShot(0, inst, [inst, row, x, y]() {
        // 钩子里已清 pick_row_，直接写入目标，避免与 Cancel 竞态
        if (row) {
          row->SetPoint(x, y);
        } else {
          inst->test_mouse_x_ = x;
          inst->test_mouse_y_ = y;
          inst->test_mouse_point_set_ = true;
          inst->RefreshTestActionTips();
        }
        inst->NotifyStatus(QStringLiteral("已取点 (%1,%2)").arg(x).arg(y), 3000);
      });
      return 1;  // 吃掉这次点击，避免误点目标窗
    }
  }
  return CallNextHookEx(hook, code, wparam, lparam);
}

void KeyPresserNextWindow::OnTestMouse() {
  QString error;
  if (!EnsureBoundForCapture(&error)) {
    NotifyStatus(QStringLiteral("测试鼠标失败：%1").arg(error), 5000);
    return;
  }
  KeepTargetTopmostIfNeeded();

  int w = 0;
  int h = 0;
  if (!core_.GetClientSize(target_hwnd_, &w, &h) || w <= 0 || h <= 0) {
    NotifyStatus(QStringLiteral("测试鼠标失败：无法获取窗口客户区尺寸"), 5000);
    return;
  }

  const int x = test_mouse_point_set_ ? test_mouse_x_ : (w / 2);
  const int y = test_mouse_point_set_ ? test_mouse_y_ : (h / 2);
  if (!core_.MoveTo(x, y) || !core_.LeftClick()) {
    const QString detail =
        core_.LastError().isEmpty() ? QStringLiteral("请检查目标窗口是否响应后台鼠标")
                                  : core_.LastError();
    NotifyStatus(
        QStringLiteral("测试鼠标失败（%1）：%2").arg(CurrentMouse(), detail), 5000);
    return;
  }

  const QString where = test_mouse_point_set_ ? QStringLiteral("指定坐标") : QStringLiteral("中心");
  NotifyStatus(
      QStringLiteral("测试鼠标成功（%1）%2左键 (%3,%4)，请观察目标窗口是否有响应")
          .arg(CurrentMouse(), where)
          .arg(x)
          .arg(y),
      4000);
}

void KeyPresserNextWindow::OnTestKey() {
  QString error;
  if (!EnsureBoundForCapture(&error)) {
    NotifyStatus(QStringLiteral("测试键盘失败：%1").arg(error), 5000);
    return;
  }
  KeepTargetTopmostIfNeeded();

  KeyBinding b;
  if (test_key_capture_ && test_key_capture_->hasBinding()) {
    b = test_key_capture_->binding();
    test_key_storage_ = test_key_capture_->storageText();
  } else {
    b = KeyBinding::fromDisplayText(test_key_storage_);
  }
  if (!b.isValid()) {
    b.mainVkCode = 'A';
  }
  const auto mods = b.modifierSendCodes(KeySendProfile::WindowsVk);
  for (int m : mods) {
    core_.KeyDown(windowsVkForKeyInput(m));
  }
  const int main_vk = windowsVkForKeyInput(b.mainSendCode(KeySendProfile::WindowsVk));
  if (!core_.KeyPress(main_vk)) {
    for (int i = mods.size() - 1; i >= 0; --i) {
      core_.KeyUp(windowsVkForKeyInput(mods[i]));
    }
    const QString detail =
        core_.LastError().isEmpty() ? QStringLiteral("请检查目标窗口是否响应后台按键")
                                  : core_.LastError();
    NotifyStatus(
        QStringLiteral("测试键盘失败（%1）：%2").arg(CurrentKeypad(), detail), 5000);
    return;
  }
  for (int i = mods.size() - 1; i >= 0; --i) {
    core_.KeyUp(windowsVkForKeyInput(mods[i]));
  }

  const QString key_label = b.displayText().isEmpty() ? QStringLiteral("A") : b.displayText();
  NotifyStatus(
      QStringLiteral("测试键盘成功（%1）已发送 %2，请观察目标窗口是否有响应")
          .arg(CurrentKeypad(), key_label),
      4000);
}

ActionRowWidget* KeyPresserNextWindow::AppendRowWidget(ActionKind kind) {
  auto* w = new ActionRowWidget(kind, this);
  row_widgets_.append(w);
  rows_layout_->insertWidget(rows_layout_->count() - 1, w);
  WireRowWidget(w);
  return w;
}

void KeyPresserNextWindow::WireRowWidget(ActionRowWidget* w) {
  connect(w, &ActionRowWidget::removeRequested, this, [this, w]() {
    row_widgets_.removeOne(w);
    w->deleteLater();
    RefreshRowIndexes();
    NotifyRowsStructureEdited();
  });
  connect(w, &ActionRowWidget::changed, this, &KeyPresserNextWindow::NotifyRowsEdited);
  connect(w, &ActionRowWidget::screenshotRequested, this, &KeyPresserNextWindow::OnRowScreenshot);
  connect(w, &ActionRowWidget::pickPointRequested, this, &KeyPresserNextWindow::OnRowPickPoint);
  const bool independent =
      run_mode_combo_ &&
      run_mode_combo_->currentData().toString() == QLatin1String("independent");
  w->SetHoldVisible(independent);
  w->SetKeyStrokeVisible(!independent);
  w->SetMousePressReleaseVisible(!independent);
}

void KeyPresserNextWindow::ClearRowWidgets() {
  for (ActionRowWidget* w : row_widgets_) {
    rows_layout_->removeWidget(w);
    delete w;
  }
  row_widgets_.clear();
}

QVector<ActionRow> KeyPresserNextWindow::CollectRows() const {
  QVector<ActionRow> rows;
  rows.reserve(row_widgets_.size());
  for (ActionRowWidget* w : row_widgets_) {
    rows.append(w->ToRow());
  }
  return rows;
}

void KeyPresserNextWindow::ApplyRowsSnapshot(const QVector<ActionRow>& rows) {
  applying_rows_history_ = true;
  if (rows_history_timer_) {
    rows_history_timer_->stop();
  }

  // 尽量复用同类型控件，避免整表销毁重建导致卡顿
  while (row_widgets_.size() > rows.size()) {
    ActionRowWidget* w = row_widgets_.takeLast();
    rows_layout_->removeWidget(w);
    delete w;
  }

  for (int i = 0; i < rows.size(); ++i) {
    const ActionRow& row = rows[i];
    if (i < row_widgets_.size()) {
      ActionRowWidget* w = row_widgets_[i];
      if (w->Kind() != row.kind) {
        rows_layout_->removeWidget(w);
        delete w;
        w = new ActionRowWidget(row.kind, this);
        row_widgets_[i] = w;
        rows_layout_->insertWidget(i, w);
        WireRowWidget(w);
      }
      w->FromRow(row);
    } else {
      AppendRowWidget(row.kind)->FromRow(row);
    }
  }

  RefreshRowIndexes();
  applying_rows_history_ = false;
  MarkDirty();
  UpdateUndoRedoButtons();
}

void KeyPresserNextWindow::PushRowsHistory() {
  if (applying_rows_history_ || loading_settings_) {
    return;
  }
  const QVector<ActionRow> cur = CollectRows();
  if (rows_history_index_ >= 0 && rows_history_index_ < rows_history_.size() &&
      rows_history_[rows_history_index_] == cur) {
    return;
  }
  while (rows_history_.size() > rows_history_index_ + 1) {
    rows_history_.removeLast();
  }
  rows_history_.append(cur);
  rows_history_index_ = rows_history_.size() - 1;
  constexpr int kMaxRowsHistory = 50;
  while (rows_history_.size() > kMaxRowsHistory) {
    rows_history_.removeFirst();
    --rows_history_index_;
  }
  UpdateUndoRedoButtons();
}

void KeyPresserNextWindow::ResetRowsHistory() {
  if (rows_history_timer_) {
    rows_history_timer_->stop();
  }
  rows_history_.clear();
  rows_history_.append(CollectRows());
  rows_history_index_ = 0;
  UpdateUndoRedoButtons();
}

void KeyPresserNextWindow::UpdateUndoRedoButtons() {
  const bool busy = recording_ || (executor_ && executor_->IsRunning());
  const bool can_undo = !busy && rows_history_index_ > 0;
  const bool can_redo =
      !busy && rows_history_index_ >= 0 && rows_history_index_ < rows_history_.size() - 1;
  if (undo_btn_) {
    undo_btn_->setEnabled(can_undo);
  }
  if (redo_btn_) {
    redo_btn_->setEnabled(can_redo);
  }
}

void KeyPresserNextWindow::SchedulePushRowsHistory() {
  if (applying_rows_history_ || loading_settings_) {
    return;
  }
  if (!rows_history_timer_) {
    rows_history_timer_ = new QTimer(this);
    rows_history_timer_->setSingleShot(true);
    rows_history_timer_->setInterval(280);
    connect(rows_history_timer_, &QTimer::timeout, this, &KeyPresserNextWindow::PushRowsHistory);
  }
  rows_history_timer_->start();
}

void KeyPresserNextWindow::NotifyRowsEdited() {
  if (applying_rows_history_ || loading_settings_) {
    return;
  }
  MarkDirty();
  SchedulePushRowsHistory();
}

void KeyPresserNextWindow::NotifyRowsStructureEdited() {
  if (applying_rows_history_ || loading_settings_) {
    return;
  }
  if (rows_history_timer_) {
    rows_history_timer_->stop();
  }
  MarkDirty();
  PushRowsHistory();
}

void KeyPresserNextWindow::AppendRows(const QVector<ActionRow>& rows, bool append) {
  applying_rows_history_ = true;
  if (!append) {
    ClearRowWidgets();
  }
  for (const ActionRow& row : rows) {
    AppendRowWidget(row.kind)->FromRow(row);
  }
  RefreshRowIndexes();
  applying_rows_history_ = false;
  NotifyRowsStructureEdited();
}

void KeyPresserNextWindow::OnAddKeyRow() {
  AppendRowWidget(ActionKind::Key);
  RefreshRowIndexes();
  NotifyRowsStructureEdited();
}

void KeyPresserNextWindow::OnAddMouseRow() {
  AppendRowWidget(ActionKind::Mouse);
  RefreshRowIndexes();
  NotifyRowsStructureEdited();
}

void KeyPresserNextWindow::OnBulkAddRows() {
  if (recording_ || (executor_ && executor_->IsRunning())) {
    QMessageBox::warning(this, QStringLiteral("提示"),
                         QStringLiteral("运行或录制中无法批量添加行，请先停止。"));
    return;
  }

  BulkAddRowsDialog dialog(this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QVector<ActionRow> rows = dialog.ParsedRows();
  if (rows.isEmpty()) {
    QMessageBox::warning(
        this, QStringLiteral("导入失败"),
        QStringLiteral("未能解析任何有效行。\n\n"
                       "按键示例：\nF1\n1,Ctrl+A,1000,1000\n1,,F3,0,1000,1000\n\n"
                       "鼠标示例：\n左键\n1,右键,1000,1000\n1,左键,100,200,500,800"));
    return;
  }

  AppendRows(rows, dialog.AppendMode());
  NotifyStatus(QStringLiteral("已批量添加 %1 行").arg(rows.size()), 3000);
}

void KeyPresserNextWindow::OnClearRows() {
  if (row_widgets_.isEmpty()) {
    return;
  }
  ClearRowWidgets();
  NotifyRowsStructureEdited();
}

void KeyPresserNextWindow::OnUndoRows() {
  if (recording_ || (executor_ && executor_->IsRunning())) {
    return;
  }
  if (rows_history_timer_ && rows_history_timer_->isActive()) {
    rows_history_timer_->stop();
    PushRowsHistory();
  }
  if (rows_history_index_ <= 0) {
    return;
  }
  --rows_history_index_;
  ApplyRowsSnapshot(rows_history_[rows_history_index_]);
  NotifyStatus(QStringLiteral("已撤销"), 2000);
}

void KeyPresserNextWindow::OnRedoRows() {
  if (recording_ || (executor_ && executor_->IsRunning())) {
    return;
  }
  if (rows_history_timer_ && rows_history_timer_->isActive()) {
    rows_history_timer_->stop();
    PushRowsHistory();
  }
  if (rows_history_index_ < 0 || rows_history_index_ >= rows_history_.size() - 1) {
    return;
  }
  ++rows_history_index_;
  ApplyRowsSnapshot(rows_history_[rows_history_index_]);
  NotifyStatus(QStringLiteral("已重做"), 2000);
}

void KeyPresserNextWindow::RefreshRowIndexes() {
  for (int i = 0; i < row_widgets_.size(); ++i) {
    row_widgets_[i]->SetIndex(i);
  }
}

void KeyPresserNextWindow::RefreshHoldVisibility() {
  const bool independent =
      run_mode_combo_ &&
      run_mode_combo_->currentData().toString() == QLatin1String("independent");
  for (ActionRowWidget* w : row_widgets_) {
    w->SetHoldVisible(independent);
    w->SetKeyStrokeVisible(!independent);
    w->SetMousePressReleaseVisible(!independent);
  }
}

void KeyPresserNextWindow::OnRunModeChanged() {
  if (loading_settings_) {
    RefreshHoldVisibility();
    return;
  }

  const QString mode =
      run_mode_combo_ ? run_mode_combo_->currentData().toString() : QString();
  if (mode == QLatin1String("independent")) {
    int affected = 0;
    for (ActionRowWidget* w : row_widgets_) {
      if (w && w->HasSequentialPressRelease()) {
        ++affected;
      }
    }
    if (affected > 0) {
      const auto reply = QMessageBox::question(
          this, QStringLiteral("切换运行模式"),
          QStringLiteral(
              "动作行中有 %1 处「按下 / 弹起」相关设置。\n\n"
              "独立运行不支持这些选项，切换后将替换为：\n"
              "• 按键「按下」「弹起」→「按键」\n"
              "• 鼠标「左键按下 / 弹起」→「左键」\n"
              "• 鼠标「右键按下 / 弹起」→「右键」\n\n"
              "是否切换为独立运行？")
              .arg(affected),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (reply != QMessageBox::Yes) {
        const int seq = run_mode_combo_->findData(QStringLiteral("sequential"));
        if (seq >= 0) {
          QSignalBlocker blocker(run_mode_combo_);
          run_mode_combo_->setCurrentIndex(seq);
        }
        return;
      }
      applying_rows_history_ = true;
      for (ActionRowWidget* w : row_widgets_) {
        if (w) {
          w->ConvertPressReleaseForIndependent();
        }
      }
      applying_rows_history_ = false;
      NotifyRowsStructureEdited();
      RefreshHoldVisibility();
      return;
    }
  }

  RefreshHoldVisibility();
  MarkDirty();
}

void KeyPresserNextWindow::SetRowsEditingEnabled(bool enabled) {
  for (ActionRowWidget* w : row_widgets_) {
    w->SetEditingEnabled(enabled);
  }
}

AppSettings KeyPresserNextWindow::CollectSettingsFromUi() const {
  AppSettings s;
  s.window.display = CurrentDisplay();
  s.window.mouse = CurrentMouse();
  s.window.keypad = CurrentKeypad();
  s.window.bind_mode = bind_mode_combo_->currentData().toInt();
  s.window.topmost = topmost_check_->isChecked();
  s.window.run_mode = run_mode_combo_->currentData().toString();
  s.window.run_count = run_count_spin_ ? run_count_spin_->value() : 0;
  s.window.run_loop_interval_ms = run_gap_spin_ ? run_gap_spin_->value() : 0;
  s.window.run_speed_percent = run_speed_spin_ ? run_speed_spin_->value() : 100;
  s.window.hotkey_start = hotkey_start_;
  s.window.hotkey_stop = hotkey_start_;  // 与开始运行共用
  s.window.hotkey_record = hotkey_record_;
  s.window.hotkey_stop_record = hotkey_record_;
  s.window.hotkey_select_window = hotkey_select_window_;
  s.window.hotkey_highlight_window = hotkey_highlight_window_;
  s.window.hotkey_add_key = hotkey_add_key_;
  s.window.hotkey_add_mouse = hotkey_add_mouse_;
  s.window.hotkey_clear = hotkey_clear_;
  s.window.hotkey_undo = hotkey_undo_;
  s.window.hotkey_redo = hotkey_redo_;
  for (ActionRowWidget* w : row_widgets_) {
    s.rows.append(w->ToRow());
  }
  return s;
}

void KeyPresserNextWindow::ApplySettingsToUi(const AppSettings& settings) {
  loading_settings_ = true;
  auto set_combo = [](QComboBox* c, const QString& v) {
    const int i = c->findData(v);
    if (i >= 0) c->setCurrentIndex(i);
  };
  set_combo(display_combo_, settings.window.display);
  set_combo(mouse_combo_, settings.window.mouse);
  set_combo(keypad_combo_, settings.window.keypad);
  const int bm = bind_mode_combo_->findData(settings.window.bind_mode);
  if (bm >= 0) bind_mode_combo_->setCurrentIndex(bm);
  topmost_check_->setChecked(settings.window.topmost);
  set_combo(run_mode_combo_, settings.window.run_mode);
  if (run_count_spin_) {
    run_count_spin_->setValue(settings.window.run_count);
  }
  if (run_gap_spin_) {
    run_gap_spin_->setValue(settings.window.run_loop_interval_ms);
  }
  if (run_speed_spin_) {
    run_speed_spin_->setValue(settings.window.run_speed_percent);
  }
  hotkey_start_ = settings.window.hotkey_start;
  hotkey_stop_ = hotkey_start_;  // 开始/停止运行共用同一快捷键
  hotkey_record_ = settings.window.hotkey_record;
  hotkey_stop_record_ = hotkey_record_;  // 开始/停止录制共用同一快捷键
  hotkey_select_window_ = settings.window.hotkey_select_window;
  hotkey_highlight_window_ = settings.window.hotkey_highlight_window;
  hotkey_add_key_ = settings.window.hotkey_add_key;
  hotkey_add_mouse_ = settings.window.hotkey_add_mouse;
  hotkey_clear_ = settings.window.hotkey_clear;
  hotkey_undo_ = settings.window.hotkey_undo;
  hotkey_redo_ = settings.window.hotkey_redo;
  SyncHotkeyDialogFromSettings();
  ApplyHotkeysToEngine();

  AppendRows(settings.rows, false);
  loading_settings_ = false;
  settings_dirty_ = false;
  ResetRowsHistory();
  UpdateWindowTitle();
}

void KeyPresserNextWindow::OnHotkeyStart() {
  // 开始/停止运行共用同一快捷键；录制中禁止（仅允许停止录制键）
  if (recording_) {
    return;
  }
  if (executor_ && executor_->IsRunning()) {
    OnStop();
  } else {
    OnStart();
  }
}

void KeyPresserNextWindow::OnHotkeyStop() {
  // 兼容旧配置：停止键已并入开始键
  OnHotkeyStart();
}

void KeyPresserNextWindow::OnHotkeyRecord() {
  // 运行中禁止（仅允许停止运行键）
  if (executor_ && executor_->IsRunning()) {
    return;
  }
  if (recording_) {
    OnStopRecord();
  } else {
    OnStartRecord();
  }
}

void KeyPresserNextWindow::OnOpenHotkeySettings() {
  SyncHotkeyDialogFromSettings();
  hotkey_dialog_->show();
  hotkey_dialog_->raise();
  hotkey_dialog_->activateWindow();
}

void KeyPresserNextWindow::OnHotkeysChanged() {
  if (!hotkey_dialog_) {
    return;
  }
  using A = GlobalHotkeys::Action;
  hotkey_start_ = hotkey_dialog_->Storage(A::Start);
  hotkey_stop_ = hotkey_start_;  // 与开始运行共用
  hotkey_record_ = hotkey_dialog_->Storage(A::Record);
  hotkey_stop_record_ = hotkey_record_;  // 与开始录制共用
  hotkey_select_window_ = hotkey_dialog_->Storage(A::SelectWindow);
  hotkey_highlight_window_ = hotkey_dialog_->Storage(A::HighlightWindow);
  hotkey_add_key_ = hotkey_dialog_->Storage(A::AddKey);
  hotkey_add_mouse_ = hotkey_dialog_->Storage(A::AddMouse);
  hotkey_clear_ = hotkey_dialog_->Storage(A::Clear);
  hotkey_undo_ = hotkey_dialog_->Storage(A::Undo);
  hotkey_redo_ = hotkey_dialog_->Storage(A::Redo);
  ApplyHotkeysToEngine();
  if (recording_) {
    const QString key = hotkeys_->Label(GlobalHotkeys::Action::Record);
    StopHotkeyHint::updateIfVisible(QStringLiteral("录制中"),
                                    key.isEmpty() ? StopHotkeyLabel() : key);
  } else if (executor_ && executor_->IsRunning()) {
    StopHotkeyHint::updateIfVisible(QStringLiteral("运行中"), StopHotkeyLabel());
  }
  MarkDirty();
}

void KeyPresserNextWindow::ApplyHotkeysToEngine() {
  if (!hotkeys_) {
    return;
  }
  using A = GlobalHotkeys::Action;
  hotkeys_->SetFromStorage(A::Start, hotkey_start_);
  hotkeys_->SetFromStorage(A::Stop, QString());  // 已并入 Start
  hotkeys_->SetFromStorage(A::Record, hotkey_record_);
  hotkeys_->SetFromStorage(A::StopRecord, QString());  // 已并入 Record
  hotkeys_->SetFromStorage(A::SelectWindow, hotkey_select_window_);
  hotkeys_->SetFromStorage(A::HighlightWindow, hotkey_highlight_window_);
  hotkeys_->SetFromStorage(A::AddKey, hotkey_add_key_);
  hotkeys_->SetFromStorage(A::AddMouse, hotkey_add_mouse_);
  hotkeys_->SetFromStorage(A::Clear, hotkey_clear_);
  hotkeys_->SetFromStorage(A::Undo, hotkey_undo_);
  hotkeys_->SetFromStorage(A::Redo, hotkey_redo_);
  SyncHotkeyRestrictMode();
  UpdateActionButtonTexts();
}

void KeyPresserNextWindow::SyncHotkeyRestrictMode() {
  if (!hotkeys_) {
    return;
  }
  if (recording_) {
    hotkeys_->SetRestrictMode(GlobalHotkeys::RestrictMode::OnlyRecordStop);
  } else if (executor_ && executor_->IsRunning()) {
    hotkeys_->SetRestrictMode(GlobalHotkeys::RestrictMode::OnlyStartStop);
  } else {
    hotkeys_->SetRestrictMode(GlobalHotkeys::RestrictMode::None);
  }
}

void KeyPresserNextWindow::UpdateEditActionButtonsEnabled() {
  const bool busy = recording_ || (executor_ && executor_->IsRunning());
  const bool idle = !busy;
  if (add_key_btn_) {
    add_key_btn_->setEnabled(idle);
  }
  if (add_mouse_btn_) {
    add_mouse_btn_->setEnabled(idle);
  }
  if (bulk_add_btn_) {
    bulk_add_btn_->setEnabled(idle);
  }
  if (clear_btn_) {
    clear_btn_->setEnabled(idle);
  }
  if (start_rec_btn_) {
    // 录制中保留停止；运行中禁用录制
    start_rec_btn_->setEnabled(!executor_ || !executor_->IsRunning());
  }
  if (start_btn_) {
    // 运行中保留停止；录制中禁用开始运行
    start_btn_->setEnabled(!recording_);
  }
  if (run_mode_combo_) {
    run_mode_combo_->setEnabled(idle);
  }
  UpdateUndoRedoButtons();
}

QString KeyPresserNextWindow::ButtonTextWithHotkey(const QString& base, const QString& hotkey_label) {
  if (hotkey_label.trimmed().isEmpty()) {
    return base;
  }
  return QStringLiteral("%1 (%2)").arg(base, hotkey_label);
}

QString KeyPresserNextWindow::HotkeyLabel(const QString& storage) const {
  if (storage.trimmed().isEmpty()) {
    return QString();
  }
  return KeyBinding::fromDisplayText(storage).displayText();
}

void KeyPresserNextWindow::UpdateActionButtonTexts() {
  using A = GlobalHotkeys::Action;
  const QString start_key = hotkeys_ ? hotkeys_->Label(A::Start) : HotkeyLabel(hotkey_start_);
  const QString stop_key = start_key;  // 开始/停止运行同一键
  const QString record_key = hotkeys_ ? hotkeys_->Label(A::Record) : HotkeyLabel(hotkey_record_);
  const QString select_key =
      hotkeys_ ? hotkeys_->Label(A::SelectWindow) : HotkeyLabel(hotkey_select_window_);
  const QString highlight_key =
      hotkeys_ ? hotkeys_->Label(A::HighlightWindow) : HotkeyLabel(hotkey_highlight_window_);
  const QString add_key = hotkeys_ ? hotkeys_->Label(A::AddKey) : HotkeyLabel(hotkey_add_key_);
  const QString add_mouse =
      hotkeys_ ? hotkeys_->Label(A::AddMouse) : HotkeyLabel(hotkey_add_mouse_);
  const QString clear_key = hotkeys_ ? hotkeys_->Label(A::Clear) : HotkeyLabel(hotkey_clear_);
  const QString undo_key = hotkeys_ ? hotkeys_->Label(A::Undo) : HotkeyLabel(hotkey_undo_);
  const QString redo_key = hotkeys_ ? hotkeys_->Label(A::Redo) : HotkeyLabel(hotkey_redo_);

  auto tip_with = [](const QString& tip, const QString& key) {
    return key.isEmpty() ? tip : tip + QStringLiteral("\n快捷键：%1").arg(key);
  };

  if (select_btn_) {
    select_btn_->setText(ButtonTextWithHotkey(QStringLiteral("选择窗口"), select_key));
    select_btn_->setToolTip(tip_with(QStringLiteral("点选要操作的目标窗口。"), select_key));
  }
  if (highlight_btn_) {
    highlight_btn_->setText(ButtonTextWithHotkey(QStringLiteral("凸显所选窗口"), highlight_key));
    highlight_btn_->setToolTip(
        tip_with(QStringLiteral("用红色边框闪烁提示当前已选目标窗口。"), highlight_key));
  }
  if (add_key_btn_) {
    add_key_btn_->setText(ButtonTextWithHotkey(QStringLiteral("按键行"), add_key));
    add_key_btn_->setToolTip(tip_with(QStringLiteral("添加一行按键动作。"), add_key));
  }
  if (add_mouse_btn_) {
    add_mouse_btn_->setText(ButtonTextWithHotkey(QStringLiteral("鼠标行"), add_mouse));
    add_mouse_btn_->setToolTip(tip_with(QStringLiteral("添加一行鼠标动作。"), add_mouse));
  }
  if (clear_btn_) {
    clear_btn_->setText(ButtonTextWithHotkey(QStringLiteral("清空"), clear_key));
    clear_btn_->setToolTip(tip_with(QStringLiteral("清空全部动作行。"), clear_key));
  }
  if (undo_btn_) {
    undo_btn_->setText(QStringLiteral("撤销"));
    undo_btn_->setToolTip(tip_with(QStringLiteral("撤销动作行修改。"), undo_key));
  }
  if (redo_btn_) {
    redo_btn_->setText(QStringLiteral("重做"));
    redo_btn_->setToolTip(tip_with(QStringLiteral("重做动作行修改。"), redo_key));
  }
  UpdateUndoRedoButtons();
  if (start_btn_) {
    const bool running = executor_ && executor_->IsRunning();
    if (running) {
      start_btn_->setText(ButtonTextWithHotkey(QStringLiteral("停止"), stop_key));
      start_btn_->setToolTip(tip_with(QStringLiteral("停止运行并显示窗口"), stop_key));
      start_btn_->setIcon(QIcon(QStringLiteral(":/png/wr_stop.png")));
    } else {
      start_btn_->setText(ButtonTextWithHotkey(QStringLiteral("开始"), start_key));
      start_btn_->setToolTip(tip_with(
          QStringLiteral("开始运行：顺序则先执行第一行再等待；"
                         "独立则各行先执行一遍再按各自间隔循环。"),
          start_key));
      start_btn_->setIcon(QIcon(QStringLiteral(":/png/wr_play.png")));
    }
  }
  if (start_rec_btn_) {
    if (recording_) {
      start_rec_btn_->setText(ButtonTextWithHotkey(QStringLiteral("停止录制"), record_key));
      start_rec_btn_->setToolTip(tip_with(
          QStringLiteral(
              "再按同一快捷键（或点此按钮）结束录制：\n"
              "• 恢复本窗口，并把录到的操作转成可编辑动作行\n"
              "• 行间隔 = 做完该动作后到下一动作的等待（与回放一致）\n"
              "• 可选择追加到现有列表，或替换全部行\n"
              "• 导入后运行模式会切到「顺序运行」"),
          record_key));
      start_rec_btn_->setIcon(QIcon(QStringLiteral(":/png/wr_stop.png")));
    } else {
      start_rec_btn_->setText(ButtonTextWithHotkey(QStringLiteral("开始录制"), record_key));
      start_rec_btn_->setToolTip(tip_with(
          QStringLiteral(
              "在目标窗口内录制键鼠，生成可编辑动作行。\n"
              "• 请先选择窗口\n"
              "• 开始后本窗口最小化；请把目标窗置于前台操作\n"
              "• 只记录目标窗口内的按键 / 点击 / 滚轮（移动不记）\n"
              "• 再按同一快捷键停止；间隔按「做完再等」写入各行"),
          record_key));
      start_rec_btn_->setIcon(QIcon(QStringLiteral(":/png/wr_record.png")));
    }
  }
  if (bulk_add_btn_) {
    const bool busy = recording_ || (executor_ && executor_->IsRunning());
    bulk_add_btn_->setEnabled(!busy);
  }
  UpdateEditActionButtonsEnabled();
}

void KeyPresserNextWindow::SyncHotkeyDialogFromSettings() {
  if (!hotkey_dialog_) {
    return;
  }
  using A = GlobalHotkeys::Action;
  // 加载时去掉冲突项（保留靠前的：运行 > 录制 > …）
  // 注意：不可命名为 slots，会与 Qt 的 slots 宏冲突
  hotkey_stop_ = hotkey_start_;
  hotkey_stop_record_ = hotkey_record_;
  QString* hotkey_ptrs[] = {&hotkey_start_,
                            &hotkey_record_,
                            &hotkey_select_window_,
                            &hotkey_highlight_window_,
                            &hotkey_add_key_,
                            &hotkey_add_mouse_,
                            &hotkey_clear_,
                            &hotkey_undo_,
                            &hotkey_redo_};
  constexpr int kHotkeyCount = 9;
  for (int i = 0; i < kHotkeyCount; ++i) {
    if (hotkey_ptrs[i]->trimmed().isEmpty()) {
      continue;
    }
    for (int j = i + 1; j < kHotkeyCount; ++j) {
      if (GlobalHotkeys::BindingsEqual(*hotkey_ptrs[i], *hotkey_ptrs[j])) {
        hotkey_ptrs[j]->clear();
      }
    }
  }
  hotkey_stop_ = hotkey_start_;
  hotkey_dialog_->blockSignals(true);
  hotkey_dialog_->SetStorage(A::Start, hotkey_start_);
  hotkey_dialog_->SetStorage(A::Record, hotkey_record_);
  hotkey_dialog_->SetStorage(A::SelectWindow, hotkey_select_window_);
  hotkey_dialog_->SetStorage(A::HighlightWindow, hotkey_highlight_window_);
  hotkey_dialog_->SetStorage(A::AddKey, hotkey_add_key_);
  hotkey_dialog_->SetStorage(A::AddMouse, hotkey_add_mouse_);
  hotkey_dialog_->SetStorage(A::Clear, hotkey_clear_);
  hotkey_dialog_->SetStorage(A::Undo, hotkey_undo_);
  hotkey_dialog_->SetStorage(A::Redo, hotkey_redo_);
  hotkey_dialog_->blockSignals(false);
}

void KeyPresserNextWindow::OnStart() {
  if (recording_) {
    return;
  }
  CancelPickPointHook();
  // 与测试截图 / 动作行截图相同：按当前截图模式 Bind，运行时 FindPic 同源截屏
  QString error;
  if (!EnsureBoundForCapture(&error)) {
    QMessageBox::warning(this, QStringLiteral("运行"), error);
    return;
  }
  AppSettings s = CollectSettingsFromUi();
  QStringList empty_key_rows;
  QStringList empty_pic_rows;
  for (int i = 0; i < s.rows.size(); ++i) {
    const ActionRow& row = s.rows[i];
    if (!row.enabled) {
      continue;
    }
    if (row.kind == ActionKind::Key &&
        !KeyBinding::fromDisplayText(row.shortcut).isValid()) {
      empty_key_rows.append(QString::number(i + 1));
    }
    if (row.kind == ActionKind::Mouse && row.target.mode == TargetMode::FindPic &&
        (row.target.image_path.isEmpty() || !QFileInfo::exists(row.target.image_path))) {
      empty_pic_rows.append(QString::number(i + 1));
    }
  }
  if (!empty_key_rows.isEmpty()) {
    QMessageBox::warning(
        this, QStringLiteral("运行"),
        QStringLiteral("第 %1 行未设置按键，请设置后再开始。")
            .arg(empty_key_rows.join(QStringLiteral("、"))));
    return;
  }
  if (!empty_pic_rows.isEmpty()) {
    QMessageBox::warning(
        this, QStringLiteral("运行"),
        QStringLiteral("第 %1 行鼠标识图未设置截图，请截图后再开始。")
            .arg(empty_pic_rows.join(QStringLiteral("、"))));
    return;
  }
  NotifyStatus(
      QStringLiteral("开始运行（截图 %1）").arg(CurrentDisplay()), 3000);
  const QVector<ActionRow> rows = s.rows;
  const QString mode = s.window.run_mode;
  const int run_count = s.window.run_count;
  const int gap = s.window.run_loop_interval_ms;
  const double speed = s.window.run_speed_percent / 100.0;
  const bool topmost = s.window.topmost;
  QMetaObject::invokeMethod(
      executor_,
      [ex = executor_, rows, mode, run_count, gap, speed, topmost]() {
        ex->ConfigureAndStart(rows, mode, run_count, gap, speed, topmost);
      },
      Qt::QueuedConnection);
}

void KeyPresserNextWindow::OnStop() {
  if (recording_) {
    OnStopRecord();
    return;
  }
  if (executor_) {
    QMetaObject::invokeMethod(executor_, "Stop", Qt::QueuedConnection);
  }
}

void KeyPresserNextWindow::OnRunningChanged(bool running) {
  SetRowsEditingEnabled(!running);
  SetBindControlsEnabled(!running && !recording_);
  SyncHotkeyRestrictMode();
  UpdateEditActionButtonsEnabled();
  if (run_count_spin_) {
    run_count_spin_->setEnabled(!running);
  }
  if (run_gap_spin_) {
    run_gap_spin_->setEnabled(!running);
  }
  if (run_speed_spin_) {
    run_speed_spin_->setEnabled(!running);
  }
  status_label_->setText(running ? QStringLiteral("运行中…") : QStringLiteral("已停止"));
  UpdateActionButtonTexts();
  if (running) {
    ShowStopHotkeyHint(QStringLiteral("运行中"));
    showMinimized();
  } else if (!recording_) {
    HideStopHotkeyHint();
    RestoreMainWindow();
  }
}

void KeyPresserNextWindow::OnSaveKpns() {
  if (kpns_path_.isEmpty()) {
    OnSaveKpnsAs();
    return;
  }
  const QString path_before = kpns_path_;
  if (!SaveExistingWithWindowCheck()) {
    return;
  }
  // 另存为时 OnSaveKpnsAs 已提示；覆盖原文件时在此提示
  if (kpns_path_ == path_before) {
    NotifyStatus(QStringLiteral("已保存 %1").arg(kpns_path_), 4000);
  }
}

void KeyPresserNextWindow::OnSaveKpnsAs() {
  const QString suggested =
      SuggestedKpnsSavePath(kpns_path_, target_hwnd_, window_label_);
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("另存为"), suggested, QStringLiteral("KeyPresserNext (*.kpn)"));
  if (path.isEmpty()) {
    return;
  }
  if (!SaveToPath(path)) {
    return;
  }
  NotifyStatus(QStringLiteral("已保存 %1").arg(path), 4000);
}

bool KeyPresserNextWindow::SaveExistingWithWindowCheck() {
  if (kpns_path_.isEmpty()) {
    return false;
  }

  QString file_win;
  QString cur_win;
  if (WindowNameMismatchesKpnsFile(kpns_path_, target_hwnd_, window_label_, &file_win,
                                   &cur_win)) {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("窗口名不一致"));
    box.setText(QStringLiteral("当前窗口与脚本文件名中的窗口不一致。"));
    box.setInformativeText(
        QStringLiteral("文件名中的窗口：%1\n当前窗口：%2\n\n"
                       "覆盖：仍写入「%3」\n"
                       "另存为：按当前窗口生成新文件名，避免混用。")
            .arg(file_win, cur_win, QFileInfo(kpns_path_).fileName()));
    auto* overwrite_btn =
        box.addButton(QStringLiteral("覆盖原文件"), QMessageBox::AcceptRole);
    auto* save_as_btn = box.addButton(QStringLiteral("另存为…"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(save_as_btn);
    box.exec();

    if (box.clickedButton() == save_as_btn) {
      OnSaveKpnsAs();
      return !settings_dirty_;
    }
    if (box.clickedButton() != overwrite_btn) {
      return false;
    }
  }

  return SaveToPath(kpns_path_);
}

void KeyPresserNextWindow::OnNewKpns() {
  if (!PromptSaveBeforeClose()) {
    return;
  }
  ApplySettingsToUi(AppSettings{});
  SetCurrentFile(QString(), false);
  NotifyStatus(QStringLiteral("已新建"), 3000);
}

void KeyPresserNextWindow::OnOpenKpns() {
  if (!PromptSaveBeforeClose()) {
    return;
  }
  const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开"), QString(),
                                                    QStringLiteral("KeyPresserNext (*.kpn)"));
  if (path.isEmpty()) {
    return;
  }
  OpenFromPath(path);
}

bool KeyPresserNextWindow::SaveToPath(const QString& path) {
  AppSettings s = CollectSettingsFromUi();
  QString err;
  if (!SettingsStore::SaveToKpns(path, &s, &err)) {
    QMessageBox::warning(this, QStringLiteral("保存失败"), err);
    return false;
  }
  SetCurrentFile(path, false);
  SettingsStore::RememberOpenedFile(path);
  RebuildRecentMenu();
  return true;
}

bool KeyPresserNextWindow::OpenFromPath(const QString& path, bool force_open_duplicate) {
  const QString abs = NormalizeKpnPath(path);
  if (abs.isEmpty() || !QFileInfo::exists(abs)) {
    QMessageBox::warning(this, QStringLiteral("打开失败"),
                         QStringLiteral("文件不存在：\n%1").arg(path));
    return false;
  }
  if (!kpns_path_.isEmpty() && NormalizeKpnPath(kpns_path_) == abs) {
    RaiseMainWindow();
    return true;
  }
  if (!force_open_duplicate && IsKpnOpenInAnotherInstance(abs)) {
    switch (PromptKpnAlreadyOpen(this, abs)) {
      case KpnOpenConflictResult::SwitchToExisting:
        ActivateExistingKpnInstance(abs);
        NotifyStatus(
            QStringLiteral("已切换到已打开该文件的窗口：%1").arg(QFileInfo(abs).fileName()),
            4000);
        return true;
      case KpnOpenConflictResult::Cancel:
        return false;
      case KpnOpenConflictResult::OpenAnotherCopy:
        break;
    }
  }
  AppSettings s;
  QString err;
  if (!SettingsStore::LoadFromKpns(abs, &s, &err)) {
    QMessageBox::warning(this, QStringLiteral("打开失败"), err);
    return false;
  }
  if (!err.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("提示"), err);
  }
  ApplySettingsToUi(s);
  SetCurrentFile(abs, false);
  SettingsStore::RememberOpenedFile(abs);
  RebuildRecentMenu();
  NotifyStatus(QStringLiteral("已打开 %1").arg(abs), 4000);
  return true;
}

void KeyPresserNextWindow::RaiseMainWindow() {
  WindowSelectHelper::forceBringToFront(this);
}

void KeyPresserNextWindow::SetCurrentFile(const QString& path, bool dirty) {
  kpns_path_ = path;
  settings_dirty_ = dirty;
  if (kpn_host_) {
    if (path.isEmpty()) {
      kpn_host_->clear();
    } else {
      kpn_host_->listenFor(path);
    }
  }
  UpdateWindowTitle();
}

void KeyPresserNextWindow::UpdateWindowTitle() {
  // 有文件时标题栏以文件名为主（应用名已由图标表达），避免过长压菜单
  QString title;
  if (!kpns_path_.isEmpty()) {
    title = QFileInfo(kpns_path_).fileName();
  } else {
    title = AppProductName();
  }
  if (settings_dirty_) {
    title += QStringLiteral(" *");
  }
  setWindowTitle(title);
}

bool KeyPresserNextWindow::PromptSaveBeforeClose() {
  if (kpns_path_.isEmpty()) {
    const auto ret = QMessageBox::question(
        this, QStringLiteral("保存设置"), QStringLiteral("是否保存当前设置？"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (ret == QMessageBox::Cancel) {
      return false;
    }
    if (ret == QMessageBox::Yes) {
      OnSaveKpnsAs();
      return !kpns_path_.isEmpty();
    }
    return true;
  }
  if (!settings_dirty_) {
    return true;
  }
  const auto ret = QMessageBox::question(
      this, QStringLiteral("保存设置"),
      QStringLiteral("文件「%1」已修改，是否保存？").arg(QFileInfo(kpns_path_).fileName()),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
  if (ret == QMessageBox::Cancel) {
    return false;
  }
  if (ret == QMessageBox::Yes) {
    const QString path_before = kpns_path_;
    if (!SaveExistingWithWindowCheck()) {
      return false;
    }
    if (kpns_path_ == path_before) {
      NotifyStatus(QStringLiteral("已保存 %1").arg(kpns_path_), 4000);
    }
    return true;
  }
  return true;
}

void KeyPresserNextWindow::OnAbout() {
  QMessageBox about(this);
  about.setWindowTitle(QStringLiteral("关于 %1").arg(AppProductName()));
  about.setIcon(QMessageBox::Information);
  about.setText(QStringLiteral("%1 %2\n录得下，改得动，跑得稳！您的自动化键鼠助手！\n\n"
                               "开源社区版 · MIT License")
                    .arg(AppProductName(), AppVersion()));
  about.setInformativeText(QString());
  auto* site_btn = about.addButton(QStringLiteral("访问官网"), QMessageBox::AcceptRole);
  about.addButton(QMessageBox::Close);
  about.exec();
  if (about.clickedButton() == site_btn) {
    OnOpenOfficialSite();
  }
}

void KeyPresserNextWindow::OnShowChangelog() {
  Changelog::ShowDialog(this, true);
}

void KeyPresserNextWindow::OnOpenOfficialSite() {
  LayoutMetrics::OpenPrimaryRef();
}

void KeyPresserNextWindow::OnRowScreenshot(ActionRowWidget* row) {
  // 与「测试截图 / 开始运行」相同：当前截图模式下 Bind + Capture 客户区
  QString error;
  if (!EnsureBoundForCapture(&error)) {
    QMessageBox::warning(this, QStringLiteral("截图"), error);
    return;
  }
  KeepTargetTopmostIfNeeded();

  int w = 0;
  int h = 0;
  if (!core_.GetClientSize(target_hwnd_, &w, &h) || w <= 0 || h <= 0) {
    QMessageBox::warning(this, QStringLiteral("截图"),
                         QStringLiteral("无法获取窗口客户区尺寸"));
    return;
  }

  const QString dir = SettingsStore::EnsureImagesDir();
  const QString full_path = QDir(dir).filePath(
      QStringLiteral("row_capture_%1.bmp")
          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmsszzz"))));
  if (!core_.Capture(0, 0, w, h, full_path) || !QFileInfo::exists(full_path)) {
    QMessageBox::warning(
        this, QStringLiteral("截图"),
        QStringLiteral("截图失败（模式：%1）\n%2")
            .arg(CurrentDisplay(), core_.LastError().isEmpty()
                                       ? QStringLiteral("请检查窗口是否可见、截图方式是否匹配")
                                       : core_.LastError()));
    return;
  }

  const QPixmap pix(full_path);
  if (pix.isNull()) {
    QMessageBox::warning(this, QStringLiteral("截图"),
                         QStringLiteral("截图文件无法打开：\n%1").arg(full_path));
    return;
  }

  auto* widget = new ScreenshotWidget();
  widget->setPixmap(pix);
  // 取消框选时清理临时全图；确认后在保存模板时删除
  connect(widget->m_cancelButton, &QPushButton::clicked, this, [full_path]() {
    QFile::remove(full_path);
  });
  connect(widget, &ScreenshotWidget::screenshotSelected, this,
          [this, row, widget, full_path](const QRect& /*rect*/) {
            // ScreenshotWidget::onScreenshotSelected 已把框选结果写入 m_screenshotImage，
            // 切勿再用全图像素坐标二次 copy（会越界填黑）。
            QImage img = widget->m_screenshotImage;
            if (img.isNull()) {
              QFile::remove(full_path);
              return;
            }
            const QString dir = SettingsStore::EnsureImagesDir();
            const QString path =
                dir + QStringLiteral("/shot_%1.png")
                          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmsszzz")));
            if (!img.save(path, "PNG")) {
              // 回退 BMP，避免个别环境 PNG 插件异常
              const QString bmp =
                  dir + QStringLiteral("/shot_%1.bmp")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmsszzz")));
              if (!img.save(bmp, "BMP")) {
                QFile::remove(full_path);
                return;
              }
              row->SetImagePath(bmp);
            } else {
              row->SetImagePath(path);
            }
            QFile::remove(full_path);
          });
  widget->showNaturalSize();
}

void KeyPresserNextWindow::OnRowPickPoint(ActionRowWidget* row) {
  StartPickPoint(row);
}

void KeyPresserNextWindow::AppendLog(const QString& msg) {
  if (msg.isEmpty()) {
    return;
  }
  last_log_line_ = msg;
  if (!log_edit_ || log_collapsed_) {
    return;
  }
  const QString line =
      QStringLiteral("[%1] %2")
          .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")), msg);
  pending_log_lines_.append(line);
  if (log_flush_timer_ && !log_flush_timer_->isActive()) {
    log_flush_timer_->start();
  }
}

void KeyPresserNextWindow::FlushPendingLogs() {
  if (!log_edit_ || pending_log_lines_.isEmpty() || log_collapsed_) {
    pending_log_lines_.clear();
    return;
  }
  log_edit_->appendPlainText(pending_log_lines_.join(QLatin1Char('\n')));
  pending_log_lines_.clear();
  QTextCursor cursor = log_edit_->textCursor();
  cursor.movePosition(QTextCursor::End);
  log_edit_->setTextCursor(cursor);
}

void KeyPresserNextWindow::SetLogCollapsed(bool collapsed) {
  if (!log_edit_ || !log_panel_ || !bottom_panel_) {
    return;
  }
  const bool changing = (log_collapsed_ != collapsed);
  log_collapsed_ = collapsed;

  log_edit_->setVisible(!collapsed);
  if (clear_log_btn_) {
    clear_log_btn_->setVisible(!collapsed);
  }
  if (log_toggle_btn_) {
    log_toggle_btn_->setText(collapsed ? QStringLiteral("▸") : QStringLiteral("▾"));
    log_toggle_btn_->setToolTip(collapsed ? QStringLiteral("展开日志")
                                          : QStringLiteral("折叠日志"));
  }

  if (collapsed) {
    pending_log_lines_.clear();
    if (log_flush_timer_) {
      log_flush_timer_->stop();
    }
    if (changing && main_split_) {
      log_split_sizes_ = main_split_->sizes();
    }
    log_edit_->setMinimumHeight(0);
    log_panel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    log_panel_->setMaximumHeight(34);
    bottom_panel_->setMinimumHeight(52);
    bottom_panel_->setMaximumHeight(92);
    if (main_split_) {
      const QList<int> cur = main_split_->sizes();
      const int total = (cur.value(0) + cur.value(1));
      const int bottom = 72;
      if (total > bottom) {
        main_split_->setSizes({total - bottom, bottom});
      }
    }
  } else {
    FlushPendingLogs();
    log_panel_->setMaximumHeight(QWIDGETSIZE_MAX);
    log_panel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    log_edit_->setMinimumHeight(64);
    bottom_panel_->setMaximumHeight(QWIDGETSIZE_MAX);
    bottom_panel_->setMinimumHeight(140);
    if (main_split_ && !log_split_sizes_.isEmpty()) {
      main_split_->setSizes(log_split_sizes_);
      log_split_sizes_.clear();
    } else if (main_split_ && changing) {
      const QList<int> cur = main_split_->sizes();
      const int total = cur.value(0) + cur.value(1);
      if (total > 200) {
        main_split_->setSizes({total - 200, 200});
      }
    }
  }

  QSettings settings;
  settings.setValue(QStringLiteral("ui/logCollapsed"), collapsed);
}

bool KeyPresserNextWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched && watched->objectName() == QLatin1String("logTitle") &&
      event->type() == QEvent::MouseButtonRelease) {
    const auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::LeftButton) {
      SetLogCollapsed(!log_collapsed_);
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void KeyPresserNextWindow::NotifyStatus(const QString& msg, int timeoutMs, bool appendLog) {
  statusBar()->showMessage(msg, timeoutMs);
  if (appendLog) {
    AppendLog(msg);
  }
}

void KeyPresserNextWindow::MarkDirty() {
  if (loading_settings_) {
    return;
  }
  if (!settings_dirty_) {
    settings_dirty_ = true;
    UpdateWindowTitle();
  }
}

void KeyPresserNextWindow::closeEvent(QCloseEvent* event) {
  if (!PromptSaveBeforeClose()) {
    event->ignore();
    return;
  }
  if (recording_) {
    UninstallRecordHooks();
    recording_ = false;
  }
  HideStopHotkeyHint();
  if (executor_) {
    QMetaObject::invokeMethod(executor_, "Stop", Qt::BlockingQueuedConnection);
  }
  CancelPickPointHook();
  UninstallRecordHooks();
  QMainWindow::closeEvent(event);
}

void KeyPresserNextWindow::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);
  RefreshWindowTitleLabel();
}

QString KeyPresserNextWindow::StopHotkeyLabel() const {
  if (hotkeys_) {
    const QString label = hotkeys_->Label(GlobalHotkeys::Action::Start);
    if (!label.isEmpty()) {
      return label;
    }
  }
  return hotkey_start_.isEmpty() ? QStringLiteral("F9") : HotkeyLabel(hotkey_start_);
}

void KeyPresserNextWindow::ShowStopHotkeyHint(const QString& mode_text) {
  if (mode_text == QString("录制中")) {
    const QString key =
        hotkeys_ ? hotkeys_->Label(GlobalHotkeys::Action::Record) : HotkeyLabel(hotkey_record_);
    StopHotkeyHint::show(mode_text, key.isEmpty() ? StopHotkeyLabel() : key);
    return;
  }
  StopHotkeyHint::show(mode_text, StopHotkeyLabel());
}

void KeyPresserNextWindow::HideStopHotkeyHint() { StopHotkeyHint::hide(); }

void KeyPresserNextWindow::RestoreMainWindow() {
  if (isMinimized()) {
    showNormal();
  }
  WindowSelectHelper::forceBringToFront(this);
}

void KeyPresserNextWindow::OnStartRecord() {
  if (recording_) {
    return;
  }
  if (!target_hwnd_ || !IsWindow(target_hwnd_)) {
    QMessageBox::warning(this, QStringLiteral("录制"), QStringLiteral("请先选择窗口"));
    return;
  }
  CancelPickPointHook();
  // 录制时先解绑，避免 BindWindow 与全局钩子互相干扰（与旧版一致）
  if (core_.IsBound()) {
    core_.UnBindWindow();
  }
  record_events_.clear();
  last_record_tick_ = QDateTime::currentMSecsSinceEpoch();
  InstallRecordHooks();
  recording_ = true;
  SetBindControlsEnabled(false);
  SyncHotkeyRestrictMode();
  UpdateEditActionButtonsEnabled();
  UpdateActionButtonTexts();
  ShowStopHotkeyHint(QStringLiteral("录制中"));
  showMinimized();
  NotifyStatus(
      QStringLiteral("录制中：请在目标窗口内操作（仅记录该窗口内的键鼠）；按停止键结束并显示窗口"));
}

void KeyPresserNextWindow::OnStopRecord() {
  if (!recording_) {
    return;
  }
  UninstallRecordHooks();
  recording_ = false;
  SetBindControlsEnabled(!(executor_ && executor_->IsRunning()));
  SyncHotkeyRestrictMode();
  UpdateEditActionButtonsEnabled();
  UpdateActionButtonTexts();
  HideStopHotkeyHint();
  RestoreMainWindow();
  const QVector<ActionRow> converted = RecordConverter::Convert(record_events_);
  if (converted.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("录制"),
                             QStringLiteral("未录到目标窗口内的操作"));
    if (target_hwnd_) {
      EnsureBound();
    }
    return;
  }
  const auto reply = QMessageBox::question(
      this, QStringLiteral("录制转行"),
      QStringLiteral("将生成 %1 行。追加到现有列表？\n选「否」则替换全部行。")
          .arg(converted.size()),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
  if (reply == QMessageBox::Cancel) {
    if (target_hwnd_) {
      EnsureBound();
    }
    return;
  }
  AppendRows(converted, reply == QMessageBox::Yes);
  // 录制结束导入后默认切到顺序运行（多步操作按列表执行）
  const int seq = run_mode_combo_->findData(QStringLiteral("sequential"));
  if (seq >= 0) {
    run_mode_combo_->setCurrentIndex(seq);
  }
  if (target_hwnd_) {
    EnsureBound();
  }
  NotifyStatus(QStringLiteral("录制已转为 %1 行").arg(converted.size()), 4000);
}

void KeyPresserNextWindow::InstallRecordHooks() {
  record_instance_ = this;
  if (!record_kb_hook_) {
    record_kb_hook_ =
        SetWindowsHookExW(WH_KEYBOARD_LL, RecordKeyboardProc, GetModuleHandleW(nullptr), 0);
  }
  if (!record_mouse_hook_) {
    record_mouse_hook_ =
        SetWindowsHookExW(WH_MOUSE_LL, RecordMouseProc, GetModuleHandleW(nullptr), 0);
  }
}

void KeyPresserNextWindow::UninstallRecordHooks() {
  if (record_kb_hook_) {
    UnhookWindowsHookEx(record_kb_hook_);
    record_kb_hook_ = nullptr;
  }
  if (record_mouse_hook_) {
    UnhookWindowsHookEx(record_mouse_hook_);
    record_mouse_hook_ = nullptr;
  }
  if (record_instance_ == this) {
    record_instance_ = nullptr;
  }
}

void KeyPresserNextWindow::AppendRecordEvent(const RecordEvent& ev) {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  RecordEvent timed = ev;
  timed.delay_ms = static_cast<int>(now - last_record_tick_);
  last_record_tick_ = now;
  record_events_.append(timed);
}

bool KeyPresserNextWindow::BelongsToTarget(HWND hwnd) const {
  return WindowCoordHelper::BelongsToTarget(target_hwnd_, hwnd);
}

bool KeyPresserNextWindow::IsTargetForeground() const {
  return BelongsToTarget(GetForegroundWindow());
}

bool KeyPresserNextWindow::IsRecordHotkeyVk(int vk) const {
  return hotkeys_ && hotkeys_->IsConfiguredHotkeyVk(vk);
}

bool KeyPresserNextWindow::ScreenToClientInTarget(int screen_x, int screen_y, int* client_x,
                                                   int* client_y) const {
  return WindowCoordHelper::ScreenToClientDpiAware(target_hwnd_, screen_x, screen_y, client_x,
                                                   client_y);
}

LRESULT CALLBACK KeyPresserNextWindow::RecordKeyboardProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code == HC_ACTION && record_instance_ && record_instance_->recording_) {
    const auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
    const int vk = static_cast<int>(kb->vkCode);
    if (!record_instance_->IsRecordHotkeyVk(vk) && record_instance_->IsTargetForeground()) {
      RecordEvent ev;
      ev.vk = vk;
      if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
        ev.type = QStringLiteral("key_down");
        record_instance_->AppendRecordEvent(ev);
      } else if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP) {
        ev.type = QStringLiteral("key_up");
        record_instance_->AppendRecordEvent(ev);
      }
    }
  }
  return CallNextHookEx(record_instance_ ? record_instance_->record_kb_hook_ : nullptr, code,
                        wparam, lparam);
}

LRESULT CALLBACK KeyPresserNextWindow::RecordMouseProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code == HC_ACTION && record_instance_ && record_instance_->recording_) {
    const auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lparam);
    int cx = 0;
    int cy = 0;
    if (record_instance_->ScreenToClientInTarget(ms->pt.x, ms->pt.y, &cx, &cy)) {
      RecordEvent ev;
      ev.x = cx;
      ev.y = cy;
      switch (wparam) {
        case WM_LBUTTONDOWN:
          ev.type = QStringLiteral("mouse_down");
          ev.button = QStringLiteral("left");
          record_instance_->AppendRecordEvent(ev);
          break;
        case WM_LBUTTONUP:
          ev.type = QStringLiteral("mouse_up");
          ev.button = QStringLiteral("left");
          record_instance_->AppendRecordEvent(ev);
          break;
        case WM_RBUTTONDOWN:
          ev.type = QStringLiteral("mouse_down");
          ev.button = QStringLiteral("right");
          record_instance_->AppendRecordEvent(ev);
          break;
        case WM_RBUTTONUP:
          ev.type = QStringLiteral("mouse_up");
          ev.button = QStringLiteral("right");
          record_instance_->AppendRecordEvent(ev);
          break;
        case WM_MBUTTONDOWN:
          ev.type = QStringLiteral("mouse_down");
          ev.button = QStringLiteral("middle");
          record_instance_->AppendRecordEvent(ev);
          break;
        case WM_MBUTTONUP:
          ev.type = QStringLiteral("mouse_up");
          ev.button = QStringLiteral("middle");
          record_instance_->AppendRecordEvent(ev);
          break;
        case WM_MOUSEWHEEL: {
          ev.type = QStringLiteral("mouse_wheel");
          const short delta = static_cast<short>(HIWORD(ms->mouseData));
          ev.wheel = delta / WHEEL_DELTA;
          if (ev.wheel == 0) {
            ev.wheel = (delta > 0) ? 1 : -1;
          }
          record_instance_->AppendRecordEvent(ev);
          break;
        }
        default:
          break;
      }
    }
  }
  return CallNextHookEx(record_instance_ ? record_instance_->record_mouse_hook_ : nullptr, code,
                        wparam, lparam);
}
