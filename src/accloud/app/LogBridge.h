#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QVariantMap>

namespace accloud {

class LogTailModel;

class LogBridge : public QObject {
  Q_OBJECT
  Q_PROPERTY(QAbstractListModel* tailModel READ tailModel CONSTANT)

 public:
  explicit LogBridge(QObject* parent = nullptr);
  QAbstractListModel* tailModel();

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

 private:
  LogTailModel* m_tailModel{nullptr};
};

} // namespace accloud
