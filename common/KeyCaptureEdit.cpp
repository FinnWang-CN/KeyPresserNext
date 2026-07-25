#include "KeyCaptureEdit.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>
#include <windows.h>

namespace {

KeyCaptureEdit *g_activeCapture = nullptr;
QVector<int> g_heldModifiers;

int normalizeModifierVk(int vk)
{
    switch (vk) {
    case VK_LCONTROL:
    case VK_RCONTROL:
        return VK_CONTROL;
    case VK_LSHIFT:
    case VK_RSHIFT:
        return VK_SHIFT;
    case VK_LMENU:
    case VK_RMENU:
        return VK_MENU;
    case VK_RWIN:
        return VK_LWIN;
    default:
        return vk;
    }
}

void clearHeldModifiers()
{
    g_heldModifiers.clear();
}

void setModifierHeld(int vk)
{
    const int normalized = normalizeModifierVk(vk);
    if (!isModifierVk(normalized))
        return;

    if (!g_heldModifiers.contains(normalized))
        g_heldModifiers.append(normalized);
}

QVector<int> collectModifierVkCodes(unsigned int hookFlags)
{
    QVector<int> modifiers;
    auto ensureModifier = [&](int vk) {
        const int normalized = normalizeModifierVk(vk);
        if (!modifiers.contains(normalized))
            modifiers.append(normalized);
    };

    for (int vk : g_heldModifiers)
        ensureModifier(vk);

    BYTE keys[256] = {0};
    if (GetKeyboardState(keys)) {
        if (keys[VK_CONTROL] & 0x80)
            ensureModifier(VK_CONTROL);
        if (keys[VK_LCONTROL] & 0x80)
            ensureModifier(VK_LCONTROL);
        if (keys[VK_RCONTROL] & 0x80)
            ensureModifier(VK_RCONTROL);
        if (keys[VK_SHIFT] & 0x80)
            ensureModifier(VK_SHIFT);
        if (keys[VK_LSHIFT] & 0x80)
            ensureModifier(VK_LSHIFT);
        if (keys[VK_RSHIFT] & 0x80)
            ensureModifier(VK_RSHIFT);
        if (keys[VK_MENU] & 0x80)
            ensureModifier(VK_MENU);
        if (keys[VK_LMENU] & 0x80)
            ensureModifier(VK_LMENU);
        if (keys[VK_RMENU] & 0x80)
            ensureModifier(VK_RMENU);
        if ((keys[VK_LWIN] & 0x80) || (keys[VK_RWIN] & 0x80))
            ensureModifier(VK_LWIN);
    }

    if (hookFlags & LLKHF_ALTDOWN)
        ensureModifier(VK_MENU);

    return modifiers;
}

} // namespace

KeyCaptureEdit::KeyCaptureEdit(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setReadOnly(true);
    m_lineEdit->setPlaceholderText(QStringLiteral("点击设置按键"));
    m_lineEdit->setFocusPolicy(Qt::StrongFocus);
    m_lineEdit->installEventFilter(this);
    layout->addWidget(m_lineEdit);

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    updateDisplay();
}

bool KeyCaptureEdit::hasBinding() const
{
    return m_binding.isValid();
}

bool KeyCaptureEdit::isCapturing() const
{
    return m_capturing;
}

KeyBinding KeyCaptureEdit::binding() const
{
    return m_binding;
}

QString KeyCaptureEdit::displayText() const
{
    return m_binding.displayText();
}

QString KeyCaptureEdit::storageText() const
{
    return m_binding.storageText();
}

QStringList KeyCaptureEdit::modifierVkCodes() const
{
    QStringList result;
    for (int vk : m_binding.modifierVkCodes)
        result << QString::number(vk);
    return result;
}

int KeyCaptureEdit::mainVkCode() const
{
    return m_binding.mainVkCode;
}

int KeyCaptureEdit::mainSendCode(KeySendProfile profile) const
{
    return m_binding.mainSendCode(profile);
}

QVector<int> KeyCaptureEdit::modifierSendCodes(KeySendProfile profile) const
{
    return m_binding.modifierSendCodes(profile);
}

