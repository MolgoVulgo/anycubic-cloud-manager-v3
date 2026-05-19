#include "app/CloudFilesModel.h"
#include "app/PrintersModel.h"
#include "app/RecentJobsModel.h"

#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

class AccloudUiTestSetup : public QObject {
  Q_OBJECT

 public slots:
  void qmlEngineAvailable(QQmlEngine*) {
    qmlRegisterType<accloud::CloudFilesModel>("Accloud.Models", 1, 0, "CloudFilesModel");
    qmlRegisterType<accloud::PrintersModel>("Accloud.Models", 1, 0, "PrintersModel");
    qmlRegisterType<accloud::RecentJobsModel>("Accloud.Models", 1, 0, "RecentJobsModel");
  }
};

QUICK_TEST_MAIN_WITH_SETUP(accloud_ui, AccloudUiTestSetup)

#include "tst_ui_main.moc"
