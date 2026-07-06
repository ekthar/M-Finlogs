#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mfinlogs::app {

struct XlsxRow final {
    QDate date;
    QString billNo;
    QString party;
    QString type;
    QString mode;
    double amount = 0.0;
    QString error;
};

struct XlsxParseResult final {
    QVector<XlsxRow> rows;
    QString error;
};

class XlsxReader final {
public:
    static XlsxParseResult read(const QString& path);
};

} // namespace mfinlogs::app
