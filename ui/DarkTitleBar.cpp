#include "DarkTitleBar.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>

namespace {

bool HasCaptionIconFont() {
  const QStringList families = {
      QStringLiteral("Segoe Fluent Icons"),
      QStringLiteral("Segoe MDL2 Assets"),
  };
  for (const QString& name : families) {
    if (!QFontDatabase().families().filter(name, Qt::CaseInsensitive).isEmpty()) {
      return true;
    }
  }
  return false;
}

QFont CaptionIconFont() {
  // Win10/11 标题栏图标字体；字号略大于系统默认，更易辨认
  const QStringList families = {
      QStringLiteral("Segoe Fluent Icons"),
      QStringLiteral("Segoe MDL2 Assets"),
  };
  for (const QString& name : families) {
    if (!QFontDatabase().families().filter(name, Qt::CaseInsensitive).isEmpty()) {
      QFont f(name);
      f.setPixelSize(12);
      return f;
    }
  }
  QFont f(QStringLiteral("Segoe UI"));
  f.setPixelSize(16);
  f.setBold(true);
  return f;
}

// Segoe MDL2 / Fluent Icons（无该字体时用普通 Unicode）
QChar GlyphMin() { return HasCaptionIconFont() ? QChar(0xE921) : QChar(0x2013); }
QChar GlyphMax() { return HasCaptionIconFont() ? QChar(0xE922) : QChar(0x25A1); }
QChar GlyphRestore() { return HasCaptionIconFont() ? QChar(0xE923) : QChar(0x25A3); }
QChar GlyphClose() { return HasCaptionIconFont() ? QChar(0xE8BB) : QChar(0x00D7); }

}  // namespace

DarkTitleBar::DarkTitleBar(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("darkTitleBar"));
  setFixedHeight(kHeight);
  setAttribute(Qt::WA_StyledBackground, true);
  // 宽度跟窗口走，不要用菜单栏的最小宽度把整窗撑开
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  auto* lay = new QHBoxLayout(this);
  lay->setContentsMargins(6, 0, 0, 0);
  lay->setSpacing(0);

  icon_label_ = new QLabel(this);
  icon_label_->setObjectName(QStringLiteral("titleIcon"));
  icon_label_->setFixedSize(16, 16);
  icon_label_->setScaledContents(true);

  // 标题叠在正中（Cursor 风格），不参与左右布局，绝不撑开窗口
  title_label_ = new QLabel(this);
  title_label_->setObjectName(QStringLiteral("titleLabel"));
  title_label_->setAlignment(Qt::AlignCenter);
  title_label_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  title_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  title_label_->setMinimumSize(0, 0);

  const QFont icon_font = CaptionIconFont();
  auto make_sys = [this, &icon_font](const QString& name, QChar glyph) {
    auto* btn = new QPushButton(QString(glyph), this);
    btn->setObjectName(name);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setFlat(true);
    btn->setFixedSize(46, kHeight);
    btn->setFont(icon_font);
    btn->setProperty("system-button", true);
    return btn;
  };

  min_btn_ = make_sys(QStringLiteral("titleMinBtn"), GlyphMin());
  max_btn_ = make_sys(QStringLiteral("titleMaxBtn"), GlyphMax());
  max_btn_->setCheckable(true);
  close_btn_ = make_sys(QStringLiteral("titleCloseBtn"), GlyphClose());

  lay->addWidget(icon_label_, 0, Qt::AlignVCenter);
  lay->addStretch(1);
  lay->addWidget(min_btn_);
  lay->addWidget(max_btn_);
  lay->addWidget(close_btn_);

  connect(min_btn_, &QPushButton::clicked, this, &DarkTitleBar::minimizeRequested);
  connect(max_btn_, &QPushButton::clicked, this, [this](bool checked) {
    emit maximizeRequested(checked);
  });
  connect(close_btn_, &QPushButton::clicked, this, &DarkTitleBar::closeRequested);
}

