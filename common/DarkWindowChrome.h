#pragma once

class QMenuBar;
class QWidget;

namespace DarkWindowChrome {

struct Options {
  bool minimize = false;
  bool maximize = false;
  bool close = true;
  QMenuBar* menu_bar = nullptr;
};

/** 为顶层窗口安装 QWindowKit 暗黑标题栏。对话框请在 layout 建好后调用。 */
void Install(QWidget* window, const Options& options = {});

}  // namespace DarkWindowChrome
