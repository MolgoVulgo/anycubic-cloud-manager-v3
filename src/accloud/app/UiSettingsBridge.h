#pragma once

#include <QObject>
#include <QString>

class QSettings;

namespace accloud {

class UiSettingsBridge : public QObject {
  Q_OBJECT

 public:
  explicit UiSettingsBridge(QObject* parent = nullptr);
  ~UiSettingsBridge() override;

  Q_INVOKABLE QString getString(const QString& key, const QString& defaultValue = {}) const;
  Q_INVOKABLE void setString(const QString& key, const QString& value);
  Q_INVOKABLE bool hasKey(const QString& key) const;
  Q_INVOKABLE void removeKey(const QString& key);
  Q_INVOKABLE void sync();

 private:
  QSettings* m_settings{nullptr};
};

} // namespace accloud