void KeyCaptureEdit::setBinding(const KeyBinding &binding)
{
    if (m_capturing)
        cancelCapture();
    m_binding = binding;
    updateDisplay();
    emit bindingChanged();
}

void KeyCaptureEdit::clearBinding()
{
    const bool had = m_binding.isValid() || m_capturing;
    m_binding = KeyBinding();
    if (m_capturing)
        cancelCapture();
    else
        updateDisplay();
    if (had)
        emit bindingChanged();
}

void KeyCaptureEdit::setFromStorageText(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        clearBinding();
        return;
    }
    setBinding(KeyBinding::fromDisplayText(text));
}

void KeyCaptureEdit::setFromLegacySelection(const QString &shortcutLabel, const QString &keyLabel)
{
    setBinding(KeyBinding::fromLegacySelection(shortcutLabel, keyLabel));
}

bool KeyCaptureEdit::handleHookKey(unsigned int vkCode, unsigned int message, unsigned int hookFlags)
{
    if (!g_activeCapture)
        return false;

    const bool isKeyUp = (message == WM_KEYUP || message == WM_SYSKEYUP);

    if (isKeyUp)
        return true;

    if (isModifierVk(static_cast<int>(vkCode))) {
        setModifierHeld(static_cast<int>(vkCode));
        return true;
    }

    KeyBinding binding;
    binding.modifierVkCodes = collectModifierVkCodes(hookFlags);
    binding.mainVkCode = static_cast<int>(vkCode);
    g_activeCapture->finishCapture(binding);
    return true;
}

bool KeyCaptureEdit::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_lineEdit) {
        if (event->type() == QEvent::FocusOut) {
            if (m_capturing)
                cancelCapture();
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                startCapture();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void KeyCaptureEdit::startCapture()
{
    if (!isEnabled())
        return;

    if (g_activeCapture && g_activeCapture != this)
        g_activeCapture->cancelCapture();

    m_capturing = true;
    g_activeCapture = this;
    clearHeldModifiers();
    applyCaptureStyle(true);
    updateDisplay();
    m_lineEdit->setFocus();
}

void KeyCaptureEdit::cancelCapture()
{
    if (!m_capturing)
        return;

    m_capturing = false;
    if (g_activeCapture == this)
        g_activeCapture = nullptr;
    clearHeldModifiers();
    applyCaptureStyle(false);
    updateDisplay();
}

void KeyCaptureEdit::finishCapture(const KeyBinding &binding)
{
    clearHeldModifiers();

    if (binding.isValid())
        m_binding = binding;
    emit bindingChanged();

    // 保持监听，可连续按键覆盖，失焦或 Esc 后退出
    m_capturing = true;
    g_activeCapture = this;
    applyCaptureStyle(true);
    updateDisplay();
    m_lineEdit->setFocus();
}

void KeyCaptureEdit::updateDisplay()
{
    if (m_binding.isValid())
        m_lineEdit->setText(m_binding.displayText());
    else if (m_capturing)
        m_lineEdit->setText(QStringLiteral("请按下按键..."));
    else
        m_lineEdit->clear();
    updateToolTip();
}

void KeyCaptureEdit::updateToolTip()
{
    QString tip;
    if (m_capturing) {
        tip = QStringLiteral("请按下要模拟的按键（可带 Ctrl/Shift/Alt）\n"
                             "再次按键可覆盖；点击别处或失焦取消");
    } else if (m_binding.isValid()) {
        tip = QStringLiteral("将模拟的按键：%1\n点击后按下新按键可重新设置").arg(m_binding.displayText());
    } else {
        tip = QStringLiteral("点击此处，再按下要模拟的按键或组合键（如 Ctrl+A、F1）");
    }
    setToolTip(tip);
    if (m_lineEdit)
        m_lineEdit->setToolTip(tip);
}

void KeyCaptureEdit::applyCaptureStyle(bool capturing)
{
    m_lineEdit->setProperty("capturing", capturing);
    m_lineEdit->style()->unpolish(m_lineEdit);
    m_lineEdit->style()->polish(m_lineEdit);
    m_lineEdit->update();
}
