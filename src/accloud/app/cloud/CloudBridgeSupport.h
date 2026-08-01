#pragma once

#include "infra/cloud/CloudClient.h"
#include "app/realtime/PrinterRealtimeStore.h"

#include <QString>
#include <QVariantMap>

#include <map>
#include <string>

namespace accloud::cloud_bridge_support {

QString normalizeUploadLocalPath(const QString& pathOrUrl);
std::string compactJsonFromVariantMap(const QVariantMap& data);
QVariantMap fileInfoToMap(const cloud::CloudFileInfo& file);
QVariantMap printerInfoToMap(const cloud::CloudPrinterInfo& printer);
QVariantMap printerCompatToMap(const cloud::CloudPrinterCompatItem& printer);
void applyRealtimeOverlayToPrinterMap(
    QVariantMap& printer,
    const std::map<std::string, realtime::PrinterRealtimeSnapshot>& snapshots);
QVariantMap printerDetailsToMap(const cloud::CloudPrinterDetailsResult& details);
QVariantMap reasonCatalogItemToMap(const cloud::CloudReasonCatalogItem& item);
QVariantMap printerProjectToMap(const cloud::CloudPrinterProjectItem& item);
void finalizeUiMessage(QVariantMap& out);

} // namespace accloud::cloud_bridge_support
