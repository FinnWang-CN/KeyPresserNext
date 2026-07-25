#pragma once

#include "ActionRow.h"

#include <QString>
#include <QVector>

namespace BulkRowImporter {

bool ParseKeyLine(const QString& line, ActionRow* out);
bool ParseMouseLine(const QString& line, ActionRow* out);
QVector<ActionRow> ParseText(const QString& text, ActionKind kind);

}  // namespace BulkRowImporter
