#include "screenshotwidget.h"

#include <QCoreApplication>
#include <QDialog>
#include <QEventLoop>
#include <QFrame>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

ScreenshotWidget::ScreenshotWidget(QWidget* parent)
    : QWidget(parent),
      m_isSelecting(false),
      m_confirmButton(new QPushButton(QStringLiteral("确定"), this)),
      m_cancelButton(new QPushButton(QStringLiteral("取消"), this)) {
  setCursor(Qt::CrossCursor);
  setWindowTitle(QStringLiteral("请框选区域"));
  setWindowModality(Qt::ApplicationModal);
  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

  RelocateButtons();

  connect(m_confirmButton, &QPushButton::clicked, this, [this]() {
    emit screenshotSelected(QRect(m_startPoint, m_endPoint));
  });
  connect(this, &ScreenshotWidget::screenshotSelected, this,
          &ScreenshotWidget::onScreenshotSelected);
}

void ScreenshotWidget::RelocateButtons() {
  if (!m_confirmButton || !m_cancelButton) {
    return;
  }
  const int bw = 70;
  const int bh = 30;
  const int margin = 10;
  m_confirmButton->setGeometry(width() - bw * 2 - margin * 2, height() - bh - margin, bw, bh);
  m_cancelButton->setGeometry(width() - bw - margin, height() - bh - margin, bw, bh);
  m_confirmButton->raise();
  m_cancelButton->raise();
}

void ScreenshotWidget::setPixmap(const QPixmap& pixmap) {
  m_pixmap = pixmap;
  m_screenshotImage = QImage();  // 全图换了，旧框选结果作废
  if (!m_pixmap.isNull()) {
    setFixedSize(m_pixmap.size());
  }
  RelocateButtons();
  update();
}

void ScreenshotWidget::setHwnd(WId hwnd) {
  if (!hwnd) {
    return;
  }
  HWND h = reinterpret_cast<HWND>(hwnd);
  bool was_minimized = IsIconic(h);
  if (was_minimized) {
    ShowWindow(h, SW_RESTORE);
    QEventLoop loop;
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();
  }
  setPixmap(QPixmap::fromImage(captureWindow(h)));
  if (was_minimized) {
    ShowWindow(h, SW_MINIMIZE);
  }
}

void ScreenshotWidget::showNaturalSize() {
  if (m_pixmap.isNull()) {
    show();
    return;
  }

  setFixedSize(m_pixmap.size());
  RelocateButtons();

  QScreen* scr = QGuiApplication::screenAt(QCursor::pos());
  if (!scr) {
    scr = QGuiApplication::primaryScreen();
  }
  const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1280, 720);
  constexpr int kMargin = 24;
  const int max_w = qMax(200, avail.width() - kMargin * 2);
  const int max_h = qMax(160, avail.height() - kMargin * 2);
  const bool fits = m_pixmap.width() <= max_w && m_pixmap.height() <= max_h;

  auto* host = new QDialog(nullptr);
  host->setAttribute(Qt::WA_DeleteOnClose);
  host->setWindowTitle(windowTitle());
  host->setWindowModality(Qt::ApplicationModal);
  host->setWindowFlags((host->windowFlags() | Qt::WindowStaysOnTopHint) &
                       ~Qt::WindowContextHelpButtonHint);

  auto* root = new QVBoxLayout(host);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto* scroll = new QScrollArea(host);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(false);
  scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  if (fits) {
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }

  setParent(scroll);
  setWindowFlags(Qt::Widget);
  scroll->setWidget(this);
  root->addWidget(scroll);

  const int host_w = fits ? m_pixmap.width() : max_w;
  const int host_h = fits ? m_pixmap.height() : max_h;
  host->resize(host_w, host_h);
  host->move(avail.x() + (avail.width() - host_w) / 2,
             avail.y() + (avail.height() - host_h) / 2);

  auto close_host = [host]() { host->close(); };
  connect(m_confirmButton, &QPushButton::clicked, host, close_host);
  connect(m_cancelButton, &QPushButton::clicked, host, close_host);

  host->show();
  host->raise();
  host->activateWindow();
}

QImage ScreenshotWidget::captureWindow(HWND hwnd) {
  HDC hdc = CreateDCW(L"DISPLAY", nullptr, nullptr, nullptr);
  RECT rect = {0};
  int scr_width = 0;
  int scr_height = 0;
  if (hwnd == nullptr) {
    scr_width = GetDeviceCaps(hdc, HORZRES);
    scr_height = GetDeviceCaps(hdc, VERTRES);
  } else {
    GetWindowRect(hwnd, &rect);
    scr_width = rect.right - rect.left;
    scr_height = rect.bottom - rect.top;
  }

  HDC hmdc = CreateCompatibleDC(hdc);
  HBITMAP hbmp = CreateCompatibleBitmap(hdc, scr_width, scr_height);
  HBITMAP hold = static_cast<HBITMAP>(SelectObject(hmdc, hbmp));

  BITMAP bm;
  GetObject(hbmp, sizeof(bm), &bm);
  BITMAPINFOHEADER bi = {};
  bi.biSize = sizeof(BITMAPINFOHEADER);
  bi.biWidth = bm.bmWidth;
  bi.biHeight = -bm.bmHeight;
  bi.biPlanes = bm.bmPlanes;
  bi.biBitCount = bm.bmBitsPixel;
  bi.biCompression = BI_RGB;
  bi.biSizeImage = bm.bmHeight * bm.bmWidthBytes;

  char* buf = new char[bi.biSizeImage];
  BitBlt(hmdc, 0, 0, scr_width, scr_height, hdc, rect.left, rect.top, SRCCOPY);
  GetDIBits(hmdc, hbmp, 0L, static_cast<DWORD>(scr_height), buf, reinterpret_cast<LPBITMAPINFO>(&bi),
            DIB_RGB_COLORS);

  QImage img(reinterpret_cast<uchar*>(buf), scr_width, scr_height, QImage::Format_ARGB32);
  QImage result = img.copy();
  delete[] buf;
  SelectObject(hmdc, hold);
  DeleteObject(hbmp);
  DeleteDC(hmdc);
  DeleteDC(hdc);
  return result;
}

void ScreenshotWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.drawPixmap(0, 0, m_pixmap);
  QRect selection_rect(m_startPoint, m_endPoint);
  selection_rect = selection_rect.normalized();
  QPainterPath path;
  path.addRect(rect());
  path.addRect(selection_rect);
  painter.setBrush(QColor(0, 0, 0, 100));
  painter.setPen(Qt::NoPen);
  painter.drawPath(path);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(Qt::red, 2));
  painter.drawRect(selection_rect);
}

void ScreenshotWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  RelocateButtons();
}

void ScreenshotWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_startPoint = event->pos();
    m_endPoint = event->pos();
    m_isSelecting = true;
  }
}

void ScreenshotWidget::mouseMoveEvent(QMouseEvent* event) {
  if (m_isSelecting) {
    m_endPoint = event->pos();
    update();
  }
}

void ScreenshotWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && m_isSelecting) {
    m_isSelecting = false;
    update();
  }
}

void ScreenshotWidget::onScreenshotSelected(const QRect& rect) {
  if (rect.isValid()) {
    m_screenshotImage = m_pixmap.toImage().copy(rect.normalized());
  }
}
