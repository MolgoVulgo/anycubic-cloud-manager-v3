#include "App.h"
#include "infra/logging/JsonlLogger.h"

#include <cstdlib>
#include <exception>
#include <string_view>

#if defined(ACCLOUD_WITH_QT)
#include "CloudBridge.h"
#include "LogBridge.h"
#include "SessionImportBridge.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#endif

namespace {

bool hasArg(int argc, char** argv, std::string_view flag) {
  for (int i = 1; i < argc; ++i) {
    if (flag == argv[i]) {
      return true;
    }
  }
  return false;
}

[[noreturn]] void terminateHandler() {
  std::string reason = "non-standard exception";
  if (std::exception_ptr current = std::current_exception(); current != nullptr) {
    try {
      std::rethrow_exception(current);
    } catch (const std::exception& ex) {
      reason = ex.what();
    } catch (...) {
      reason = "unknown exception";
    }
  } else {
    reason = "terminate called without active exception";
  }

  accloud::logging::fatal("app", "runtime", "terminate",
                          "Unhandled exception triggered std::terminate", {{"reason", reason}});
  std::abort();
}

#if defined(ACCLOUD_WITH_QT)
void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
  accloud::logging::Level level = accloud::logging::Level::kInfo;
  std::string event = "qt_info";
  switch (type) {
    case QtDebugMsg:
      level = accloud::logging::Level::kDebug;
      event = "qt_debug";
      break;
    case QtInfoMsg:
      level = accloud::logging::Level::kInfo;
      event = "qt_info";
      break;
    case QtWarningMsg:
      level = accloud::logging::Level::kWarn;
      event = "qt_warning";
      break;
    case QtCriticalMsg:
      level = accloud::logging::Level::kError;
      event = "qt_critical";
      break;
    case QtFatalMsg:
      level = accloud::logging::Level::kFatal;
      event = "qt_fatal";
      break;
  }

  accloud::logging::FieldMap fields;
  if (context.file != nullptr && *context.file != '\0') {
    fields["file"] = context.file;
  }
  if (context.function != nullptr && *context.function != '\0') {
    fields["function"] = context.function;
  }
  if (context.line > 0) {
    fields["line"] = std::to_string(context.line);
  }
  std::string component = "qt";
  if (context.category != nullptr && *context.category != '\0') {
    component = context.category;
    fields["category"] = component;
  }

  accloud::logging::log(level, "qt", std::move(component), std::move(event), message.toStdString(),
                        std::move(fields));
}
#endif

} // namespace

int main(int argc, char** argv) {
  accloud::logging::initialize();
  std::set_terminate(terminateHandler);

  accloud::logging::info("app", "bootstrap", "startup",
                         "Application bootstrap started",
                         {{"argc", std::to_string(argc)},
                          {"mode", hasArg(argc, argv, "--import-har") ? "import" : "default"}});

  int exitCode = 0;
  if (hasArg(argc, argv, "--smoke") || hasArg(argc, argv, "--import-har")) {
    accloud::logging::info("app", "bootstrap", "cli_mode", "Starting CLI mode");
    accloud::App app;
    exitCode = app.run(argc, argv);
    accloud::logging::info("app", "bootstrap", "shutdown", "CLI mode finished",
                           {{"exit_code", std::to_string(exitCode)}});
    accloud::logging::shutdown();
    return exitCode;
  }

#if defined(ACCLOUD_WITH_QT)
  QGuiApplication app(argc, argv);
  qInstallMessageHandler(qtMessageHandler);
  accloud::logging::info("app", "bootstrap", "qt_initialized", "Qt GUI initialized",
                         {{"log_dir", accloud::logging::logDirectory().string()}});

  QQmlApplicationEngine engine;
  accloud::SessionImportBridge sessionImportBridge;
  accloud::CloudBridge cloudBridge;
  accloud::LogBridge logBridge;
  engine.rootContext()->setContextProperty("sessionImportBridge", &sessionImportBridge);
  engine.rootContext()->setContextProperty("cloudBridge", &cloudBridge);
  engine.rootContext()->setContextProperty("logBridge", &logBridge);
  engine.load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));
  if (engine.rootObjects().isEmpty()) {
    accloud::logging::error("app", "bootstrap", "qml_load_failed",
                            "No QML root object returned by engine");
    exitCode = 1;
  } else {
    accloud::logging::info("app", "bootstrap", "qml_loaded", "MainWindow QML loaded");
    exitCode = app.exec();
  }
#else
  accloud::logging::warn("app", "bootstrap", "headless_mode", "Qt disabled, running headless CLI");
  accloud::App app;
  exitCode = app.run(argc, argv);
#endif

  accloud::logging::info("app", "bootstrap", "shutdown", "Application terminated",
                         {{"exit_code", std::to_string(exitCode)}});
  accloud::logging::shutdown();
  return exitCode;
}
