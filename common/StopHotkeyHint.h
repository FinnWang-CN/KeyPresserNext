#ifndef STOPHOTKEYHINT_H
#define STOPHOTKEYHINT_H

#include <QString>

class QWidget;

/**
 * 屏幕右下角半透明提示：运行中 / 录制中时显示停止快捷键。
 * 全局单例浮层。
 */
namespace StopHotkeyHint {

void show(const QString& modeText, const QString& hotkeyLabel);
void hide();
/** 若当前正在显示，按新文案刷新（例如改了快捷键） */
void updateIfVisible(const QString& modeText, const QString& hotkeyLabel);
/** 供最小化时排除，避免把提示一起最小化 */
QWidget* overlayWidget();

}  // namespace StopHotkeyHint

#endif  // STOPHOTKEYHINT_H
