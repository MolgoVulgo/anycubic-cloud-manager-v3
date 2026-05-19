#include "app/CloudFilesModel.h"
#include "app/MqttTailModel.h"
#include "app/PrintersModel.h"
#include "app/RecentJobsModel.h"

#include <QCoreApplication>

#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    return false;
  }
  return true;
}

QVariantMap file(QString id, QString name) {
  QVariantMap out;
  out.insert(QStringLiteral("fileId"), id);
  out.insert(QStringLiteral("fileName"), name);
  out.insert(QStringLiteral("sizeText"), QStringLiteral("1 MB"));
  out.insert(QStringLiteral("status"), QStringLiteral("READY"));
  return out;
}

bool test_cloud_files_append_keeps_paging_contract() {
  accloud::CloudFilesModel model;
  model.setPageSize(2);

  QVariantList files;
  files.append(file(QStringLiteral("f1"), QStringLiteral("one.pwmb")));
  files.append(file(QStringLiteral("f2"), QStringLiteral("two.pws")));
  model.replaceFiles(files);

  bool ok = expect(model.count() == 2, "initial visible count should be 2")
      && expect(model.visibleCount() == 2, "initial total visible count should be 2");

  model.append(file(QStringLiteral("f3"), QStringLiteral("three.photon")));
  ok = ok
      && expect(model.count() == 2, "append on full first page should not add visible row")
      && expect(model.visibleCount() == 3, "append should update visibleCount")
      && expect(model.totalPages() == 2, "append should update total pages");

  model.setCurrentPage(1);
  ok = ok
      && expect(model.count() == 1, "second page should show appended row")
      && expect(model.get(0).value(QStringLiteral("fileId")).toString() == QStringLiteral("f3"),
                "second page should contain appended file");

  model.setTypeFilter(QStringLiteral("pwmb"));
  model.append(file(QStringLiteral("f4"), QStringLiteral("four.pwmb")));
  ok = ok
      && expect(model.visibleCount() == 2, "filtered append should rebuild visible rows")
      && expect(model.count() == 2, "filtered append should expose matching row");
  return ok;
}

bool test_mqtt_tail_append_preserves_tail_contract() {
  accloud::MqttTailModel model;
  model.appendMessage(QStringLiteral("t1"),
                      QStringLiteral("topic/a"),
                      QStringLiteral("{}"),
                      2,
                      QStringLiteral("line-a"));
  bool ok = expect(model.count() == 1, "mqtt tail append should add visible row")
      && expect(model.data(model.index(0, 0), accloud::MqttTailModel::TopicRole).toString()
                    == QStringLiteral("topic/a"),
                "mqtt tail topic should be readable");

  model.setTopicFilter(QStringLiteral("missing"));
  model.appendMessage(QStringLiteral("t2"),
                      QStringLiteral("topic/b"),
                      QStringLiteral("{}"),
                      2,
                      QStringLiteral("line-b"));
  ok = ok && expect(model.count() == 0, "filtered-out append should not become visible");
  return ok;
}

bool test_patch_models_preserve_identity_updates() {
  accloud::PrintersModel printers;
  QVariantMap p1;
  p1.insert(QStringLiteral("id"), QStringLiteral("p1"));
  p1.insert(QStringLiteral("name"), QStringLiteral("Printer"));
  QVariantList printerList;
  printerList.append(p1);
  bool ok = expect(printers.replaceOrPatchPrinters(printerList), "initial printers replace should change");
  p1.insert(QStringLiteral("state"), QStringLiteral("PRINTING"));
  QVariantList patchedPrinters;
  patchedPrinters.append(p1);
  ok = ok
      && expect(printers.replaceOrPatchPrinters(patchedPrinters), "printer patch should change")
      && expect(printers.count() == 1, "printer patch should preserve row count")
      && expect(printers.get(0).value(QStringLiteral("state")).toString() == QStringLiteral("PRINTING"),
                "printer patch should update state");

  accloud::RecentJobsModel jobs;
  QVariantMap job;
  job.insert(QStringLiteral("taskId"), QStringLiteral("j1"));
  job.insert(QStringLiteral("gcodeName"), QStringLiteral("old.pwmb"));
  QVariantList jobList;
  jobList.append(job);
  ok = ok && expect(jobs.replaceOrPatchJobs(jobList), "initial jobs replace should change");
  job.insert(QStringLiteral("gcodeName"), QStringLiteral("new.pwmb"));
  QVariantList patchedJobs;
  patchedJobs.append(job);
  ok = ok
      && expect(jobs.replaceOrPatchJobs(patchedJobs), "job patch should change")
      && expect(jobs.count() == 1, "job patch should preserve row count")
      && expect(jobs.get(0).value(QStringLiteral("gcodeName")).toString() == QStringLiteral("new.pwmb"),
                "job patch should update gcode name");
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  bool ok = true;
  ok = test_cloud_files_append_keeps_paging_contract() && ok;
  ok = test_mqtt_tail_append_preserves_tail_contract() && ok;
  ok = test_patch_models_preserve_identity_updates() && ok;
  return ok ? 0 : 1;
}
