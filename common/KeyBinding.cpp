#include "KeyBinding.h"

#include <QHash>
#include <windows.h>

namespace {

QString normalizePart(const QString &part)
{
    return part.trimmed();
}

QHash<QString, int> buildNameToVkMap()
{
    QHash<QString, int> map;
    auto add = [&](const QString &name, int vk) {
        map.insert(name, vk);
    };

    for (int i = 0; i < 12; ++i)
        add(QStringLiteral("F%1").arg(i + 1), VK_F1 + i);

    for (int code = 'A'; code <= 'Z'; ++code)
        add(QString(QChar(code)), code);
    for (int code = '0'; code <= '9'; ++code)
        add(QString(QChar(code)), code);

    add(QStringLiteral("小键盘 0"), VK_NUMPAD0);
    add(QStringLiteral("小键盘 1"), VK_NUMPAD1);
    add(QStringLiteral("小键盘 2"), VK_NUMPAD2);
    add(QStringLiteral("小键盘 3"), VK_NUMPAD3);
    add(QStringLiteral("小键盘 4"), VK_NUMPAD4);
    add(QStringLiteral("小键盘 5"), VK_NUMPAD5);
    add(QStringLiteral("小键盘 6"), VK_NUMPAD6);
    add(QStringLiteral("小键盘 7"), VK_NUMPAD7);
    add(QStringLiteral("小键盘 8"), VK_NUMPAD8);
    add(QStringLiteral("小键盘 9"), VK_NUMPAD9);
    add(QStringLiteral("小键盘 +"), VK_ADD);
    add(QStringLiteral("小键盘 -"), VK_SUBTRACT);
    add(QStringLiteral("小键盘 *"), VK_MULTIPLY);
    add(QStringLiteral("小键盘 /"), VK_DIVIDE);
    add(QStringLiteral("小键盘 ."), VK_DECIMAL);

    add(QStringLiteral("Shift"), VK_SHIFT);
    add(QStringLiteral("Ctrl"), VK_CONTROL);
    add(QStringLiteral("Alt"), VK_MENU);
    add(QStringLiteral("Win"), VK_LWIN);
    add(QStringLiteral("Space"), VK_SPACE);
    add(QStringLiteral("Enter"), VK_RETURN);
    add(QStringLiteral("Tab"), VK_TAB);
    add(QStringLiteral("Esc"), VK_ESCAPE);
    add(QStringLiteral("Backspace"), VK_BACK);
    add(QStringLiteral("Insert"), VK_INSERT);
    add(QStringLiteral("Delete"), VK_DELETE);
    add(QStringLiteral("Home"), VK_HOME);
    add(QStringLiteral("End"), VK_END);
    add(QStringLiteral("Page Up"), VK_PRIOR);
    add(QStringLiteral("Page Down"), VK_NEXT);
    add(QStringLiteral("Left Arrow"), VK_LEFT);
    add(QStringLiteral("Right Arrow"), VK_RIGHT);
    add(QStringLiteral("Up Arrow"), VK_UP);
    add(QStringLiteral("Down Arrow"), VK_DOWN);
    add(QStringLiteral("←"), VK_LEFT);
    add(QStringLiteral("→"), VK_RIGHT);
    add(QStringLiteral("↑"), VK_UP);
    add(QStringLiteral("↓"), VK_DOWN);

    add(QStringLiteral("Caps Lock"), VK_CAPITAL);
    add(QStringLiteral("Num Lock"), VK_NUMLOCK);
    add(QStringLiteral("Scroll Lock"), VK_SCROLL);
    add(QStringLiteral("Print Screen"), VK_SNAPSHOT);
    add(QStringLiteral("Pause"), VK_PAUSE);
    add(QStringLiteral("Menu"), VK_APPS);

    add(QStringLiteral(";"), VK_OEM_1);
    add(QStringLiteral("="), VK_OEM_PLUS);
    add(QStringLiteral(","), VK_OEM_COMMA);
    add(QStringLiteral("-"), VK_OEM_MINUS);
    add(QStringLiteral("."), VK_OEM_PERIOD);
    add(QStringLiteral("/"), VK_OEM_2);
    add(QStringLiteral("`"), VK_OEM_3);
    add(QStringLiteral("["), VK_OEM_4);
    add(QStringLiteral("\\"), VK_OEM_5);
    add(QStringLiteral("]"), VK_OEM_6);
    add(QStringLiteral("'"), VK_OEM_7);
    return map;
}

const QHash<QString, int> &nameToVkMap()
{
    static const QHash<QString, int> map = buildNameToVkMap();
    return map;
}

int modifierNameToVk(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.compare(QStringLiteral("Ctrl"), Qt::CaseInsensitive) == 0)
        return VK_CONTROL;
    if (trimmed.compare(QStringLiteral("Shift"), Qt::CaseInsensitive) == 0)
        return VK_SHIFT;
    if (trimmed.compare(QStringLiteral("Alt"), Qt::CaseInsensitive) == 0)
        return VK_MENU;
    if (trimmed.compare(QStringLiteral("Win"), Qt::CaseInsensitive) == 0)
        return VK_LWIN;
    return 0;
}

