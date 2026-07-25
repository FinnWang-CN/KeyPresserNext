#ifndef WINDOWSELECTHELPER_H
#define WINDOWSELECTHELPER_H

#include <windows.h>

class QWidget;

/**
 * 选窗工具公共辅助：选窗结束后把本工具对话框强制唤回前台。
 * （绑定目标窗时不要 ShowWindow(目标)，否则会把焦点抢走。）
 */
namespace WindowSelectHelper {

/** 立即强制置前（附着线程输入 + 临时置顶） */
void forceBringToFront(QWidget *widget);

/** 在 0 / 200 / 500ms 各尝试一次置前 */
void scheduleBringToFront(QWidget *widget);

/**
 * 与原版 KeyPresser「保持选择窗口激活置顶」一致：
 * 勾选时恢复最小化并 HWND_TOPMOST；取消时 HWND_NOTOPMOST。
 */
void setTargetWindowTopmost(HWND hwnd, bool topmost);

/**
 * 高亮目标窗口（红色半透明边框闪烁），不阻塞 UI、不往屏幕 DC 画框，
 * 避免 KeyPresser 相关窗口被刷成空白。
 * @param returnFocusWidget 闪烁结束后唤回的 KeyPresser 工具窗（可为空）
 */
void highlightTargetWindow(HWND hwnd, QWidget *returnFocusWidget = nullptr);

/** 强制刷新所有可见顶层窗（高亮结束后调用，修复偶发空白） */
void refreshAllTopLevelWindows();

} // namespace WindowSelectHelper

#endif // WINDOWSELECTHELPER_H
