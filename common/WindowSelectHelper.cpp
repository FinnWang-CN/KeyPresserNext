#include "WindowSelectHelper.h"

#include <QApplication>
#include <QFrame>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace WindowSelectHelper {

void forceBringToFront(QWidget *widget)
{
    if (!widget)
        return;

    if (widget->isMinimized())
        widget->setWindowState((widget->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    widget->showNormal();
    widget->raise();
    widget->activateWindow();

    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd)
        return;

    HWND fg = GetForegroundWindow();
    DWORD fgTid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    DWORD curTid = GetCurrentThreadId();
    if (fgTid && fgTid != curTid)
        AttachThreadInput(curTid, fgTid, TRUE);

    ShowWindow(hwnd, SW_SHOW);
    ShowWindow(hwnd, SW_RESTORE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    if (fgTid && fgTid != curTid)
        AttachThreadInput(curTid, fgTid, FALSE);

    widget->activateWindow();
}

void scheduleBringToFront(QWidget *widget)
{
    if (!widget)
        return;
    QTimer::singleShot(0, widget, [widget]() { forceBringToFront(widget); });
    QTimer::singleShot(200, widget, [widget]() { forceBringToFront(widget); });
    QTimer::singleShot(500, widget, [widget]() { forceBringToFront(widget); });
}

void setTargetWindowTopmost(HWND hwnd, bool topmost)
{
    if (!hwnd || !::IsWindow(hwnd))
        return;
    if (topmost) {
        if (::IsIconic(hwnd)) {
            ::ShowWindow(hwnd, SW_RESTORE);
        }
        ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
    } else {
        ::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                       SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
    }
}

void refreshAllTopLevelWindows()
{
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget *w : tops) {
        if (!w || !w->isVisible())
            continue;
        w->update();
        HWND h = reinterpret_cast<HWND>(w->winId());
        if (h) {
            InvalidateRect(h, nullptr, TRUE);
            RedrawWindow(h, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
    }
}

void highlightTargetWindow(HWND hwnd, QWidget *returnFocusWidget)
{
    if (!hwnd || !IsWindow(hwnd))
        return;

    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);

    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect))
        return;

    const int margin = 4;
    const int x = rect.left - margin;
    const int y = rect.top - margin;
    const int w = (rect.right - rect.left) + margin * 2;
    const int h = (rect.bottom - rect.top) + margin * 2;
    if (w <= 0 || h <= 0)
        return;

    const QPointer<QWidget> returnWidget(returnFocusWidget);

    // 用独立覆盖层闪烁，避免 GetWindowDC(NULL)+Sleep 阻塞 UI 并污染其它窗口画面
    auto *overlay = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    overlay->setAttribute(Qt::WA_TranslucentBackground);
    overlay->setAttribute(Qt::WA_ShowWithoutActivating);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setAttribute(Qt::WA_DeleteOnClose);
    overlay->setAttribute(Qt::WA_QuitOnClose, false);

    auto *frame = new QFrame(overlay);
    frame->setObjectName(QStringLiteral("hlFrame"));
    frame->setAttribute(Qt::WA_StyledBackground, true);
    auto *lay = new QVBoxLayout(overlay);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(frame);

    overlay->setGeometry(x, y, w, h);
    overlay->show();
    overlay->raise();

    FLASHWINFO fi = {};
    fi.cbSize = sizeof(fi);
    fi.hwnd = hwnd;
    fi.dwFlags = FLASHW_ALL;
    fi.uCount = 3;
    fi.dwTimeout = 120;
    FlashWindowEx(&fi);

    // 闪 3 次（显/隐），不阻塞事件循环；结束后唤回 KeyPresser 工具窗
    auto *timer = new QTimer(overlay);
    overlay->setProperty("hlStep", 0);
    QObject::connect(timer, &QTimer::timeout, overlay, [overlay, timer, returnWidget]() {
        const int step = overlay->property("hlStep").toInt() + 1;
        overlay->setProperty("hlStep", step);
        if (step > 6) {
            timer->stop();
            overlay->close();
            refreshAllTopLevelWindows();
            if (returnWidget) {
                // 稍后再置前，避免被目标窗/覆盖层关掉瞬间抢回
                scheduleBringToFront(returnWidget.data());
            } else {
                // 未指定时：把所有可见的 KeyPresser 顶层窗恢复显示并置前
                for (QWidget *w : QApplication::topLevelWidgets()) {
                    if (!w || !w->isWindow() || !w->isVisible())
                        continue;
                    if (w->isMinimized())
                        w->showNormal();
                    w->raise();
                }
            }
            return;
        }
        overlay->setVisible(step % 2 == 1);
    });
    timer->start(120);
}

} // namespace WindowSelectHelper
