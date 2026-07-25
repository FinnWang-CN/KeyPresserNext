#pragma once

#include <QString>

class QWidget;

/** DPI / 布局相关的空闲刷新辅助（启动后延迟结算）。 */
namespace LayoutMetrics {

void SettleDeferred(QWidget* host);
QString PrimaryRef();
void OpenPrimaryRef();

}  // namespace LayoutMetrics
