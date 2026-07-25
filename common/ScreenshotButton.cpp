#include "ScreenshotButton.h"

#include <QApplication>
#include <QCursor>
#include <QDesktopWidget>
#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include <QShowEvent>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

ImageTooltip::ImageTooltip(QWidget *parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // 不用 WA_TranslucentBackground：深色主题下容易整块发黑、看不清图
    setObjectName(QStringLiteral("screenshotImageTooltip"));
    setStyleSheet(QStringLiteral(
        "#screenshotImageTooltip {"
        "  background-color: #1e1f24;"
        "  border: 1px solid #5a5d66;"
        "  border-radius: 6px;"
        "}"
        "#screenshotImageTooltip QLabel {"
        "  background-color: #121318;"
        "  color: #e8e8e8;"
        "  padding: 4px;"
        "}"));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(0);
    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(48, 48);
    lay->addWidget(m_imageLabel);
}

void ImageTooltip::setImage(const QImage &image)
{
    if (image.isNull()) {
        m_imageLabel->clear();
        m_imageLabel->setText(QStringLiteral("无法预览"));
        adjustSize();
        return;
    }

    // 强制成带 alpha 的 32bit，避免部分 BMP 格式在悬浮窗里画成纯黑
    const QImage img = image.convertToFormat(QImage::Format_ARGB32);
    QPixmap pix = QPixmap::fromImage(img);
    if (pix.isNull()) {
        m_imageLabel->clear();
        m_imageLabel->setText(QStringLiteral("无法预览"));
        adjustSize();
        return;
    }

    m_imageLabel->setText(QString());
    m_imageLabel->setPixmap(
        pix.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    adjustSize();
}

ScreenshotButton::ScreenshotButton(QWidget *parent)
    : QPushButton(parent)
    , m_tooltipTimer(new QTimer(this))
    , m_imageAvailable(false)
    , m_tooltipWidget(new ImageTooltip)
    , m_menu(new QMenu(this))
    , m_screenshotAction(new QAction(QStringLiteral("截图"), this))
    , m_localImageAction(new QAction(QStringLiteral("选择本地截图文件"), this))
    , m_clearImageAction(new QAction(QStringLiteral("清除图片"), this))
{
    m_menu->addAction(m_screenshotAction);
    m_menu->addAction(m_localImageAction);
    m_menu->addAction(m_clearImageAction);

    connect(m_screenshotAction, &QAction::triggered, this, &ScreenshotButton::onScreenshotAction);
    connect(m_localImageAction, &QAction::triggered, this, &ScreenshotButton::onLocalImageAction);
    connect(m_clearImageAction, &QAction::triggered, this, &ScreenshotButton::onClearImageAction);

    setObjectName(QStringLiteral("btnScreenshot"));
    setIcon(QIcon(QStringLiteral(":/png/image_white.png")));
    setIconSize(QSize(14, 14));
    setText(QStringLiteral("截图"));
    m_helpTip = QStringLiteral("截图点击位置");
    setToolTip(m_helpTip);
    setMenu(m_menu);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    updateLayoutWidth();

    QTimer::singleShot(0, this, &ScreenshotButton::updateLayoutWidth);

    m_tooltipTimer->setSingleShot(true);
    m_tooltipTimer->setInterval(400);
    connect(m_tooltipTimer, &QTimer::timeout, this, [this]() {
        if (!m_imageAvailable || m_screenshotImage.isNull()) {
            return;
        }
        m_tooltipWidget->setImage(m_screenshotImage);

        const QPoint globalPos = QCursor::pos();
        const QRect screenGeometry = QApplication::desktop()->screenGeometry(globalPos);
        int x = globalPos.x() + 16;
        int y = globalPos.y() + 16;
        if (x + m_tooltipWidget->width() > screenGeometry.right()) {
            x = screenGeometry.right() - m_tooltipWidget->width() - 4;
        }
        if (y + m_tooltipWidget->height() > screenGeometry.bottom()) {
            y = screenGeometry.bottom() - m_tooltipWidget->height() - 4;
        }
        if (x < screenGeometry.left()) {
            x = screenGeometry.left() + 4;
        }
        if (y < screenGeometry.top()) {
            y = screenGeometry.top() + 4;
        }

        m_tooltipWidget->move(x, y);
        m_tooltipWidget->show();
        m_tooltipWidget->raise();
    });
}

void ScreenshotButton::updateLayoutWidth()
{
    updateGeometry();
}

void ScreenshotButton::showEvent(QShowEvent *event)
{
    QPushButton::showEvent(event);
    updateLayoutWidth();
}

void ScreenshotButton::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateLayoutWidth();
    }
    QPushButton::changeEvent(event);
}

void ScreenshotButton::setHelpToolTip(const QString &tip)
{
    m_helpTip = tip;
    syncTextToolTip();
}

void ScreenshotButton::syncTextToolTip()
{
    // 有截图预览时不再显示文字 tip，避免和图片预览叠在一起
    if (m_imageAvailable && !m_screenshotImage.isNull()) {
        setToolTip(QString());
    } else {
        setToolTip(m_helpTip);
    }
}

void ScreenshotButton::setScreenshotImage(const QString &path)
{
    QImage img(path);
    if (!img.isNull() && img.format() != QImage::Format_ARGB32 &&
        img.format() != QImage::Format_RGB32 &&
        img.format() != QImage::Format_ARGB32_Premultiplied) {
        img = img.convertToFormat(QImage::Format_ARGB32);
    }
    setScreenshotImage(img);
}

void ScreenshotButton::setScreenshotImage(const QImage &image)
{
    m_screenshotImage = image;
    m_imageAvailable = !image.isNull();
    if (!m_imageAvailable) {
        m_tooltipWidget->hide();
    }
    syncTextToolTip();
}

void ScreenshotButton::clearScreenshotImage()
{
    m_screenshotImage = QImage();
    m_imageAvailable = false;
    m_tooltipWidget->hide();
    syncTextToolTip();
}

void ScreenshotButton::onClearImageAction()
{
    clearScreenshotImage();
    emit clearImageRequested();
}

void ScreenshotButton::enterEvent(QEvent *event)
{
    QPushButton::enterEvent(event);
    if (m_imageAvailable && !m_screenshotImage.isNull()) {
        // 确保不会再弹出系统文字 tip
        setToolTip(QString());
        m_tooltipTimer->start();
    }
}

void ScreenshotButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    m_tooltipTimer->stop();
    m_tooltipWidget->hide();
}

void ScreenshotButton::onScreenshotAction()
{
    emit screenshotRequested();
}

void ScreenshotButton::onLocalImageAction()
{
    emit localImageRequested();
}
