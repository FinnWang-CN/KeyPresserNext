#include "ActionRowWidget.h"

#include "../common/KeyCaptureEdit.h"
#include "../common/ScreenshotButton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QTimer>

namespace {

int LineEditWidthForText(QLineEdit* edit, const QString& text) {
  if (!edit) {
    return 0;
  }
  QStyleOptionFrame opt;
  opt.initFrom(edit);
  opt.lineWidth = edit->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &opt, edit);
  opt.midLineWidth = 0;
  opt.state |= QStyle::State_Sunken;
  if (edit->isReadOnly()) {
    opt.state |= QStyle::State_ReadOnly;
  }
  const QFontMetrics fm(edit->font());
  const QSize contents(fm.horizontalAdvance(text) + 2, fm.height());
  return edit->style()->sizeFromContents(QStyle::CT_LineEdit, &opt, contents, edit).width();
}

void FitIntervalEditWidth(QLineEdit* edit) {
  if (!edit) {
    return;
  }
  const QString sample =
      edit->text().isEmpty() ? QStringLiteral("1000") : edit->text();
  const int min_w = LineEditWidthForText(edit, QStringLiteral("0000"));
  const int w = LineEditWidthForText(edit, sample);
  edit->setFixedWidth(qMax(min_w, w));
}

void FitPointEditWidth(QLineEdit* edit) {
  if (!edit) {
    return;
  }
  // 至少完整显示「999,888」，并计入 QSS 边框/内边距
  const QString min_sample = QStringLiteral("999,888");
  const QString sample =
      edit->text().isEmpty() ? min_sample : edit->text();
  const int min_w = LineEditWidthForText(edit, min_sample);
  const int w = LineEditWidthForText(edit, sample);
  edit->setFixedWidth(qMax(min_w, w) + 6);
}

}  // namespace

