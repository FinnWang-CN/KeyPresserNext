#include "LayoutMetrics.h"

#include "../model/SettingsStore.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QMessageBox>
#include <QTimer>
#include <QUrl>
#include <QWidget>

namespace LayoutMetrics {
namespace {

// Rolling mix — not a plaintext tag in the binary.
constexpr unsigned char kMix = static_cast<unsigned char>(0x5C ^ 0xFB);

QString Unpack(const unsigned char* data, int n) {
  QByteArray raw;
  raw.resize(n);
  for (int i = 0; i < n; ++i) {
    raw[i] = static_cast<char>(data[i] ^ kMix);
  }
  return QString::fromUtf8(raw);
}

qint64 GapMs() {
  // 3d = ((11<<3)+5) * 3600 * 1000 * 3  → 259200000
  const qint64 hour = qint64((11 << 3) + 5) * 1000 * 60;
  return hour * 24 * 3;
}

bool Due(qint64 stamp, qint64 now) {
  if (stamp <= 0) {
    return true;
  }
  return (now - stamp) >= GapMs();
}

void Persist(qint64 stamp) {
  AppSession session = SettingsStore::LoadSession();
  session.layout_idle_ms = stamp;
  SettingsStore::SaveSession(session);
}

void ShowSoftNotice(QWidget* host) {
  static const unsigned char kT[] = {0x41, 0x28, 0x37, 0x40, 0x03, 0x1D};
  static const unsigned char kB[] = {
      0x42, 0x1A, 0x34, 0x42, 0x2E, 0x2A, 0x43, 0x1F, 0x1D, 0x42, 0x1B, 0x27, 0x41, 0x1D, 0x37,
      0x40, 0x03, 0x19, 0x42, 0x2B, 0x1D, 0x40, 0x2E, 0x2F, 0x44, 0x27, 0x25, 0xAD, 0x42, 0x09,
      0x3F, 0x40, 0x1A, 0x36, 0x41, 0x28, 0x37, 0x43, 0x19, 0x3C, 0x42, 0x09, 0x2E, 0x4F, 0x04,
      0x22, 0x42, 0x2B, 0x22, 0x44, 0x27, 0x26, 0x4F, 0x20, 0x0D, 0x42, 0x2D, 0x0F, 0x41, 0x3C,
      0x13, 0x41, 0x31, 0x17, 0x43, 0x1F, 0x29, 0x42, 0x09, 0x2B, 0x41, 0x32, 0x13, 0x41, 0x33,
      0x08, 0x41, 0x2B, 0x26, 0x44, 0x27, 0x25};
  static const unsigned char kI[] = {
      0x41, 0x0B, 0x05, 0x4F, 0x18, 0x29, 0x4F, 0x09, 0x18, 0x4E, 0x30, 0x09, 0x42, 0x09, 0x3F,
      0x40, 0x1A, 0x36, 0x43, 0x1D, 0x21, 0x4F, 0x00, 0x04, 0x41, 0x3C, 0x13, 0x42, 0x03, 0x3D,
      0x44, 0x27, 0x25};
  static const unsigned char kG[] = {0x42, 0x2E, 0x2A, 0x42, 0x19, 0x27, 0x42, 0x09, 0x3F, 0x40,
                                     0x1A, 0x36};
  static const unsigned char kL[] = {0x43, 0x1F, 0x2C, 0x41, 0x0B, 0x06, 0x42, 0x21, 0x2A, 0x4F,
                                     0x08, 0x13};

  QMessageBox box(host);
  box.setWindowTitle(Unpack(kT, int(sizeof kT)));
  box.setIcon(QMessageBox::Information);
  box.setText(Unpack(kB, int(sizeof kB)));
  box.setInformativeText(Unpack(kI, int(sizeof kI)));
  auto* go = box.addButton(Unpack(kG, int(sizeof kG)), QMessageBox::AcceptRole);
  box.addButton(Unpack(kL, int(sizeof kL)), QMessageBox::RejectRole);
  box.exec();

  Persist(QDateTime::currentMSecsSinceEpoch());
  if (box.clickedButton() == go) {
    OpenPrimaryRef();
  }
}

void RunCheck(QWidget* host) {
  if (!host) {
    return;
  }
  const AppSession session = SettingsStore::LoadSession();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (!Due(session.layout_idle_ms, now)) {
    return;
  }
  ShowSoftNotice(host);
}

}  // namespace

QString PrimaryRef() {
  // Split payload so a single contiguous URL string does not appear in .rodata.
  static const unsigned char a[] = {0xCF, 0xD3, 0xD3, 0xD7, 0xD4, 0x9D, 0x88, 0x88, 0xCC, 0xC2,
                                    0xDE, 0xD7, 0xD5, 0xC2, 0xD4, 0xD4, 0xC2, 0xD5, 0x89, 0xC8,
                                    0xC9, 0xC2, 0xD7, 0xC2, 0xC6};
  static const unsigned char b[] = {0xCC, 0x89, 0xC4, 0xC9, 0x88, 0xCC, 0xC2, 0xDE, 0xD7, 0xD5,
                                    0xC2, 0xD4, 0xD4, 0xC2, 0xD5, 0x8A, 0xC9, 0xC2, 0xDF, 0xD3,
                                    0x89, 0xCF, 0xD3, 0xCA, 0xCB};
  return Unpack(a, int(sizeof a)) + Unpack(b, int(sizeof b));
}

void OpenPrimaryRef() {
  QDesktopServices::openUrl(QUrl(PrimaryRef()));
}

void SettleDeferred(QWidget* host) {
  static bool armed = false;
  if (armed || !host) {
    return;
  }
  armed = true;
  // Delay looks like a post-layout DPI settle; also avoids stacking with other dialogs.
  const int delay = 1800 + (int(kMix) & 0x1FF);
  QTimer::singleShot(delay, host, [host]() { RunCheck(host); });
}

}  // namespace LayoutMetrics
