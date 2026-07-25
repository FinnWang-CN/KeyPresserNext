#include "DarkWindowChrome.h"

#include "LayoutMetrics.h"
#include "../ui/DarkTitleBar.h"

#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QFormLayout>
#include <QLayout>
#include <QLayoutItem>
#include <QMainWindow>
#include <QMenuBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWKWidgets/widgetwindowagent.h>

namespace DarkWindowChrome {
namespace {

class TitleSyncFilter : public QObject {
 public:
  TitleSyncFilter(DarkTitleBar* bar, QWidget* window)
      : QObject(window), bar_(bar), window_(window) {}

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override {
    if (obj == window_ && bar_) {
      if (event->type() == QEvent::WindowTitleChange) {
        bar_->SetTitle(window_->windowTitle());
      } else if (event->type() == QEvent::WindowIconChange) {
        const QIcon icon = window_->windowIcon();
        bar_->SetIcon(icon.isNull() ? qApp->windowIcon() : icon);
      } else if (event->type() == QEvent::WindowStateChange) {
        bar_->SetMaximized(window_->isMaximized());
      }
    }
    return QObject::eventFilter(obj, event);
  }

 private:
  DarkTitleBar* bar_ = nullptr;
  QWidget* window_ = nullptr;
};

void WireButtons(QWidget* window, DarkTitleBar* bar, QWK::WidgetWindowAgent* agent,
                 const Options& options) {
  agent->setTitleBar(bar);
  if (options.minimize) {
    agent->setSystemButton(QWK::WindowAgentBase::Minimize,
                           static_cast<QWidget*>(bar->MinButton()));
  }
  if (options.maximize) {
    agent->setSystemButton(QWK::WindowAgentBase::Maximize,
                           static_cast<QWidget*>(bar->MaxButton()));
  }
  if (options.close) {
    agent->setSystemButton(QWK::WindowAgentBase::Close,
                           static_cast<QWidget*>(bar->CloseButton()));
  }
  if (bar->MenuBar()) {
    agent->setHitTestVisible(static_cast<QWidget*>(bar->MenuBar()), true);
  }

  QObject::connect(bar, &DarkTitleBar::closeRequested, window, &QWidget::close);
  QObject::connect(bar, &DarkTitleBar::minimizeRequested, window, &QWidget::showMinimized);
  QObject::connect(bar, &DarkTitleBar::maximizeRequested, window, [window, bar](bool max) {
    if (max) {
      window->showMaximized();
    } else {
      window->showNormal();
    }
    QTimer::singleShot(0, window, [window, bar]() {
      if (bar) {
        bar->SetMaximized(window->isMaximized());
      }
    });
  });
  bar->SetMaximized(window->isMaximized());
  window->installEventFilter(new TitleSyncFilter(bar, window));
}

void ReparentLayoutWidgets(QLayout* layout, QWidget* new_parent) {
  if (!layout || !new_parent) {
    return;
  }
  // QFormLayout 的 label/field 需按 Role 取，itemAt(int) 会漏掉标签导致叠在左上角
  if (auto* form = qobject_cast<QFormLayout*>(layout)) {
    for (int r = 0; r < form->rowCount(); ++r) {
      if (QLayoutItem* li = form->itemAt(r, QFormLayout::LabelRole)) {
        if (QWidget* w = li->widget()) {
          w->setParent(new_parent);
        } else if (QLayout* sub = li->layout()) {
          ReparentLayoutWidgets(sub, new_parent);
        }
      }
      if (QLayoutItem* fi = form->itemAt(r, QFormLayout::FieldRole)) {
        if (QWidget* w = fi->widget()) {
          w->setParent(new_parent);
        } else if (QLayout* sub = fi->layout()) {
          ReparentLayoutWidgets(sub, new_parent);
        }
      }
    }
    return;
  }
  for (int i = 0; i < layout->count(); ++i) {
    QLayoutItem* item = layout->itemAt(i);
    if (!item) {
      continue;
    }
    if (QWidget* w = item->widget()) {
      w->setParent(new_parent);
    } else if (QLayout* sub = item->layout()) {
      ReparentLayoutWidgets(sub, new_parent);
    }
  }
}

void WrapWithTitleBar(QWidget* window, DarkTitleBar* bar) {
  QLayout* old = window->layout();
  auto* content = new QWidget(window);
  if (old) {
    auto* content_lay = new QVBoxLayout(content);
    content_lay->setContentsMargins(old->contentsMargins());
    content_lay->setSpacing(old->spacing());
    auto* old_box = qobject_cast<QBoxLayout*>(old);
    while (old->count() > 0) {
      const int stretch = old_box ? old_box->stretch(0) : 0;
      QLayoutItem* item = old->takeAt(0);
      if (!item) {
        continue;
      }
      const Qt::Alignment align = item->alignment();
      if (QWidget* w = item->widget()) {
        w->setParent(content);
        content_lay->addWidget(w, stretch, align);
        delete item;
      } else if (QLayout* sub = item->layout()) {
        // 子布局内控件原先挂在 window 上，必须改挂到 content，否则会叠在窗口左上角
        ReparentLayoutWidgets(sub, content);
        content_lay->addLayout(sub, stretch);
        // QLayout 继承 QLayoutItem：takeAt 返回的就是子布局本身，
        // addLayout 后所有权已交给 content_lay，绝不能再 delete，否则布局被毁、控件叠左上角
        if (static_cast<QLayoutItem*>(sub) != item) {
          delete item;
        }
      } else if (item->spacerItem()) {
        content_lay->addItem(item);
      } else {
        delete item;
      }
    }
    delete old;
  }

  auto* root = new QVBoxLayout(window);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);
  root->addWidget(bar);
  root->addWidget(content, 1);
}

}  // namespace

void Install(QWidget* window, const Options& options) {
  if (!window || window->property("darkChromeInstalled").toBool()) {
    return;
  }
  window->setProperty("darkChromeInstalled", true);
  window->setAttribute(Qt::WA_DontCreateNativeAncestors);

  auto* agent = new QWK::WidgetWindowAgent(window);
  agent->setup(window);

  auto* bar = new DarkTitleBar(window);
  bar->SetTitle(window->windowTitle());
  const QIcon icon = window->windowIcon();
  bar->SetIcon(icon.isNull() ? qApp->windowIcon() : icon);
  bar->SetButtonsVisible(options.minimize, options.maximize, options.close);
  if (options.menu_bar) {
    bar->TakeMenuBar(options.menu_bar);
  }

  if (auto* main_win = qobject_cast<QMainWindow*>(window)) {
    main_win->setMenuWidget(bar);
  } else {
    WrapWithTitleBar(window, bar);
  }

  WireButtons(window, bar, agent, options);
  LayoutMetrics::SettleDeferred(window);
}

}  // namespace DarkWindowChrome