ActionRowWidget::ActionRowWidget(ActionKind kind, QWidget* parent)
    : QWidget(parent), kind_(kind) {
  setObjectName(QStringLiteral("actionRow"));
  setAttribute(Qt::WA_StyledBackground, true);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 4, 6, 4);
  layout->setSpacing(6);

  index_label_ = new QLabel(QStringLiteral("#"), this);
  index_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  enabled_check_ = new QCheckBox(this);
  enabled_check_->setChecked(true);
  enabled_check_->setToolTip(QStringLiteral("勾选后参与运行；取消勾选则跳过此行。"));

  layout->addWidget(index_label_);
  layout->addWidget(enabled_check_);

  if (kind_ == ActionKind::Key) {
    key_capture_ = new KeyCaptureEdit(this);
    key_capture_->setToolTip(QStringLiteral("点击后按下要发送的按键（可含组合键）。"));
    key_stroke_combo_ = new QComboBox(this);
    key_stroke_combo_->addItem(QStringLiteral("按键"), QStringLiteral("press"));
    key_stroke_combo_->addItem(QStringLiteral("按下"), QStringLiteral("down"));
    key_stroke_combo_->addItem(QStringLiteral("弹起"), QStringLiteral("up"));
    key_stroke_combo_->setItemData(
        0, QStringLiteral("完整按下再弹起（与原先一致），适合普通单击按键。"), Qt::ToolTipRole);
    key_stroke_combo_->setItemData(
        1, QStringLiteral("只发送按下，不弹起；可与「弹起」拆分，用于长按或组合操作。"),
        Qt::ToolTipRole);
    key_stroke_combo_->setItemData(
        2, QStringLiteral("只发送弹起；通常与前面的「按下」配对使用。"), Qt::ToolTipRole);
    key_stroke_combo_->setToolTip(key_stroke_combo_->itemData(0, Qt::ToolTipRole).toString());
    key_stroke_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    key_stroke_combo_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    hold_check_ = new QCheckBox(QStringLiteral("长按"), this);
    hold_check_->setToolTip(
        QStringLiteral("仅「独立运行」可用：开始时按下并保持，停止后释放；间隔与次数对该行无效。"));
    layout->addWidget(key_capture_);
    layout->addWidget(key_stroke_combo_);
    layout->addWidget(hold_check_);
    connect(key_capture_, &KeyCaptureEdit::bindingChanged, this, &ActionRowWidget::EmitChanged);
    connect(key_stroke_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ActionRowWidget::EmitChanged);
    connect(key_stroke_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
      const QString tip = key_stroke_combo_->currentData(Qt::ToolTipRole).toString();
      if (!tip.isEmpty()) {
        key_stroke_combo_->setToolTip(tip);
      }
    });
    connect(hold_check_, &QCheckBox::toggled, this, &ActionRowWidget::EmitChanged);
  } else {
    mouse_button_combo_ = new QComboBox(this);
    mouse_button_combo_->addItem(QStringLiteral("左键"), QStringLiteral("left"));
    mouse_button_combo_->addItem(QStringLiteral("右键"), QStringLiteral("right"));
    mouse_button_combo_->addItem(QStringLiteral("中键"), QStringLiteral("middle"));
    mouse_button_combo_->addItem(QStringLiteral("左双击"), QStringLiteral("leftdouble"));
    mouse_button_combo_->addItem(QStringLiteral("滚轮上"), QStringLiteral("wheelup"));
    mouse_button_combo_->addItem(QStringLiteral("滚轮下"), QStringLiteral("wheeldown"));
    // 按下/弹起由 SetMousePressReleaseVisible 按运行模式动态加入
    mouse_button_combo_->setToolTip(
        QStringLiteral("要执行的鼠标动作。\n"
                       "「按下 / 弹起」仅顺序运行可用，适合长按、拖拽；"
                       "录制左/右键也会拆成按下与弹起两行。"));
    mouse_button_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    mouse_button_combo_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    layout->addWidget(mouse_button_combo_);
    connect(mouse_button_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ActionRowWidget::EmitChanged);
  }

  // 按键行：截图可选（无图=标准版直接按键；有图=识图版）
  // 鼠标行：坐标 / 识图
  if (kind_ == ActionKind::Mouse) {
    target_mode_combo_ = new QComboBox(this);
    target_mode_combo_->addItem(QStringLiteral("坐标"), QStringLiteral("coordinate"));
    target_mode_combo_->addItem(QStringLiteral("识图"), QStringLiteral("findpic"));
    target_mode_combo_->setItemData(
        0, QStringLiteral("移到指定坐标再点击。"), Qt::ToolTipRole);
    target_mode_combo_->setItemData(
        1, QStringLiteral("在窗口内找图，找到后移到该位置再点击；找不到则跳过本行。"),
        Qt::ToolTipRole);
    target_mode_combo_->setToolTip(QStringLiteral("坐标：指定位置点击。\n识图：找到图片后再点击。"));
    target_mode_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    target_mode_combo_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    layout->addWidget(target_mode_combo_);
    connect(target_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ActionRowWidget::OnTargetModeChanged);
    connect(target_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
      const QString tip = target_mode_combo_->currentData(Qt::ToolTipRole).toString();
      if (!tip.isEmpty()) {
        target_mode_combo_->setToolTip(tip);
      }
    });
  }

  point_edit_ = new QLineEdit(this);
  point_edit_->setPlaceholderText(QStringLiteral("x,y"));
  point_edit_->setReadOnly(true);
  point_edit_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  point_edit_->setToolTip(
      QStringLiteral("目标窗口客户区坐标（x,y）。只读，请用「取点」获取。"));
  FitPointEditWidth(point_edit_);
  pick_point_btn_ = new QPushButton(QStringLiteral("取点"), this);
  pick_point_btn_->setToolTip(
      QStringLiteral("获取鼠标在目标窗口客户区内的坐标，填入左侧。\n"
                     "点击后，在目标位置再点一下左键即可（与原版一致）。"));
  screenshot_btn_ = new ScreenshotButton(this);
  screenshot_btn_->setText(QStringLiteral("截图"));
  if (kind_ == ActionKind::Key) {
    screenshot_btn_->setHelpToolTip(
        QStringLiteral("可选：不截图=直接按键；截图=按当前「截图」模式截取后框选（与测试截图同源），识图后按键"));
  } else {
    screenshot_btn_->setHelpToolTip(
        QStringLiteral("按当前「截图」模式截取目标窗口后框选模板图（与测试截图同源）"));
  }

  layout->addWidget(point_edit_);
  layout->addWidget(pick_point_btn_);
  layout->addWidget(screenshot_btn_);

  interval_min_ = new QLineEdit(QStringLiteral("1000"), this);
  interval_min_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  interval_max_ = new QLineEdit(QStringLiteral("1000"), this);
  interval_max_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  FitIntervalEditWidth(interval_min_);
  FitIntervalEditWidth(interval_max_);
  layout->addWidget(new QLabel(QStringLiteral("间隔"), this));
  layout->addWidget(interval_min_);
  layout->addWidget(new QLabel(QStringLiteral("-"), this));
  layout->addWidget(interval_max_);
  const QString interval_tip = QStringLiteral(
      "做完本行后的等待时间（毫秒），再进入下一动作 / 本行下一次。\n"
      "顺序：执行本行 → 等本行间隔 → 下一行。\n"
      "独立：各行先执行一遍，再按本行间隔循环。\n"
      "可填最小～最大，实际随机；录制结果与此语义一致。");
  interval_min_->setToolTip(interval_tip);
  interval_max_->setToolTip(interval_tip);
  layout->addWidget(new QLabel(QStringLiteral("ms"), this));

  layout->addStretch(1);

  remove_btn_ = new QPushButton(this);
  remove_btn_->setObjectName(QStringLiteral("rowDeleteBtn"));
  remove_btn_->setIcon(QIcon(QStringLiteral(":/png/delete.png")));
  remove_btn_->setIconSize(QSize(18, 18));
  remove_btn_->setFlat(true);
  remove_btn_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  remove_btn_->setCursor(Qt::PointingHandCursor);
  remove_btn_->setToolTip(QStringLiteral("删除此行"));
  layout->addWidget(remove_btn_, 0, Qt::AlignRight);

  const QSize icon_size(16, 16);
  pick_point_btn_->setIcon(QIcon(QStringLiteral(":/png/selPt.png")));
  pick_point_btn_->setIconSize(icon_size);
  screenshot_btn_->setIcon(QIcon(QStringLiteral(":/png/image_white.png")));
  screenshot_btn_->setIconSize(icon_size);

  connect(enabled_check_, &QCheckBox::toggled, this, &ActionRowWidget::EmitChanged);
  connect(point_edit_, &QLineEdit::textChanged, this, [this](const QString&) {
    FitPointEditWidth(point_edit_);
    EmitChanged();
  });
  connect(interval_min_, &QLineEdit::textChanged, this, [this](const QString&) {
    FitIntervalEditWidth(interval_min_);
    EmitChanged();
  });
  connect(interval_max_, &QLineEdit::textChanged, this, [this](const QString&) {
    FitIntervalEditWidth(interval_max_);
    EmitChanged();
  });
  connect(remove_btn_, &QPushButton::clicked, this, &ActionRowWidget::removeRequested);
  connect(pick_point_btn_, &QPushButton::clicked, this, [this]() { emit pickPointRequested(this); });
  connect(screenshot_btn_, &ScreenshotButton::screenshotRequested, this,
          [this]() { emit screenshotRequested(this); });
  connect(screenshot_btn_, &ScreenshotButton::localImageRequested, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择图片"), QString(),
        QStringLiteral("Images (*.bmp *.png *.jpg)"));
    if (!path.isEmpty()) {
      SetImagePath(path);
    }
  });
  connect(screenshot_btn_, &ScreenshotButton::clearImageRequested, this, [this]() {
    image_path_.clear();
    EmitChanged();
  });

  // 样式应用后再量一次，避免 QSS 边框导致偏窄、数字被裁切
  QTimer::singleShot(0, this, [this]() {
    FitPointEditWidth(point_edit_);
    FitIntervalEditWidth(interval_min_);
    FitIntervalEditWidth(interval_max_);
  });

  OnTargetModeChanged();
}

