#include "BulkAddRowsDialog.h"

#include "../common/DarkWindowChrome.h"
#include "../model/BulkRowImporter.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

BulkAddRowsDialog::BulkAddRowsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("批量添加行"));
  setWindowIcon(QIcon(QStringLiteral(":/icons/keypresser.ico")));
  setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowCloseButtonHint);
  setMinimumSize(560, 480);

  auto* root = new QVBoxLayout(this);

  auto* kind_row = new QHBoxLayout();
  key_radio_ = new QRadioButton(QStringLiteral("按键行"), this);
  mouse_radio_ = new QRadioButton(QStringLiteral("鼠标行"), this);
  auto* kind_group = new QButtonGroup(this);
  kind_group->addButton(key_radio_);
  kind_group->addButton(mouse_radio_);
  key_radio_->setChecked(true);
  mouse_radio_->setChecked(false);
  kind_row->addWidget(new QLabel(QStringLiteral("类型："), this));
  kind_row->addWidget(key_radio_);
  kind_row->addWidget(mouse_radio_);
  kind_row->addStretch();
  root->addLayout(kind_row);

  hint_label_ = new QLabel(this);
  hint_label_->setObjectName(QStringLiteral("mutedTip"));
  hint_label_->setWordWrap(true);
  hint_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(hint_label_);

  text_edit_ = new QTextEdit(this);
  root->addWidget(text_edit_, 1);

  auto* load_btn = new QPushButton(QStringLiteral("从文件读取…"), this);
  root->addWidget(load_btn, 0, Qt::AlignLeft);

  auto* mode_row = new QHBoxLayout();
  append_radio_ = new QRadioButton(QStringLiteral("追加到现有行"), this);
  replace_radio_ = new QRadioButton(QStringLiteral("替换现有行"), this);
  auto* mode_group = new QButtonGroup(this);
  mode_group->addButton(append_radio_);
  mode_group->addButton(replace_radio_);
  append_radio_->setChecked(true);
  mode_row->addWidget(append_radio_);
  mode_row->addWidget(replace_radio_);
  mode_row->addStretch();
  root->addLayout(mode_row);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("添加"));
  root->addWidget(buttons);

  connect(key_radio_, &QRadioButton::toggled, this, &BulkAddRowsDialog::OnKindChanged);
  connect(mouse_radio_, &QRadioButton::toggled, this, &BulkAddRowsDialog::OnKindChanged);
  connect(load_btn, &QPushButton::clicked, this, &BulkAddRowsDialog::OnLoadFile);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  key_radio_->setChecked(true);
  OnKindChanged();

  DarkWindowChrome::Options chrome;
  chrome.close = true;
  DarkWindowChrome::Install(this, chrome);
}

ActionKind BulkAddRowsDialog::SelectedKind() const {
  return mouse_radio_->isChecked() ? ActionKind::Mouse : ActionKind::Key;
}

bool BulkAddRowsDialog::AppendMode() const { return append_radio_->isChecked(); }

QVector<ActionRow> BulkAddRowsDialog::ParsedRows() const {
  return BulkRowImporter::ParseText(text_edit_->toPlainText(), SelectedKind());
}

void BulkAddRowsDialog::OnKindChanged() {
  if (!text_edit_) {
    return;
  }
  if (mouse_radio_ && mouse_radio_->isChecked()) {
    if (hint_label_) {
      hint_label_->setText(QStringLiteral(
          "规则：每行一条；字段用英文逗号或 Tab 分隔；空行与以 #、// 开头的行忽略。\n"
          "\n"
          "鼠标行格式（按字段数识别）：\n"
          "• 1 段：按钮\n"
          "    例：左键\n"
          "• 3 段：启用, 按钮, 间隔ms\n"
          "    例：1,右键,1000\n"
          "• 4 段：启用, 按钮, 间隔最小ms, 间隔最大ms\n"
          "    例：1,左双击,500,800\n"
          "• 5 段：启用, 按钮, 图片路径, 间隔最小ms, 间隔最大ms（识图）\n"
          "    例：1,左键,C:\\img.png,1000,1000\n"
          "• 6 段：启用, 按钮, x, y, 间隔最小, 间隔最大（客户区坐标）\n"
          "    或：启用, 按钮, 图片路径, 空, 间隔最小, 间隔最大\n"
          "    例：1,左键,100,200,1000,1000\n"
          "\n"
          "按钮可用：左键 / 右键 / 中键 / 左双击 / 滚轮上 / 滚轮下 / "
          "左键按下 / 左键弹起 / 右键按下 / 右键弹起。\n"
          "启用：1、true、是、勾选 表示启用；其它为禁用。间隔单位为毫秒。"));
    }
    text_edit_->setPlaceholderText(QStringLiteral(
        "在此粘贴或输入，例如：\n"
        "左键\n"
        "1,左双击,1000,1000\n"
        "1,右键,100,200,500,800\n"
        "1,左键,C:\\img.png,1000,1000"));
  } else {
    if (hint_label_) {
      hint_label_->setText(QStringLiteral(
          "规则：每行一条；字段用英文逗号或 Tab 分隔；空行与以 #、// 开头的行忽略。\n"
          "\n"
          "按键行格式（按字段数识别）：\n"
          "• 1 段：按键\n"
          "    例：F1　　或　　Ctrl+A\n"
          "• 3 段：启用, 按键, 间隔ms\n"
          "    例：1,Ctrl+A,1000\n"
          "• 4 段：启用, 按键, 间隔最小ms, 间隔最大ms\n"
          "    例：1,F3,500,800\n"
          "• 5 段：启用, 修饰键, 主键, 间隔最小ms, 间隔最大ms\n"
          "    例：1,Ctrl,A,500,800（修饰键可为空：1,,F3,0,1000）\n"
          "• 6 段及以上：启用, 修饰键, 主键, 长按, 间隔最小, 间隔最大\n"
          "    例：1,Ctrl,A,0,500,800　　长按填 1/true/是 表示独立运行时长按\n"
          "\n"
          "启用：1、true、是、勾选 表示启用；其它为禁用。间隔单位为毫秒。"));
    }
    text_edit_->setPlaceholderText(QStringLiteral(
        "在此粘贴或输入，例如：\n"
        "F1\n"
        "F2\n"
        "1,Ctrl+A,1000,1000\n"
        "1,,F3,0,1000,1000\n"
        "1,Ctrl,A,0,500,800"));
  }
}

void BulkAddRowsDialog::OnLoadFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("选择导入文件"), QString(),
      QStringLiteral("文本文件 (*.txt *.csv);;所有文件 (*.*)"));
  if (path.isEmpty()) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, QStringLiteral("读取失败"),
                         QStringLiteral("无法打开文件：%1").arg(path));
    return;
  }
  QTextStream stream(&file);
  stream.setCodec("UTF-8");
  text_edit_->setPlainText(stream.readAll());
}
