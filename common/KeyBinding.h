#ifndef KEYBINDING_H
#define KEYBINDING_H

#include <QString>
#include <QStringList>
#include <QVector>

enum class KeySendProfile {
    WindowsVk,
    HardwareHid
};

struct KeyBinding {
    QVector<int> modifierVkCodes;
    int mainVkCode = 0;

    bool isValid() const { return mainVkCode != 0; }
    QString displayText() const;
    QString storageText() const;

    static KeyBinding fromDisplayText(const QString &text);
    static KeyBinding fromLegacySelection(const QString &shortcutLabel, const QString &keyLabel);

    int mainSendCode(KeySendProfile profile) const;
    QVector<int> modifierSendCodes(KeySendProfile profile) const;
};

QString legacyShortcutLabelAt(int index);
QString legacyKeyLabelAt(int index);

bool isModifierVk(int vk);
QString vkToDisplayName(int vk);
int displayNameToVk(const QString &name);
QStringList splitBindingParts(const QString &text);
int windowsVkForKeyInput(int vk);

#endif // KEYBINDING_H
