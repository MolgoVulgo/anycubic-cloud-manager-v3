#pragma once

#include <QDir>
#include <QString>

#include "infra/config/AppPaths.h"

namespace accloud::app {

inline QString userLocalRoot() {
  return QString::fromStdString(accloud::config::rootDir().string());
}

inline QString userSettingsIniPath() {
  return QString::fromStdString(accloud::config::settingsPath().string());
}

inline QString userCacheDbPath() {
  return QString::fromStdString(accloud::config::dbPath().string());
}

inline QString ensureUserLocalRoot() {
  const QString root = userLocalRoot();
  QDir().mkpath(root);
  return root;
}

} // namespace accloud::app
