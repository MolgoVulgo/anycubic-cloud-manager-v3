#include "app/CloudFilesModel.h"
#include "app/PrintersModel.h"
#include "app/PrinterFilesModel.h"
#include "app/RecentJobsModel.h"

#if defined(ACCLOUD_EXPERIMENTAL_VIEWER)
#include "render3d/qtquick/QmlGlItem.h"
#endif

#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

class AccloudUiTestSetup : public QObject {
  Q_OBJECT

 public slots:
  void qmlEngineAvailable(QQmlEngine*) {
    qmlRegisterType<accloud::CloudFilesModel>("Accloud.Models", 1, 0, "CloudFilesModel");
    qmlRegisterType<accloud::PrintersModel>("Accloud.Models", 1, 0, "PrintersModel");
    qmlRegisterType<accloud::PrinterFilesModel>("Accloud.Models", 1, 0, "PrinterFilesModel");
    qmlRegisterType<accloud::RecentJobsModel>("Accloud.Models", 1, 0, "RecentJobsModel");
#if defined(ACCLOUD_EXPERIMENTAL_VIEWER)
    qmlRegisterType<accloud::render3d::QmlGlItem>(
        "Accloud.Render3D", 1, 0, "VolumeViewer");
#endif
  }
};

QUICK_TEST_MAIN_WITH_SETUP(accloud_ui, AccloudUiTestSetup)

#include "tst_ui_main.moc"
