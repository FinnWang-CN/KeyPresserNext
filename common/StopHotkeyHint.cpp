#include "StopHotkeyHint.h"

#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>
#include <QWidget>
#include <windows.h>

namespace {

QWidget* g_overlay = nullptr;
QLabel* g_label = nullptr;
QString g_mode;
bool g_visible = false;

/** Qt 的 WA_TransparentForMouseEvents 挡不住其它进程；需 WS_EX_TRANSPARENT 真正鼠标穿透 */
void applyClickThrough(QWidget* w) {
  if (!w) {
    return;
  }
  const HWND hwnd = reinterpret_cast<HWND>(w->winId());
  if (!hwnd) {
    return;
  }
  const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                    ex | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
}

void ensureOverlay() {
  if (g_overlay) {
    return;
  }
  g_overlay = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint |
                                       Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
  g_overlay->setAttribute(Qt::WA_TranslucentBackground);
  g_overlay->setAttribute(Qt::WA_ShowWithoutActivating);
  g_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
  g_overlay->setAttribute(Qt::WA_QuitOnClose, false);
  g_label = new QLabel(g_overlay);
  g_label->setObjectName(QStringLiteral("stopHotkeyHint"));
  g_label->setAttribute(Qt::WA_TransparentForMouseEvents);
  g_label->setAttribute(Qt::WA_StyledBackground, true);
  g_label->setAlignment(Qt::AlignCenter);
  auto* lay = new QVBoxLayout(g_overlay);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->addWidget(g_label);
}

void applyText(const QString& modeText, const QString& hotkeyLabel) {
  ensureOverlay();
  g_mode = modeText;
  if (hotkeyLabel.isEmpty()) {
    g_label->setText(QStringLiteral("%1\n点击「停止」结束并显示窗口").arg(modeText));
  } else {
    g_label->setText(QStringLiteral("%1\n按 %2 停止并显示窗口").arg(modeText, hotkeyLabel));
  }
  g_overlay->adjustSize();

  QScreen* screen = QGuiApplication::primaryScreen();
  if (!screen) {
    return;
  }
  const QRect avail = screen->availableGeometry();
  const int margin = 28;
  g_overlay->move(avail.right() - g_overlay->width() - margin,
                  avail.bottom() - g_overlay->height() - margin);
}

}  // namespace

namespace StopHotkeyHint {

void show(const QString& modeText, const QString& hotkeyLabel) {
  applyText(modeText, hotkeyLabel);
  g_overlay->show();
  g_overlay->raise();
  applyClickThrough(g_overlay);
  g_visible = true;
}

void hide() {
  g_visible = false;
  g_mode.clear();
  if (g_overlay) {
    g_overlay->hide();
  }
}

void updateIfVisible(const QString& modeText, const QString& hotkeyLabel) {
  if (!g_visible) {
    return;
  }
  applyText(modeText, hotkeyLabel);
  applyClickThrough(g_overlay);
}

QWidget* overlayWidget() { return g_overlay; }

}  // namespace StopHotkeyHint
