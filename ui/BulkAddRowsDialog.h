#pragma once

#include "../model/ActionRow.h"

#include <QDialog>
#include <QVector>

class QLabel;
class QRadioButton;
class QTextEdit;

class BulkAddRowsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit BulkAddRowsDialog(QWidget* parent = nullptr);

  ActionKind SelectedKind() const;
  bool AppendMode() const;
  QVector<ActionRow> ParsedRows() const;

 private:
  void OnLoadFile();
  void OnKindChanged();

  QRadioButton* key_radio_ = nullptr;
  QRadioButton* mouse_radio_ = nullptr;
  QRadioButton* append_radio_ = nullptr;
  QRadioButton* replace_radio_ = nullptr;
  QLabel* hint_label_ = nullptr;
  QTextEdit* text_edit_ = nullptr;
};
