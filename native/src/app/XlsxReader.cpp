#include "app/XlsxReader.h"

#include <QDir>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QUuid>
#include <QXmlStreamReader>

namespace mfinlogs::app {

static QStringList parseCsvLine(const QString& line) {
    QStringList values;
    QString current;
    bool quoted = false;
    for (int index = 0; index < line.size(); ++index) {
        const QChar c = line.at(index);
        if (c == QLatin1Char('"')) {
            if (quoted && index + 1 < line.size() && line.at(index + 1) == QLatin1Char('"')) {
                current.append(QLatin1Char('"'));
                ++index;
            } else {
                quoted = !quoted;
            }
        } else if (c == QLatin1Char(',') && !quoted) {
            values.append(current.trimmed());
            current.clear();
        } else {
            current.append(c);
        }
    }
    values.append(current.trimmed());
    return values;
}

static int columnIndex(const QString& ref) {
    int result = 0;
    for (const QChar& c : ref) {
        if (c.isLetter()) {
            result = result * 26 + (c.toUpper().unicode() - 'A' + 1);
        }
    }
    return result - 1;
}

static QStringList readSharedStrings(QXmlStreamReader& xml) {
    QStringList strings;
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("t")) {
            strings.append(xml.readElementText());
        }
    }
    return strings;
}

struct CellValue final {
    bool isSharedString = false;
    bool isNumeric = false;
    double number = 0.0;
    int stringIndex = 0;
};

