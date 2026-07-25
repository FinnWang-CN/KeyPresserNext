#include "GlobalHotkeys.h"

#include "KeyCaptureEdit.h"

#include <algorithm>

GlobalHotkeys* GlobalHotkeys::instance_ = nullptr;

namespace {

KeyBinding BindingFromStorage(const QString& text) {
  if (text.trimmed().isEmpty()) {
    return KeyBinding();
  }
  return KeyBinding::fromDisplayText(text);
}

int NormalizeVk(int code) {
  switch (code) {
    case VK_LMENU:
    case VK_RMENU:
      return VK_MENU;
    case VK_LCONTROL:
    case VK_RCONTROL:
      return VK_CONTROL;
    case VK_LSHIFT:
    case VK_RSHIFT:
      return VK_SHIFT;
    case VK_RWIN:
      return VK_LWIN;
    default:
      return code;
  }
}

}  // namespace

GlobalHotkeys::GlobalHotkeys(QObject* parent) : QObject(parent) {
  instance_ = this;
  for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
    bindings_[i] = BindingFromStorage(DefaultStorage(static_cast<Action>(i)));
  }
}

QString GlobalHotkeys::DefaultStorage(Action action) {
  switch (action) {
    case Action::Start:
      return QStringLiteral("F9");
    case Action::Stop:
      // 与 Start 共用同一键，不再单独注册
      return QString();
    case Action::Record:
      return QStringLiteral("F8");
    case Action::StopRecord:
      // 与 Record 共用同一键，不再单独注册
      return QString();
    case Action::SelectWindow:
    case Action::HighlightWindow:
    case Action::AddKey:
    case Action::AddMouse:
    case Action::Clear:
      // 默认仅预置「运行」「录制」；其余留空，由用户按需设置
      return QString();
    case Action::Undo:
      return QStringLiteral("Ctrl+Z");
    case Action::Redo:
      return QStringLiteral("Ctrl+Y");
    default:
      return QString();
  }
}

GlobalHotkeys::~GlobalHotkeys() {
  Uninstall();
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

bool GlobalHotkeys::Install() {
  if (hook_) {
    return true;
  }
  hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelProc, GetModuleHandleW(nullptr), 0);
  return hook_ != nullptr;
}

void GlobalHotkeys::Uninstall() {
  if (hook_) {
    UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
  }
}

void GlobalHotkeys::SetFromStorage(Action action, const QString& text) {
  const int i = static_cast<int>(action);
  if (i < 0 || i >= static_cast<int>(Action::Count)) {
    return;
  }
  bindings_[i] = BindingFromStorage(text);
}

QString GlobalHotkeys::Storage(Action action) const {
  const int i = static_cast<int>(action);
  if (i < 0 || i >= static_cast<int>(Action::Count)) {
    return QString();
  }
  return bindings_[i].storageText();
}

QString GlobalHotkeys::Label(Action action) const {
  const int i = static_cast<int>(action);
  if (i < 0 || i >= static_cast<int>(Action::Count)) {
    return QString();
  }
  return bindings_[i].displayText();
}

int GlobalHotkeys::MainVk(Action action) const {
  const int i = static_cast<int>(action);
  if (i < 0 || i >= static_cast<int>(Action::Count)) {
    return 0;
  }
  return bindings_[i].mainVkCode;
}

void GlobalHotkeys::SetRestrictMode(RestrictMode mode) { restrict_mode_ = mode; }

bool GlobalHotkeys::IsActionAllowed(Action action) const {
  switch (restrict_mode_) {
    case RestrictMode::OnlyStartStop:
      return action == Action::Start || action == Action::Stop;
    case RestrictMode::OnlyRecordStop:
      return action == Action::Record || action == Action::StopRecord;
    case RestrictMode::None:
    default:
      return true;
  }
}

bool GlobalHotkeys::IsConfiguredHotkeyVk(int vk) const {
  const int n = NormalizeVk(vk);
  for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
    const auto action = static_cast<Action>(i);
    if (!IsActionAllowed(action)) {
      continue;
    }
    if (bindings_[i].isValid() && NormalizeVk(bindings_[i].mainVkCode) == n) {
      return true;
    }
  }
  return false;
}