int vkModifierToHardware(int vk)
{
    switch (vk) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return 129;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return 128;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return 130;
    case VK_LWIN:
    case VK_RWIN:
        return 131;
    default:
        return vk;
    }
}

int vkMainToHardware(int vk)
{
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
        return vk;
    if (vk >= VK_F1 && vk <= VK_F12)
        return 0x3A + (vk - VK_F1);
    switch (vk) {
    case VK_NUMPAD0: return 0x62;
    case VK_NUMPAD1: return 0x59;
    case VK_NUMPAD2: return 0x5A;
    case VK_NUMPAD3: return 0x5B;
    case VK_NUMPAD4: return 0x5C;
    case VK_NUMPAD5: return 0x5D;
    case VK_NUMPAD6: return 0x5E;
    case VK_NUMPAD7: return 0x5F;
    case VK_NUMPAD8: return 0x60;
    case VK_NUMPAD9: return 0x61;
    case VK_DECIMAL: return 0x63;
    case VK_ADD: return 0x57;
    case VK_SUBTRACT: return 0x56;
    case VK_MULTIPLY: return 0x55;
    case VK_DIVIDE: return 0x54;
    case VK_SPACE: return 0x20;
    case VK_RETURN: return 0x28;
    case VK_TAB: return 0x2B;
    case VK_ESCAPE: return 0x29;
    case VK_BACK: return 0x2A;
    case VK_INSERT: return 0x49;
    case VK_DELETE: return 0x4C;
    case VK_HOME: return 0x4A;
    case VK_END: return 0x4D;
    case VK_PRIOR: return 0x4B;
    case VK_NEXT: return 0x4E;
    case VK_LEFT: return 0x50;
    case VK_RIGHT: return 0x4F;
    case VK_UP: return 0x52;
    case VK_DOWN: return 0x51;
    case VK_SHIFT: return 129;
    case VK_OEM_1: return 0x33;
    case VK_OEM_PLUS: return 0x2E;
    case VK_OEM_COMMA: return 0x36;
    case VK_OEM_MINUS: return 0x2D;
    case VK_OEM_PERIOD: return 0x37;
    case VK_OEM_2: return 0x38;
    case VK_OEM_3: return 0x35;
    case VK_OEM_4: return 0x2F;
    case VK_OEM_5: return 0x31;
    case VK_OEM_6: return 0x30;
    case VK_OEM_7: return 0x34;
    case VK_CAPITAL: return 0x39;
    case VK_NUMLOCK: return 0x53;
    case VK_SCROLL: return 0x47;
    case VK_PAUSE: return 0x48;
    case VK_APPS: return 0x65;
    default:
        return vk;
    }
}

