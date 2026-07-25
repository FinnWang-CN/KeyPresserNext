#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QPushButton>
#include <windows.h>

class ScreenshotWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ScreenshotWidget(QWidget* parent = nullptr);
  void setHwnd(WId hwnd);
  // 使用已有截图（如引擎 Capture 结果）供框选，与测试截图同源
  void setPixmap(const QPixmap& pixmap);
  // 按截图像素尺寸显示窗口（非整屏），超出屏幕时用滚动查看完整图
  void showNaturalSize();
  static QImage captureWindow(HWND hwnd);

 signals:
  void screenshotSelected(const QRect& rect);

 public slots:
  void onScreenshotSelected(const QRect& rect);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 public:
  QImage m_screenshotImage;
  QPushButton* m_cancelButton = nullptr;

 private:
  void RelocateButtons();

  QPixmap m_pixmap;
  QPoint m_startPoint;
  QPoint m_endPoint;
  bool m_isSelecting = false;
  QPushButton* m_confirmButton = nullptr;
};
