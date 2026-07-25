#ifndef KEYCAPTUREEDIT_H
#define KEYCAPTUREEDIT_H

#include "KeyBinding.h"

#include <QWidget>

class QLineEdit;

class KeyCaptureEdit : public QWidget
{
    Q_OBJECT

public:
    explicit KeyCaptureEdit(QWidget *parent = nullptr);

    bool hasBinding() const;
    bool isCapturing() const;
    KeyBinding binding() const;
    QString displayText() const;
    QString storageText() const;

    QStringList modifierVkCodes() const;
    int mainVkCode() const;
    int mainSendCode(KeySendProfile profile) const;
    QVector<int> modifierSendCodes(KeySendProfile profile) const;

    void setBinding(const KeyBinding &binding);
    void clearBinding();
    void setFromStorageText(const QString &text);
    void setFromLegacySelection(const QString &shortcutLabel, const QString &keyLabel);

    static bool handleHookKey(unsigned int vkCode, unsigned int message, unsigned int hookFlags = 0);

Q_SIGNALS:
    void bindingChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void startCapture();
    void cancelCapture();
    void finishCapture(const KeyBinding &binding);
    void updateDisplay();
    void updateToolTip();
    void applyCaptureStyle(bool capturing);

    QLineEdit *m_lineEdit = nullptr;
    KeyBinding m_binding;
    bool m_capturing = false;
};

#endif // KEYCAPTUREEDIT_H
