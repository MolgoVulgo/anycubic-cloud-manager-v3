#pragma once

#include "infra/logging/JsonlLogger.h"

#include <QMutex>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariantMap>

namespace accloud::cache {

extern QMutex g_dbMutex;

QString resolveDbPath();
qint64 readNow();
QString newConnectionName();
void closeAndRemoveDatabase(QSqlDatabase& db, const QString& connectionName);
QVariantMap decodePayloadText(const QString& payload);
QVariantMap printerDetailsFromColumns(const QSqlQuery& query, int firstColumn);
QVariantMap mergePrinterDetails(const QVariantMap& existing, const QVariantMap& incoming);
QString nonNullText(const QVariantMap& map,
                    const QString& key,
                    const QString& fallback = QStringLiteral(""));
QString detailString(const QVariantMap& details, const QString& key);
int detailInt(const QVariantMap& details, const QString& key);
QString encodePayloadMap(const QVariantMap& map);
logging::FieldMap sqlErrorFields(const QSqlError& error,
                                 logging::FieldMap fields = {});
void enforceMaxRows(QSqlDatabase& db,
                    const QString& table,
                    const QString& idColumn,
                    int maxRows);
bool runSchema(QSqlDatabase& db);

} // namespace accloud::cache
