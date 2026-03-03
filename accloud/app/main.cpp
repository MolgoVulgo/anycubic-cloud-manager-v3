#include "App.h"

#include <string_view>

#if defined(ACCLOUD_WITH_QT)
#include <QGuiApplication>
#include <QQmlApplicationEngine>
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

} // namespace

int main(int argc, char** argv) {
  if (hasArg(argc, argv, "--smoke")) {
    accloud::App app;
    return app.run(argc, argv);
  }

#if defined(ACCLOUD_WITH_QT)
  QGuiApplication app(argc, argv);
  QQmlApplicationEngine engine;
  engine.load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));
  if (engine.rootObjects().isEmpty()) {
    return 1;
  }
  return app.exec();
#else
  accloud::App app;
  return app.run(argc, argv);
#endif
}
