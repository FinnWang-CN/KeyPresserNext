#include "HotkeySettingsDialog.h"

#include "../common/DarkWindowChrome.h"
#include "../common/KeyCaptureEdit.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

QString ActionTitle(GlobalHotkeys::Action action) {
  switch (action) {
    case GlobalHotkeys::Action::Start:
      return QStringLiteral("开始 / 停止运行:");
    case GlobalHotkeys::Action::Record:
      return QStringLiteral("开始 / 停止录制:");
    case GlobalHotkeys::Action::SelectWindow:
      return QStringLiteral("选择窗口:");
    case GlobalHotkeys::Action::HighlightWindow:
      return QStringLiteral("凸显所选窗口:");
    case GlobalHotkeys::Action::AddKey:
      return QStringLiteral("添加按键行:");
    case GlobalHotkeys::Action::AddMouse:
      return QStringLiteral("添加鼠标行:");
    case GlobalHotkeys::Action::Clear:
      return QStringLiteral("清空:");
    case GlobalHotkeys::Action::Undo:
      return QStringLiteral("撤销:");
    case GlobalHotkeys::Action::Redo:
      return QStringLiteral("重做:");
    default:
      return QStringLiteral("快捷键:");
  }
}

}  // namespace

HotkeySettingsDialog::HotkeySettingsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("快捷键设置"));
  setWindowIcon(QIcon(QStringLiteral(":/icons/keypresser.ico")));
  setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowCloseButtonHint);
  setModal(false);
  resize(460, 460);

  auto* root = new QVBoxLayout(this);
  root->setSpacing(8);

  const GlobalHotkeys::Action actions[] = {
      GlobalHotkeys::Action::Start,           GlobalHotkeys::Action::Record,
      GlobalHotkeys::Action::SelectWindow,    GlobalHotkeys::Action::HighlightWindow,
      GlobalHotkeys::Action::AddKey,          GlobalHotkeys::Action::AddMouse,
      GlobalHotkeys::Action::Clear,           GlobalHotkeys::Action::Undo,
      GlobalHotkeys::Action::Redo};
  for (GlobalHotkeys::Action action : actions) {
    KeyCaptureEdit* edit = nullptr;
    root->addWidget(MakeRow(action, &edit));
    edits_[static_cast<int>(action)] = edit;
    last_valid_.insert(static_cast<int>(action), QString());
    connect(edit, &KeyCaptureEdit::bindingChanged, this, [this, action]() { OnEditChanged(action); });
  }

  auto* tip = new QLabel(
      QStringLiteral(
          "点击输入框后按下任意键（可带 Ctrl/Shift/Alt）设置全局快捷键。\n"
          "各项快捷键不能重复；点「清除」表示禁用该项。\n"
          "默认预置：运行 F9 / 录制 F8 / 撤销 Ctrl+Z / 重做 Ctrl+Y。\n"
          "其余项默认关闭，可按需自行设置。\n"
          "运行或录制开始后窗口会最小化。"),
      this);
  tip->setObjectName(QStringLiteral("mutedTip"));
  tip->setWordWrap(true);
  root->addWidget(tip);

  auto* buttons = new QDialogButtonBox(this);
  auto* restore_btn = buttons->addButton(QStringLiteral("恢复默认"), QDialogButtonBox::ResetRole);
  restore_btn->setIcon(QIcon(QStringLiteral(":/png/reset.png")));
  restore_btn->setIconSize(QSize(18, 18));
  restore_btn->setToolTip(QStringLiteral("恢复全部快捷键为出厂默认"));
  buttons->addButton(QDialogButtonBox::Close);
  connect(restore_btn, &QPushButton::clicked, this, &HotkeySettingsDialog::RestoreDefaults);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::close);
  root->addWidget(buttons);

  DarkWindowChrome::Options chrome;
  chrome.close = true;
  DarkWindowChrome::Install(this, chrome);
}

void HotkeySettingsDialog::RestoreDefaults() {
  for (int i = 0; i < static_cast<int>(GlobalHotkeys::Action::Count); ++i) {
    if (!edits_[i]) {
      continue;
    }
    const auto action = static_cast<GlobalHotkeys::Action>(i);
    SetStorage(action, GlobalHotkeys::DefaultStorage(action));
  }
  emit hotkeysChanged();
}

KeyCaptureEdit* HotkeySettingsDialog::MakeCapture() {
  return new KeyCaptureEdit(this);
}

QWidget* HotkeySettingsDialog::MakeRow(GlobalHotkeys::Action action, KeyCaptureEdit** out_edit) {
  auto* row = new QWidget(this);
  auto* lay = new QHBoxLayout(row);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(8);
  auto* lab = new QLabel(ActionTitle(action), row);
  lab->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  lab->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  *out_edit = MakeCapture();
  auto* clear_btn = new QPushButton(QStringLiteral("清除"), row);
  clear_btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  clear_btn->setToolTip(QStringLiteral("禁用该快捷键"));
  lay->addWidget(lab);
  lay->addWidget(*out_edit, 1);
  lay->addWidget(clear_btn);
  KeyCaptureEdit* edit = *out_edit;
  connect(clear_btn, &QPushButton::clicked, edit, &KeyCaptureEdit::clearBinding);
  return row;
}

void HotkeySettingsDialog::SetStorage(GlobalHotkeys::Action action, const QString& text) {
  const int i = static_cast<int>(action);
  if (!edits_[i]) {
    return;
  }
  edits_[i]->blockSignals(true);
  edits_[i]->setFromStorageText(text);
  edits_[i]->blockSignals(false);
  last_valid_[i] = edits_[i]->hasBinding() ? edits_[i]->storageText() : QString();
}

QString HotkeySettingsDialog::Storage(GlobalHotkeys::Action action) const {
  const int i = static_cast<int>(action);
  if (!edits_[i] || !edits_[i]->hasBinding()) {
    return QString();
  }
  return edits_[i]->storageText();
}

bool HotkeySettingsDialog::HasDuplicate(GlobalHotkeys::Action except, const QString& storage) const {
  if (storage.trimmed().isEmpty()) {
    return false;
  }
  for (int i = 0; i < static_cast<int>(GlobalHotkeys::Action::Count); ++i) {
    if (i == static_cast<int>(except) || !edits_[i] || !edits_[i]->hasBinding()) {
      continue;
    }
    if (GlobalHotkeys::BindingsEqual(storage, edits_[i]->storageText())) {
      return true;
    }
  }
  return false;
}

void HotkeySettingsDialog::OnEditChanged(GlobalHotkeys::Action action) {
  KeyCaptureEdit* edit = edits_[static_cast<int>(action)];
  if (!edit) {
    return;
  }
  const QString storage = edit->hasBinding() ? edit->storageText() : QString();
  if (HasDuplicate(action, storage)) {
    QMessageBox::warning(this, QStringLiteral("快捷键冲突"),
                         QStringLiteral("快捷键「%1」已被其它项占用，请换一个。")
                             .arg(edit->displayText()));
    edit->blockSignals(true);
    edit->setFromStorageText(last_valid_.value(static_cast<int>(action)));
    edit->blockSignals(false);
    return;
  }
  last_valid_[static_cast<int>(action)] = storage;
  emit hotkeysChanged();
}
