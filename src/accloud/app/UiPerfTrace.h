#pragma once

#include "infra/logging/JsonlLogger.h"

#include <QElapsedTimer>
#include <QString>

#include <cstdlib>
#include <map>
#include <string>

namespace accloud {

class UiPerfTrace {
 public:
  explicit UiPerfTrace(std::string operation)
      : m_operation(std::move(operation)),
        m_enabled(isEnabled()) {
    if (m_enabled) {
      m_timer.start();
    }
  }

  ~UiPerfTrace() {
    if (!m_enabled) {
      return;
    }
    m_fields["operation"] = m_operation;
    m_fields["elapsed_ms"] = std::to_string(m_timer.elapsed());
    logging::info("app", "ui_perf", "operation_timing", "UI performance trace", std::move(m_fields));
  }

  UiPerfTrace(const UiPerfTrace&) = delete;
  UiPerfTrace& operator=(const UiPerfTrace&) = delete;

  void setField(std::string key, std::string value) {
    if (!m_enabled) {
      return;
    }
    m_fields[std::move(key)] = std::move(value);
  }

  void setField(std::string key, const QString& value) {
    setField(std::move(key), value.toStdString());
  }

  void setField(std::string key, const char* value) {
    setField(std::move(key), std::string(value != nullptr ? value : ""));
  }

  void setCount(std::string key, qsizetype value) {
    setField(std::move(key), std::to_string(value));
  }

 private:
  static bool isEnabled() {
    const char* raw = std::getenv("ACCLOUD_UI_PERF_TRACE");
    return raw != nullptr && *raw != '\0' && std::string(raw) != "0";
  }

  std::string m_operation;
  bool m_enabled{false};
  QElapsedTimer m_timer;
  logging::FieldMap m_fields;
};

} // namespace accloud
