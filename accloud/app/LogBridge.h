#pragma once

#include <QObject>
#include <QVariantMap>

namespace accloud {

class LogBridge : public QObject {
  Q_OBJECT

 public:
  explicit LogBridge(QObject* parent = nullptr);

  // Retourne un snapshot multi-sources:
  // {
  //   ok: bool,
  //   message: string,
  //   logDir: string,
  //   totalEntries: number,
  //   sources: [string],
  //   components: [string],
  //   events: [string],
  //   entries: [{sink, ts, level, source, component, event, opId, message, formatted}]
  // }
  Q_INVOKABLE QVariantMap fetchSnapshot(int maxLines = 1000) const;
};

} // namespace accloud
