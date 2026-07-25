#ifndef SCREENSHOTBUTTON_H
#define SCREENSHOTBUTTON_H

#include <QPushButton>
#include <QShowEvent>
#include <QToolTip>
#include <QTimer>
#include <QMouseEvent>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QFileDialog>

// 自定义的tooltip窗口类
class ImageTooltip : public QWidget
{
public:
    explicit ImageTooltip(QWidget *parent = nullptr);
    void setImage(const QImage &image);

private:
    QLabel *m_imageLabel;
};

class ScreenshotButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ScreenshotButton(QWidget *parent = nullptr);
    void setScreenshotImage(const QString &path);
    void setScreenshotImage(const QImage &image);
    void clearScreenshotImage();
    /** 无截图时的文字说明；有截图时只显示图片预览，不显示该 tip */
    void setHelpToolTip(const QString &tip);

signals:
    void screenshotRequested();
    void localImageRequested();
    void clearImageRequested();

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onScreenshotAction();
    void onLocalImageAction();
    void onClearImageAction();

private:
    void updateLayoutWidth();
    void syncTextToolTip();
    QImage m_screenshotImage;
    QString m_helpTip;
    QTimer *m_tooltipTimer;
    bool m_imageAvailable;
    ImageTooltip *m_tooltipWidget;
    QMenu *m_menu;
    QAction *m_screenshotAction;
    QAction *m_localImageAction;
    QAction *m_clearImageAction;
};

#endif // SCREENSHOTBUTTON_H
