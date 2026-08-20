#include "app/SupportAnalysisBridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>

namespace {

bool writeJson(const QString& path, const QJsonObject& object) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  return file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) > 0;
}

bool near(double lhs, double rhs, double epsilon = 1e-9) {
  return std::abs(lhs - rhs) <= epsilon;
}

int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

} // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  QTemporaryDir bundle;
  if (!bundle.isValid()) {
    return fail("unable to create temporary support-analysis bundle");
  }
  QDir root(bundle.path());
  if (!root.mkpath(QStringLiteral("images"))
      || !root.mkpath(QStringLiteral("layers"))) {
    return fail("unable to create temporary bundle directories");
  }

  constexpr int width = 10;
  constexpr int height = 10;
  QImage pick(width, height, QImage::Format_RGB32);
  pick.fill(qRgb(0, 0, 0));
  // component_id 4 is encoded as selection id 5 (RGB24 value 0x000005).
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      pick.setPixel(x, y, qRgb(0, 0, 5));
    }
  }
  if (!pick.save(root.filePath(QStringLiteral("images/layer_000001_pick.png")))) {
    return fail("unable to save temporary pick map");
  }

  QImage regionPick(width, height, QImage::Format_RGB32);
  regionPick.fill(qRgb(0, 0, 1)); // model region selection id 1

  QImage semantic(width, height, QImage::Format_RGB32);
  semantic.fill(qRgb(224, 94, 74)); // model red
  for (int y = 2; y <= 3; ++y) {
    for (int x = 2; x <= 3; ++x) {
      semantic.setPixel(x, y, qRgb(80, 184, 198)); // support cyan
      regionPick.setPixel(x, y, qRgb(0, 0, 2));
    }
  }
  // A second disconnected support island belongs to the same raster component.
  semantic.setPixel(7, 7, qRgb(80, 184, 198));
  regionPick.setPixel(7, 7, qRgb(0, 0, 3));
  if (!semantic.save(
          root.filePath(QStringLiteral("images/layer_000001_semantic.png")))) {
    return fail("unable to save temporary semantic map");
  }
  if (!regionPick.save(
          root.filePath(QStringLiteral("images/layer_000001_regions.png")))) {
    return fail("unable to save temporary semantic-region pick map");
  }

  const QJsonObject sourceBounds{
      {QStringLiteral("min_x"), 0}, {QStringLiteral("min_y"), 0},
      {QStringLiteral("max_x"), width}, {QStringLiteral("max_y"), height}};
  const QJsonObject decisionBounds = sourceBounds;
  const QJsonObject decision{
      {QStringLiteral("node_id"), 4377},
      {QStringLiteral("component_id"), 4},
      {QStringLiteral("choice"), QStringLiteral("support")},
      {QStringLiteral("bounds_pixels"), decisionBounds},
      {QStringLiteral("state"),
       QJsonObject{{QStringLiteral("mixed_semantic_projection"), true}}}};
  const QJsonArray semanticRegions{
      QJsonObject{
          {QStringLiteral("region_id"), QStringLiteral("L000001-N4377-M001")},
          {QStringLiteral("lineage_id"), QStringLiteral("M000001")},
          {QStringLiteral("semantic"), QStringLiteral("model")},
          {QStringLiteral("component_id"), 4},
          {QStringLiteral("node_id"), 4377},
          {QStringLiteral("selection_id"), 1},
          {QStringLiteral("diagnostic_bounds_pixels"), sourceBounds}},
      QJsonObject{
          {QStringLiteral("region_id"), QStringLiteral("L000001-N4377-S001")},
          {QStringLiteral("lineage_id"), QStringLiteral("S000001")},
          {QStringLiteral("semantic"), QStringLiteral("support")},
          {QStringLiteral("component_id"), 4},
          {QStringLiteral("node_id"), 4377},
          {QStringLiteral("selection_id"), 2},
          {QStringLiteral("diagnostic_bounds_pixels"),
           QJsonObject{{QStringLiteral("min_x"), 2},
                       {QStringLiteral("min_y"), 2},
                       {QStringLiteral("max_x"), 4},
                       {QStringLiteral("max_y"), 4}}}},
      QJsonObject{
          {QStringLiteral("region_id"), QStringLiteral("L000001-N4377-S002")},
          {QStringLiteral("lineage_id"), QStringLiteral("S000002")},
          {QStringLiteral("semantic"), QStringLiteral("support")},
          {QStringLiteral("component_id"), 4},
          {QStringLiteral("node_id"), 4377},
          {QStringLiteral("selection_id"), 3},
          {QStringLiteral("diagnostic_bounds_pixels"),
           QJsonObject{{QStringLiteral("min_x"), 7},
                       {QStringLiteral("min_y"), 7},
                       {QStringLiteral("max_x"), 8},
                       {QStringLiteral("max_y"), 8}}}},
  };
  const QJsonObject layer{
      {QStringLiteral("decisions"), QJsonArray{decision}},
      {QStringLiteral("semantic_regions"), semanticRegions},
      {QStringLiteral("diagnostic"),
       QJsonObject{
           {QStringLiteral("source_bounds_pixels"), sourceBounds},
           {QStringLiteral("image_size_pixels"),
            QJsonObject{{QStringLiteral("width"), width},
                        {QStringLiteral("height"), height}}},
           {QStringLiteral("images"),
            QJsonObject{
                {QStringLiteral("semantic_result"),
                 QStringLiteral("images/layer_000001_semantic.png")},
                {QStringLiteral("pick_map"),
                 QStringLiteral("images/layer_000001_pick.png")},
                {QStringLiteral("region_pick_map"),
                 QStringLiteral("images/layer_000001_regions.png")}}}}}};
  if (!writeJson(root.filePath(QStringLiteral("layers/layer_000001.json")), layer)) {
    return fail("unable to save temporary layer JSON");
  }

  const QJsonObject manifest{
      {QStringLiteral("schema"), QStringLiteral("accloud.support-analysis-bundle.v1")},
      {QStringLiteral("source_path"), QStringLiteral("fixture.pwsz")},
      {QStringLiteral("layer_count"), 1},
      {QStringLiteral("images"),
       QJsonArray{QJsonObject{
           {QStringLiteral("layer"), 1},
           {QStringLiteral("layer_json"), QStringLiteral("layers/layer_000001.json")},
           {QStringLiteral("semantic_path"),
            QStringLiteral("images/layer_000001_semantic.png")},
           {QStringLiteral("pick_path"),
            QStringLiteral("images/layer_000001_pick.png")}}}}};
  if (!writeJson(root.filePath(QStringLiteral("manifest.json")), manifest)
      || !writeJson(root.filePath(QStringLiteral("summary.json")), QJsonObject{})) {
    return fail("unable to save temporary bundle metadata");
  }

  accloud::SupportAnalysisBridge bridge;
  if (!bridge.openBundle(bundle.path())) {
    return fail("SupportAnalysisBridge refused the temporary bundle");
  }

  // Click the first cyan island. The selected decision remains node 4377, but
  // the highlight must be limited to this connected support island.
  if (!bridge.selectCurrentComponent(0.25, 0.25)) {
    return fail("unable to select mixed component from support island");
  }
  if (bridge.selectedNodeId() != 4377) {
    return fail("unexpected selected node id");
  }
  if (bridge.selectedSemantic() != QStringLiteral("mixed")) {
    return fail("mixed component is not exposed as mixed");
  }
  if (bridge.selectedRegionId() != QStringLiteral("L000001-N4377-S001")
      || bridge.selectedLineageId() != QStringLiteral("S000001")
      || bridge.selectedRegionSemantic() != QStringLiteral("support")) {
    return fail("first semantic region identity is not exposed");
  }
  if (!bridge.currentDecisionJson().contains(QStringLiteral("selected_region"))
      || !bridge.currentDecisionJson().contains(
          QStringLiteral("L000001-N4377-S001"))) {
    return fail("selected semantic region is missing from decision JSON");
  }
  const auto island = bridge.selectedRegion();
  if (!island.value(QStringLiteral("valid")).toBool()
      || !near(island.value(QStringLiteral("x")).toDouble(), 0.2)
      || !near(island.value(QStringLiteral("y")).toDouble(), 0.2)
      || !near(island.value(QStringLiteral("width")).toDouble(), 0.2)
      || !near(island.value(QStringLiteral("height")).toDouble(), 0.2)) {
    return fail("support-island selection still expands to the whole mixed component");
  }

  // The disconnected cyan pixel must produce its own selection region.
  if (!bridge.selectCurrentComponent(0.75, 0.75)) {
    return fail("unable to select second support island");
  }
  const auto secondIsland = bridge.selectedRegion();
  if (bridge.selectedRegionId() != QStringLiteral("L000001-N4377-S002")
      || bridge.selectedLineageId() != QStringLiteral("S000002")) {
    return fail("second semantic region identity is not distinct");
  }
  if (!near(secondIsland.value(QStringLiteral("x")).toDouble(), 0.7)
      || !near(secondIsland.value(QStringLiteral("y")).toDouble(), 0.7)
      || !near(secondIsland.value(QStringLiteral("width")).toDouble(), 0.1)
      || !near(secondIsland.value(QStringLiteral("height")).toDouble(), 0.1)) {
    return fail("disconnected support islands are merged by selection");
  }

  // Clicking the model part resolves the model semantic region of the same
  // mixed raster component; in this fixture its bounds equal the component.
  if (!bridge.selectCurrentComponent(0.5, 0.5)) {
    return fail("unable to select model area of mixed component");
  }
  const auto component = bridge.selectedRegion();
  if (bridge.selectedRegionId() != QStringLiteral("L000001-N4377-M001")
      || bridge.selectedRegionSemantic() != QStringLiteral("model")) {
    return fail("model semantic region identity is not exposed");
  }
  if (!component.value(QStringLiteral("valid")).toBool()
      || !near(component.value(QStringLiteral("x")).toDouble(), 0.0)
      || !near(component.value(QStringLiteral("y")).toDouble(), 0.0)
      || !near(component.value(QStringLiteral("width")).toDouble(), 1.0)
      || !near(component.value(QStringLiteral("height")).toDouble(), 1.0)) {
    return fail("model-area selection no longer falls back to component bounds");
  }

  bridge.clearDecisionSelection();
  if (bridge.selectedNodeId() != -1
      || !bridge.selectedRegionId().isEmpty()
      || bridge.selectedRegion().value(QStringLiteral("valid")).toBool()) {
    return fail("clearDecisionSelection did not reset selection state");
  }

  return 0;
}