int GlobalHotkeys::VkFromName(const QString& name) {
  const KeyBinding b = BindingFromStorage(name);
  return b.isValid() ? b.mainVkCode : VK_F9;
}

QString GlobalHotkeys::NameFromVk(int vk) {
  if (vk <= 0) {
    return QString();
  }
  return vkToDisplayName(vk);
}

QString GlobalHotkeys::NormalizeStorage(const QString& text) {
  const KeyBinding b = BindingFromStorage(text);
  if (!b.isValid()) {
    return QString();
  }
  QVector<int> mods = b.modifierVkCodes;
  std::sort(mods.begin(), mods.end());
  QStringList parts;
  for (int m : mods) {
    parts << vkToDisplayName(NormalizeVk(m));
  }
  parts << vkToDisplayName(NormalizeVk(b.mainVkCode));
  return parts.join(QLatin1Char('+')).toLower();
}

bool GlobalHotkeys::BindingsEqual(const QString& a, const QString& b) {
  const QString na = NormalizeStorage(a);
  const QString nb = NormalizeStorage(b);
  if (na.isEmpty() || nb.isEmpty()) {
    return false;
  }
  return na == nb;
}

bool GlobalHotkeys::BindingMatches(const KeyBinding& binding, unsigned int vk) {
  if (!binding.isValid()) {
    return false;
  }
  if (NormalizeVk(binding.mainVkCode) != NormalizeVk(static_cast<int>(vk))) {
    return false;
  }
  for (int mod : binding.modifierVkCodes) {
    const int check = NormalizeVk(mod);
    if (check == VK_LWIN) {
      if (!((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000))) {
        return false;
      }
      continue;
    }
    if (!(GetAsyncKeyState(check) & 0x8000)) {
      return false;
    }
  }
  return true;
}

void GlobalHotkeys::EmitAction(Action action) {
  switch (action) {
    case Action::Start:
      emit startPressed();
      break;
    case Action::Stop:
      emit stopPressed();
      break;
    case Action::Record:
      emit recordPressed();
      break;
    case Action::StopRecord:
      emit stopRecordPressed();
      break;
    case Action::SelectWindow:
      emit selectWindowPressed();
      break;
    case Action::HighlightWindow:
      emit highlightWindowPressed();
      break;
    case Action::AddKey:
      emit addKeyPressed();
      break;
    case Action::AddMouse:
      emit addMousePressed();
      break;
    case Action::Clear:
      emit clearPressed();
      break;
    case Action::Undo:
      emit undoPressed();
      break;
    case Action::Redo:
      emit redoPressed();
      break;
    default:
      break;
  }
}

LRESULT CALLBACK GlobalHotkeys::LowLevelProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code == HC_ACTION && instance_) {
    const auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
    const unsigned int vk = kb->vkCode;
    const unsigned int message = static_cast<unsigned int>(wparam);
    const unsigned int flags = kb->flags;

    if (KeyCaptureEdit::handleHookKey(vk, message, flags)) {
      return 1;
    }

    const bool key_down = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN);
    const bool key_up = (wparam == WM_KEYUP || wparam == WM_SYSKEYUP);
    if (key_up) {
      instance_->latch_.remove(static_cast<int>(vk));
    }
    if (key_down) {
      if (instance_->latch_.contains(static_cast<int>(vk))) {
        return CallNextHookEx(instance_->hook_, code, wparam, lparam);
      }
      // 停止优先，避免与其它键冲突时无法停下
      static const Action kOrder[] = {
          Action::Start, Action::Record, Action::SelectWindow, Action::HighlightWindow,
          Action::AddKey, Action::AddMouse, Action::Clear, Action::Undo, Action::Redo};
      for (Action action : kOrder) {
        if (!instance_->IsActionAllowed(action)) {
          continue;
        }
        const KeyBinding& b = instance_->bindings_[static_cast<int>(action)];
        if (BindingMatches(b, vk)) {
          instance_->latch_.insert(static_cast<int>(vk));
          instance_->EmitAction(action);
          break;
        }
      }
    }
  }
  return CallNextHookEx(instance_ ? instance_->hook_ : nullptr, code, wparam, lparam);
}
