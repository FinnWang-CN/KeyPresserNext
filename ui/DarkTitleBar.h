#pragma once

#include <QIcon>
#include <QWidget>

class QLabel;
class QMenuBar;
class QPushButton;
class QResizeEvent;
class QShowEvent;

class DarkTitleBar : public QWidget {
  Q_OBJECT

 public:
  static constexpr int kHeight = 32;

  explicit DarkTitleBar(QWidget* parent = nullptr);

  void TakeMenuBar(QMenuBar* menu_bar);
  void SetTitle(const QString& title);
  void SetIcon(const QIcon& icon);
  void SetMaximized(bool maximized);
  void SetButtonsVisible(bool minimize, bool maximize, bool close = true);

  QLabel* IconLabel() const { return icon_label_; }
  QPushButton* MinButton() const { return min_btn_; }
  QPushButton* MaxButton() const { return max_btn_; }
  QPushButton* CloseButton() const { return close_btn_; }
  QMenuBar* MenuBar() const { return menu_bar_; }

 signals:
  void minimizeRequested();
  void maximizeRequested(bool maximize);
  void closeRequested();

 protected:
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 private:
  void RelayoutTitle();

  QLabel* icon_label_ = nullptr;
  QLabel* title_label_ = nullptr;
  QMenuBar* menu_bar_ = nullptr;
  QPushButton* min_btn_ = nullptr;
  QPushButton* max_btn_ = nullptr;
  QPushButton* close_btn_ = nullptr;
  QString title_text_;
};