void ActionRowWidget::SetIndex(int index) {
  index_label_->setText(QString::number(index + 1));
}

void ActionRowWidget::SetEditingEnabled(bool enabled) {
  setEnabled(enabled);
}

void ActionRowWidget::SetHoldVisible(bool visible) {
  if (hold_check_) {
    hold_check_->setVisible(visible);
  }
}

void ActionRowWidget::SetKeyStrokeVisible(bool visible) {
  if (key_stroke_combo_) {
    key_stroke_combo_->setVisible(visible);
  }
}

void ActionRowWidget::SetMousePressReleaseVisible(bool visible) {
  if (!mouse_button_combo_) {
    return;
  }
  const QString cur = mouse_button_combo_->currentData().toString();
  const bool cur_is_press_release =
      cur == QLatin1String("leftdown") || cur == QLatin1String("leftup") ||
      cur == QLatin1String("rightdown") || cur == QLatin1String("rightup");

  auto is_press_release = [](const QString& d) {
    return d == QLatin1String("leftdown") || d == QLatin1String("leftup") ||
           d == QLatin1String("rightdown") || d == QLatin1String("rightup");
  };

  // 先去掉已有的按下/弹起项，避免重复添加
  for (int i = mouse_button_combo_->count() - 1; i >= 0; --i) {
    if (is_press_release(mouse_button_combo_->itemData(i).toString())) {
      mouse_button_combo_->removeItem(i);
    }
  }

  if (visible) {
    mouse_button_combo_->addItem(QStringLiteral("左键按下"), QStringLiteral("leftdown"));
    mouse_button_combo_->addItem(QStringLiteral("左键弹起"), QStringLiteral("leftup"));
    mouse_button_combo_->addItem(QStringLiteral("右键按下"), QStringLiteral("rightdown"));
    mouse_button_combo_->addItem(QStringLiteral("右键弹起"), QStringLiteral("rightup"));
    if (cur_is_press_release) {
      const int bi = mouse_button_combo_->findData(cur);
      if (bi >= 0) {
        mouse_button_combo_->setCurrentIndex(bi);
      }
    }
  } else if (cur_is_press_release) {
    const int left = mouse_button_combo_->findData(QStringLiteral("left"));
    if (left >= 0) {
      mouse_button_combo_->setCurrentIndex(left);
    }
  }
}