void DarkTitleBar::TakeMenuBar(QMenuBar* menu_bar) {
  if (!menu_bar || menu_bar_ == menu_bar) {
    return;
  }
  menu_bar_ = menu_bar;
  menu_bar_->setParent(this);
  menu_bar_->setNativeMenuBar(false);
  menu_bar_->setObjectName(QStringLiteral("titleMenuBar"));
  // 菜单只按内容占位，允许窗口比「全部菜单展开宽度」更窄
  menu_bar_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  menu_bar_->setMinimumWidth(0);

  auto* lay = qobject_cast<QHBoxLayout*>(layout());
  if (!lay) {
    return;
  }
  // 图标右侧：菜单
  lay->insertWidget(1, menu_bar_, 0, Qt::AlignVCenter);
  RelayoutTitle();
  // 菜单首次布局后宽度才准确，再收一次标题区域
  QTimer::singleShot(0, this, [this]() { RelayoutTitle(); });
}

void DarkTitleBar::SetTitle(const QString& title) {
  title_text_ = title;
  RelayoutTitle();
}

void DarkTitleBar::SetIcon(const QIcon& icon) {
  icon_label_->setPixmap(icon.pixmap(16, 16));
}

void DarkTitleBar::SetMaximized(bool maximized) {
  max_btn_->blockSignals(true);
  max_btn_->setChecked(maximized);
  max_btn_->setText(QString(maximized ? GlyphRestore() : GlyphMax()));
  max_btn_->blockSignals(false);
}

void DarkTitleBar::SetButtonsVisible(bool minimize, bool maximize, bool close) {
  min_btn_->setVisible(minimize);
  max_btn_->setVisible(maximize);
  close_btn_->setVisible(close);
  RelayoutTitle();
}

QSize DarkTitleBar::sizeHint() const {
  return QSize(400, kHeight);
}

QSize DarkTitleBar::minimumSizeHint() const {
  // 仅图标 + 可见系统按钮；标题省略、菜单不计入，避免长文件名撑开窗口
  int w = 8 + 16 + 8;
  if (min_btn_ && min_btn_->isVisible()) {
    w += 46;
  }
  if (max_btn_ && max_btn_->isVisible()) {
    w += 46;
  }
  if (close_btn_ && close_btn_->isVisible()) {
    w += 46;
  }
  return QSize(w, kHeight);
}

void DarkTitleBar::RelayoutTitle() {
  if (!title_label_) {
    return;
  }

  // 标题只占用「菜单右侧 ～ 系统按钮左侧」的空隙，避免压住菜单
  int left = 8 + (icon_label_ ? icon_label_->width() : 0) + 4;
  if (menu_bar_ && menu_bar_->isVisible()) {
    const int menu_w = menu_bar_->width() > 0 ? menu_bar_->width()
                                              : menu_bar_->sizeHint().width();
    left += menu_w + 10;
  }
  int right = 8;
  if (min_btn_ && min_btn_->isVisible()) {
    right += min_btn_->width();
  }
  if (max_btn_ && max_btn_->isVisible()) {
    right += max_btn_->width();
  }
  if (close_btn_ && close_btn_->isVisible()) {
    right += close_btn_->width();
  }

  const int x = left;
  const int avail = qMax(0, width() - left - right);
  // 长文件名中间省略，两端（时间前缀 / 扩展名）更易辨认
  const QString elided =
      title_label_->fontMetrics().elidedText(title_text_, Qt::ElideMiddle, avail);
  title_label_->setText(elided);
  title_label_->setToolTip(title_text_ == elided ? QString() : title_text_);
  title_label_->setGeometry(x, 0, avail, height());
  title_label_->lower();  // 在菜单/按钮之下，避免挡住点击
}

void DarkTitleBar::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  RelayoutTitle();
}

void DarkTitleBar::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  RelayoutTitle();
}

void DarkTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && max_btn_->isVisible()) {
    emit maximizeRequested(!max_btn_->isChecked());
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}