XlsxParseResult XlsxReader::read(const QString& path) {
    XlsxParseResult result;

    const QString tempDir = QDir::temp().absoluteFilePath(
        QStringLiteral("mfinlogs_xlsx_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    QDir().mkpath(tempDir);

    QProcess ps;
    ps.setProgram(QStringLiteral("powershell"));
    ps.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-Command"),
        QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2'").arg(path, tempDir)
    });
    ps.start();
    if (!ps.waitForFinished(30000) || ps.exitCode() != 0) {
        QDir(tempDir).removeRecursively();
        result.error = QStringLiteral("Failed to unzip xlsx. Ensure the file is a valid Excel .xlsx file.");
        return result;
    }

    const QString ssPath = tempDir + QStringLiteral("/xl/sharedStrings.xml");
    const QString sheetPath = tempDir + QStringLiteral("/xl/worksheets/sheet1.xml");
    if (!QFile::exists(ssPath) || !QFile::exists(sheetPath)) {
        QDir(tempDir).removeRecursively();
        result.error = QStringLiteral("Invalid xlsx format: missing sharedStrings.xml or sheet1.xml");
        return result;
    }

    QStringList sharedStrings;
    {
        QFile ssFile(ssPath);
        if (ssFile.open(QIODevice::ReadOnly)) {
            QXmlStreamReader xml(&ssFile);
            sharedStrings = readSharedStrings(xml);
        }
    }

    if (sharedStrings.isEmpty()) {
        QDir(tempDir).removeRecursively();
        result.error = QStringLiteral("Could not read shared strings from xlsx");
        return result;
    }

    QMap<int, CellValue> cells;
    {
        QFile sheetFile(sheetPath);
        if (!sheetFile.open(QIODevice::ReadOnly)) {
            QDir(tempDir).removeRecursively();
            result.error = QStringLiteral("Could not open sheet1.xml");
            return result;
        }

        QXmlStreamReader xml(&sheetFile);
        int currentRow = 0;
        int currentCol = 0;
        CellValue currentCell;

        auto flushCell = [&]() {
            if (currentRow > 0 && currentCol >= 0) {
                cells.insert((currentRow - 1) * 1000 + currentCol, currentCell);
            }
            currentCell = CellValue();
        };

        while (!xml.atEnd() && !xml.hasError()) {
            const QXmlStreamReader::TokenType token = xml.readNext();
            if (token == QXmlStreamReader::StartElement) {
                const QStringRef name = xml.name();
                if (name == QStringLiteral("row")) {
                    flushCell();
                    currentRow = xml.attributes().value(QStringLiteral("r")).toInt();
                    currentCol = 0;
                } else if (name == QStringLiteral("c")) {
                    flushCell();
                    const QString ref = xml.attributes().value(QStringLiteral("r")).toString();
                    currentCol = columnIndex(ref);
                    const QString t = xml.attributes().value(QStringLiteral("t")).toString();
                    if (t == QStringLiteral("s")) {
                        currentCell.isSharedString = true;
                    } else if (t != QStringLiteral("str") && t != QStringLiteral("inlineStr")) {
                        currentCell.isNumeric = true;
                    }
                } else if (name == QStringLiteral("v")) {
                    const QString text = xml.readElementText();
                    if (currentCell.isSharedString) {
                        currentCell.stringIndex = text.toInt();
                    } else if (currentCell.isNumeric) {
                        currentCell.number = text.toDouble();
                    }
                }
            } else if (token == QXmlStreamReader::EndElement) {
                if (xml.name() == QStringLiteral("row")) {
                    flushCell();
                }
            }
        }
    }

    auto cellText = [&](int row, int col) -> QString {
        const int key = row * 1000 + col;
        auto it = cells.constFind(key);
        if (it == cells.constEnd()) return QString();
        const CellValue& cv = it.value();
        if (cv.isSharedString) {
            return (cv.stringIndex >= 0 && cv.stringIndex < sharedStrings.size())
                ? sharedStrings.at(cv.stringIndex) : QString();
        }
        if (cv.isNumeric) {
            return QString::number(cv.number, 'f', 10);
        }
        return QString();
    };

    auto cellNumber = [&](int row, int col) -> double {
        const int key = row * 1000 + col;
        auto it = cells.constFind(key);
        if (it == cells.constEnd()) return 0.0;
        return it.value().number;
    };

    auto isNumeric = [&](int row, int col) -> bool {
        const int key = row * 1000 + col;
        auto it = cells.constFind(key);
        return it != cells.constEnd() && it.value().isNumeric;
    };

    const int headerRow = 0;
    QStringList headers;
    for (int col = 0; col < 20; ++col) {
        const QString val = cellText(headerRow, col);
        if (!val.isEmpty()) {
            headers.append(val.trimmed().toLower());
        }
    }

    QMap<QString, int> colMap;
    for (int i = 0; i < headers.size(); ++i) {
        const QString h = headers.at(i);
        if (h == QStringLiteral("date") || h == QStringLiteral("txn_date")) colMap.insert(QStringLiteral("date"), i);
        else if (h == QStringLiteral("bill_no") || h == QStringLiteral("billno") || h == QStringLiteral("bill")) colMap.insert(QStringLiteral("bill_no"), i);
        else if (h == QStringLiteral("party") || h == QStringLiteral("customer") || h == QStringLiteral("customer name") || h == QStringLiteral("name")) colMap.insert(QStringLiteral("party"), i);
        else if (h == QStringLiteral("type") || h == QStringLiteral("txn_type") || h == QStringLiteral("transaction type")) colMap.insert(QStringLiteral("type"), i);
        else if (h == QStringLiteral("mode") || h == QStringLiteral("payment_mode") || h == QStringLiteral("payment mode")) colMap.insert(QStringLiteral("mode"), i);
        else if (h == QStringLiteral("amount")) colMap.insert(QStringLiteral("amount"), i);
    }

    const QStringList required = {QStringLiteral("date"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")};
    for (const QString& key : required) {
        if (!colMap.contains(key)) {
            QDir(tempDir).removeRecursively();
            result.error = QStringLiteral("XLSX file is missing required column: %1").arg(key);
            return result;
        }
    }

    auto excelSerialToDate = [](double serial) -> QDate {
        if (serial < 1.0) return QDate();
        return QDate(1899, 12, 30).addDays(static_cast<int>(serial));
    };

    auto parseDate = [&](int row, int col) -> QDate {
        if (isNumeric(row, col)) {
            return excelSerialToDate(cellNumber(row, col));
        }
        const QString text = cellText(row, col);
        QDate d = QDate::fromString(text, Qt::ISODate);
        if (d.isValid()) return d;
        d = QDate::fromString(text, QStringLiteral("dd/MM/yyyy"));
        if (d.isValid()) return d;
        d = QDate::fromString(text, QStringLiteral("MM/dd/yyyy"));
        if (d.isValid()) return d;
        d = QDate::fromString(text, QStringLiteral("d/M/yyyy"));
        if (d.isValid()) return d;
        d = QDate::fromString(text, QStringLiteral("M/d/yyyy"));
        if (d.isValid()) return d;
        d = QDate::fromString(text, QStringLiteral("yyyy-MM-dd"));
        if (d.isValid()) return d;
        d = QDate::fromString(text, QStringLiteral("dd-MM-yyyy"));
        if (d.isValid()) return d;
        return QDate();
    };

    int maxRow = 0;
    for (auto it = cells.constBegin(); it != cells.constEnd(); ++it) {
        int row = it.key() / 1000;
        if (row > maxRow) maxRow = row;
    }

    for (int row = 1; row <= maxRow; ++row) {
        XlsxRow xrow;
        xrow.date = parseDate(row, colMap.value(QStringLiteral("date")));
        xrow.billNo = cellText(row, colMap.value(QStringLiteral("bill_no"), -1));
        xrow.party = cellText(row, colMap.value(QStringLiteral("party")));
        xrow.type = cellText(row, colMap.value(QStringLiteral("type")));
        xrow.mode = cellText(row, colMap.value(QStringLiteral("mode")));
        xrow.amount = cellNumber(row, colMap.value(QStringLiteral("amount")));
        if (xrow.amount == 0.0) {
            bool ok = false;
            xrow.amount = cellText(row, colMap.value(QStringLiteral("amount"))).toDouble(&ok);
        }
        result.rows.append(xrow);
    }

    QDir(tempDir).removeRecursively();
    return result;
}

} // namespace mfinlogs::app
