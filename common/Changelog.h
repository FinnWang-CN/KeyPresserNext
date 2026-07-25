#pragma once

#include <QString>

class QWidget;

namespace Changelog {

/** 从 docs/ 或内嵌资源加载 Markdown 全文。 */
QString LoadMarkdown();

/** 弹出更新日志对话框；force=false 时若内容为空则提示找不到文件。 */
void ShowDialog(QWidget* parent, bool force = true);

/**
 * 若当前版本与 session 中记录的「已读更新日志版本」不同，则弹窗并写回。
 * 首次安装也会弹出（记录为空）。
 */
void MaybeShowAfterUpgrade(QWidget* parent);

}  // namespace Changelog