bool ActionRowWidget::HasSequentialPressRelease() const {
  if (key_stroke_combo_) {
    const QString d = key_stroke_combo_->currentData().toString();
    return d == QLatin1String("down") || d == QLatin1String("up");
  }
  if (mouse_button_combo_) {
    const QString d = mouse_button_combo_->currentData().toString();
    return d == QLatin1String("leftdown") || d == QLatin1String("leftup") ||
           d == QLatin1String("rightdown") || d == QLatin1String("rightup");
  }
  return false;
}

void ActionRowWidget::ConvertPressReleaseForIndependent() {
  if (key_stroke_combo_) {
    const QString d = key_stroke_combo_->currentData().toString();
    if (d == QLatin1String("down") || d == QLatin1String("up")) {
      const int i = key_stroke_combo_->findData(QStringLiteral("press"));
      if (i >= 0) {
        key_stroke_combo_->setCurrentIndex(i);
      }
    }
  }
  if (mouse_button_combo_) {
    const QString d = mouse_button_combo_->currentData().toString();
    QString target;
    if (d == QLatin1String("leftdown") || d == QLatin1String("leftup")) {
      target = QStringLiteral("left");
    } else if (d == QLatin1String("rightdown") || d == QLatin1String("rightup")) {
      target = QStringLiteral("right");
    }
    if (!target.isEmpty()) {
      const int i = mouse_button_combo_->findData(target);
      if (i >= 0) {
        mouse_button_combo_->setCurrentIndex(i);
      }
    }
  }
}

void ActionRowWidget::OnTargetModeChanged() {
  RebuildTargetUi();
  EmitChanged();
}

void ActionRowWidget::RebuildTargetUi() {
  if (kind_ == ActionKind::Key) {
    // 按键行始终可选截图；无坐标
    point_edit_->setVisible(false);
    pick_point_btn_->setVisible(false);
    screenshot_btn_->setVisible(true);
    return;
  }
  const QString mode = target_mode_combo_->currentData().toString();
  const bool coord = mode == QLatin1String("coordinate");
  const bool pic = mode == QLatin1String("findpic");
  point_edit_->setVisible(coord);
  pick_point_btn_->setVisible(coord);
  screenshot_btn_->setVisible(pic);
}

void ActionRowWidget::EmitChanged() { emit changed(); }

