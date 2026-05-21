#pragma once

#include <QObject>

class QEvent;

namespace accloud {

class UiClickTracer : public QObject {
  Q_OBJECT

 public:
  explicit UiClickTracer(QObject* parent = nullptr);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace accloud