QString legacyShortcutToBindingPrefix(const QString &shortcutLabel)
{
    const QString trimmed = shortcutLabel.trimmed();
    if (trimmed.isEmpty())
        return QString();

    if (trimmed == QStringLiteral("Shift+Ctrl"))
        return QStringLiteral("Ctrl+Shift");
    if (trimmed == QStringLiteral("Shift+Alt"))
        return QStringLiteral("Shift+Alt");
    if (trimmed == QStringLiteral("Ctrl+Alt"))
        return QStringLiteral("Ctrl+Alt");
    if (trimmed == QStringLiteral("Ctrl+Win"))
        return QStringLiteral("Ctrl+Win");
    if (trimmed == QStringLiteral("Alt+Win"))
        return QStringLiteral("Alt+Win");
    if (trimmed == QStringLiteral("Shift+Win"))
        return QStringLiteral("Shift+Win");

    return trimmed;
}

} // namespace

QStringList splitBindingParts(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral(" + "), QStringLiteral("+"));
    normalized.replace(QStringLiteral("＋"), QStringLiteral("+"));
    return normalized.split('+', Qt::SkipEmptyParts);
}

bool isModifierVk(int vk)
{
    switch (vk) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LMENU:
    case VK_RMENU:
        return true;
    default:
        return false;
    }
}

QString vkToDisplayName(int vk)
{
    switch (vk) {
    case VK_LEFT: return QStringLiteral("←");
    case VK_RIGHT: return QStringLiteral("→");
    case VK_UP: return QStringLiteral("↑");
    case VK_DOWN: return QStringLiteral("↓");
    default:
        break;
    }

    static const QHash<int, QString> vkNames = []() {
        QHash<int, QString> names;
        const auto &map = nameToVkMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            names.insert(it.value(), it.key());
        return names;
    }();

    const auto it = vkNames.constFind(vk);
    if (it != vkNames.constEnd())
        return it.value();
    return QStringLiteral("VK_%1").arg(vk);
}

int displayNameToVk(const QString &name)
{
    const QString trimmed = name.trimmed();
    const int modifierVk = modifierNameToVk(trimmed);
    if (modifierVk != 0)
        return modifierVk;

    const auto &map = nameToVkMap();
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (it.key().compare(trimmed, Qt::CaseInsensitive) == 0)
            return it.value();
    }
    return 0;
}

int windowsVkForKeyInput(int vk)
{
    switch (vk) {
    case VK_CONTROL:
        return VK_LCONTROL;
    case VK_SHIFT:
        return VK_LSHIFT;
    case VK_MENU:
        return VK_LMENU;
    case VK_RWIN:
        return VK_LWIN;
    default:
        return vk;
    }
}

QString KeyBinding::displayText() const
{
    if (!isValid())
        return QString();

    QStringList parts;
    for (int vk : modifierVkCodes)
        parts << vkToDisplayName(vk);
    parts << vkToDisplayName(mainVkCode);
    return parts.join(QStringLiteral(" + "));
}

QString KeyBinding::storageText() const
{
    if (!isValid())
        return QString();

    QStringList parts;
    for (int vk : modifierVkCodes)
        parts << vkToDisplayName(vk);
    parts << vkToDisplayName(mainVkCode);
    return parts.join('+');
}

KeyBinding KeyBinding::fromDisplayText(const QString &text)
{
    KeyBinding binding;
    const QStringList parts = splitBindingParts(text);
    if (parts.isEmpty())
        return binding;

    QStringList modifierNames;
    QString mainName;
    for (const QString &part : parts) {
        const QString normalized = normalizePart(part);
        if (modifierNameToVk(normalized) != 0)
            modifierNames << normalized;
        else
            mainName = normalized;
    }

    if (mainName.isEmpty() && !modifierNames.isEmpty()) {
        mainName = modifierNames.takeLast();
        if (modifierNameToVk(mainName) == 0)
            return binding;
    }

    const int mainVk = displayNameToVk(mainName);
    if (mainVk == 0 || isModifierVk(mainVk))
        return binding;

    binding.mainVkCode = mainVk;
    for (const QString &modifierName : modifierNames) {
        const int modifierVk = modifierNameToVk(modifierName);
        if (modifierVk != 0 && !binding.modifierVkCodes.contains(modifierVk))
            binding.modifierVkCodes.append(modifierVk);
    }
    return binding;
}