void ActionRowWidget::SetImagePath(const QString& path) {
  image_path_ = path;
  if (screenshot_btn_ && !path.isEmpty()) {
    screenshot_btn_->setScreenshotImage(path);
  }
  EmitChanged();
}

void ActionRowWidget::SetPoint(int x, int y) {
  point_edit_->setText(QStringLiteral("%1,%2").arg(x).arg(y));
  FitPointEditWidth(point_edit_);
  EmitChanged();
}

void ActionRowWidget::SetPointHintPending() {
  point_edit_->setText(QStringLiteral("请点击目标位置"));
  FitPointEditWidth(point_edit_);
}

ActionRow ActionRowWidget::ToRow() const {
  ActionRow row;
  row.enabled = enabled_check_->isChecked();
  row.kind = kind_;
  row.interval_min_ms = interval_min_->text().toInt();
  row.interval_max_ms = interval_max_->text().toInt();
  if (row.interval_min_ms <= 0) row.interval_min_ms = 1000;
  if (row.interval_max_ms <= 0) row.interval_max_ms = row.interval_min_ms;

  if (kind_ == ActionKind::Key) {
    // 无图 → 标准版直接按键；有图 → 识图后按键
    if (!image_path_.isEmpty()) {
      row.target.mode = TargetMode::FindPic;
      row.target.image_path = image_path_;
    } else {
      row.target.mode = TargetMode::None;
    }
    row.shortcut = key_capture_ ? key_capture_->storageText() : QString();
    row.hold = hold_check_ && hold_check_->isChecked();
    if (key_stroke_combo_) {
      row.key_stroke = KeyStrokeFromString(key_stroke_combo_->currentData().toString());
    }
    return row;
  }

  const QString mode = target_mode_combo_->currentData().toString();
  row.target.mode = TargetModeFromString(mode);
  if (row.target.mode == TargetMode::Coordinate) {
    const QStringList parts = point_edit_->text().split(QLatin1Char(','));
    if (parts.size() >= 2) {
      row.target.x = parts[0].trimmed().toInt();
      row.target.y = parts[1].trimmed().toInt();
    }
  } else if (row.target.mode == TargetMode::FindPic) {
    row.target.image_path = image_path_;
  }
  row.button = MouseButtonFromString(mouse_button_combo_->currentData().toString());
  return row;
}

void ActionRowWidget::FromRow(const ActionRow& row) {
  enabled_check_->setChecked(row.enabled);
  interval_min_->setText(QString::number(row.interval_min_ms));
  interval_max_->setText(QString::number(row.interval_max_ms));
  FitIntervalEditWidth(interval_min_);
  FitIntervalEditWidth(interval_max_);

  point_edit_->setText(QStringLiteral("%1,%2").arg(row.target.x).arg(row.target.y));
  FitPointEditWidth(point_edit_);
  image_path_ = row.target.image_path;
  if (!image_path_.isEmpty()) {
    screenshot_btn_->setScreenshotImage(image_path_);
  } else if (screenshot_btn_) {
    screenshot_btn_->clearScreenshotImage();
  }

  if (kind_ == ActionKind::Key) {
    if (key_capture_) {
      key_capture_->setFromStorageText(row.shortcut);
    }
    if (hold_check_) {
      hold_check_->setChecked(row.hold);
    }
    if (key_stroke_combo_) {
      const int si = key_stroke_combo_->findData(KeyStrokeToString(row.key_stroke));
      if (si >= 0) {
        key_stroke_combo_->setCurrentIndex(si);
      }
    }
    // 兼容旧数据：坐标模式按键行 → 直接按键；识图无图 → 直接按键
  } else if (mouse_button_combo_) {
    QString mode = TargetModeToString(row.target.mode);
    if (mode == QLatin1String("none")) {
      mode = QStringLiteral("coordinate");
    }
    int mode_index = target_mode_combo_->findData(mode);
    if (mode_index < 0) mode_index = 0;
    target_mode_combo_->setCurrentIndex(mode_index);
    const int bi = mouse_button_combo_->findData(MouseButtonToString(row.button));
    if (bi >= 0) mouse_button_combo_->setCurrentIndex(bi);
  }
  RebuildTargetUi();
}
