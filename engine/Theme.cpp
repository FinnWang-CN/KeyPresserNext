#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace Theme {

void ApplyDarkTheme(QApplication* app) {
  app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

  // Fluent Design Dark (Win11)
  QFont ui_font(QStringLiteral("Segoe UI"));
  ui_font.setPixelSize(13);
  app->setFont(ui_font);

  QPalette palette;
  const QColor window(0x20, 0x20, 0x20);      // #202020
  const QColor base(0x2c, 0x2c, 0x2c);        // #2C2C2C layer
  const QColor alt_base(0x32, 0x32, 0x32);    // #323232
  const QColor text(0xc8, 0xc8, 0xc8);        // #C8C8C8
  const QColor text_bright(0xff, 0xff, 0xff); // #FFFFFF
  const QColor disabled(0x6d, 0x6d, 0x6d);    // #6D6D6D
  const QColor button(0x2c, 0x2c, 0x2c);
  const QColor highlight(0x00, 0x78, 0xd4);   // #0078D4
  const QColor accent(0x60, 0xcd, 0xff);      // #60CDFF Fluent accent light

  palette.setColor(QPalette::Window, window);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, base);
  palette.setColor(QPalette::AlternateBase, alt_base);
  palette.setColor(QPalette::ToolTipBase, base);
  palette.setColor(QPalette::ToolTipText, text_bright);
  palette.setColor(QPalette::Text, text_bright);
  palette.setColor(QPalette::Button, button);
  palette.setColor(QPalette::ButtonText, text_bright);
  palette.setColor(QPalette::BrightText, text_bright);
  palette.setColor(QPalette::Link, accent);
  palette.setColor(QPalette::Highlight, highlight);
  palette.setColor(QPalette::HighlightedText, text_bright);
  palette.setColor(QPalette::Mid, QColor(0x3f, 0x3f, 0x3f));
  palette.setColor(QPalette::Dark, QColor(0x1c, 0x1c, 0x1c));
  palette.setColor(QPalette::Light, QColor(0x3c, 0x3c, 0x3c));
  palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
  app->setPalette(palette);

  QFile qss(QStringLiteral(":/style.qss"));
  if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
    app->setStyleSheet(QString::fromUtf8(qss.readAll()));
  }
}

}  // namespace Theme