KeyBinding KeyBinding::fromLegacySelection(const QString &shortcutLabel, const QString &keyLabel)
{
    QString combined;
    const QString prefix = legacyShortcutToBindingPrefix(shortcutLabel);
    if (!prefix.isEmpty() && !keyLabel.trimmed().isEmpty())
        combined = prefix + '+' + keyLabel.trimmed();
    else if (!keyLabel.trimmed().isEmpty())
        combined = keyLabel.trimmed();
    else
        return KeyBinding();

    return KeyBinding::fromDisplayText(combined);
}

int KeyBinding::mainSendCode(KeySendProfile profile) const
{
    if (profile == KeySendProfile::HardwareHid)
        return vkMainToHardware(mainVkCode);
    return mainVkCode;
}

QVector<int> KeyBinding::modifierSendCodes(KeySendProfile profile) const
{
    QVector<int> codes;
    codes.reserve(modifierVkCodes.size());
    for (int vk : modifierVkCodes) {
        if (profile == KeySendProfile::HardwareHid)
            codes.append(vkModifierToHardware(vk));
        else
            codes.append(vk);
    }
    return codes;
}

QString legacyShortcutLabelAt(int index)
{
    static const QStringList labels = {
        QString(),
        QStringLiteral("Shift"),
        QStringLiteral("Ctrl"),
        QStringLiteral("Alt"),
        QStringLiteral("Win"),
        QStringLiteral("Shift+Ctrl"),
        QStringLiteral("Shift+Alt"),
        QStringLiteral("Ctrl+Alt"),
        QStringLiteral("Ctrl+Win"),
        QStringLiteral("Alt+Win"),
        QStringLiteral("Shift+Win"),
    };
    if (index < 0 || index >= labels.size())
        return QString();
    return labels.at(index);
}

QString legacyKeyLabelAt(int index)
{
    static const QStringList labels = {
        QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3"), QStringLiteral("F4"),
        QStringLiteral("F5"), QStringLiteral("F6"), QStringLiteral("F7"), QStringLiteral("F8"),
        QStringLiteral("F9"), QStringLiteral("F10"), QStringLiteral("F11"), QStringLiteral("F12"),
        QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D"),
        QStringLiteral("E"), QStringLiteral("F"), QStringLiteral("G"), QStringLiteral("H"),
        QStringLiteral("I"), QStringLiteral("J"), QStringLiteral("K"), QStringLiteral("L"),
        QStringLiteral("M"), QStringLiteral("N"), QStringLiteral("O"), QStringLiteral("P"),
        QStringLiteral("Q"), QStringLiteral("R"), QStringLiteral("S"), QStringLiteral("T"),
        QStringLiteral("U"), QStringLiteral("V"), QStringLiteral("W"), QStringLiteral("X"),
        QStringLiteral("Y"), QStringLiteral("Z"),
        QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
        QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"),
        QStringLiteral("8"), QStringLiteral("9"),
        QStringLiteral("小键盘 0"), QStringLiteral("小键盘 1"), QStringLiteral("小键盘 2"),
        QStringLiteral("小键盘 3"), QStringLiteral("小键盘 4"), QStringLiteral("小键盘 5"),
        QStringLiteral("小键盘 6"), QStringLiteral("小键盘 7"), QStringLiteral("小键盘 8"),
        QStringLiteral("小键盘 9"), QStringLiteral("小键盘 +"), QStringLiteral("小键盘 -"),
        QStringLiteral("小键盘 *"), QStringLiteral("小键盘 /"), QStringLiteral("小键盘 ."),
        QStringLiteral("Shift"), QStringLiteral("Space"), QStringLiteral("Enter"),
        QStringLiteral("Tab"), QStringLiteral("Esc"), QStringLiteral("Backspace"),
        QStringLiteral("Insert"), QStringLiteral("Delete"), QStringLiteral("Home"),
        QStringLiteral("End"), QStringLiteral("Page Up"), QStringLiteral("Page Down"),
        QStringLiteral("Left Arrow"), QStringLiteral("Right Arrow"), QStringLiteral("Up Arrow"),
        QStringLiteral("Down Arrow"),
    };
    if (index < 0 || index >= labels.size())
        return QString();
    return labels.at(index);
}
