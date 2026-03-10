#include "app/commands/AddOperationCommand.h"
#include "app/document/Document.h"
#include "app/history/RegenerationEngine.h"
#include "core/loop/LoopDetector.h"
#include "core/loop/RegionUtils.h"
#include "core/sketch/Sketch.h"
#include "io/OneCADFileIO.h"

#include <QDir>
#include <QFile>
#include <QUuid>

#include <iostream>
#include <string>

namespace {

std::string buildRect(onecad::app::Document& doc, double w, double h) {
    auto sketch = std::make_unique<onecad::core::sketch::Sketch>();
    const auto p1 = sketch->addPoint(0.0, 0.0);
    const auto p2 = sketch->addPoint(w, 0.0);
    const auto p3 = sketch->addPoint(w, h);
    const auto p4 = sketch->addPoint(0.0, h);
    sketch->addLine(p1, p2);
    sketch->addLine(p2, p3);
    sketch->addLine(p3, p4);
    sketch->addLine(p4, p1);
    return doc.addSketch(std::move(sketch));
}

std::string detectRegion(onecad::core::sketch::Sketch& sketch) {
    onecad::core::loop::LoopDetectorConfig config = onecad::core::loop::makeRegionDetectionConfig();
    onecad::core::loop::LoopDetector detector(config);
    const auto result = detector.detect(sketch);
    if (!result.success || result.faces.empty()) return {};
    return onecad::core::loop::regionKey(result.faces[0].outerLoop);
}

#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << "\n"; return 1; } } while(0)

} // namespace

int main() {
    // 1. Create document + sketch + extrude
    onecad::app::Document doc;

    const std::string sketchId = buildRect(doc, 30.0, 20.0);
    auto* sketch = doc.getSketch(sketchId);
    CHECK(sketch, "sketch creation");

    const std::string regionId = detectRegion(*sketch);
    CHECK(!regionId.empty(), "region detection");

    onecad::app::OperationRecord op;
    op.opId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    op.type = onecad::app::OperationType::Extrude;
    op.input = onecad::app::SketchRegionRef{sketchId, regionId};
    op.params = onecad::app::ExtrudeParams{15.0, 0.0, onecad::app::ExtrudeMode::Blind, onecad::app::BooleanMode::NewBody};
    const std::string bodyId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    op.resultBodyIds.push_back(bodyId);

    CHECK(onecad::app::commands::AddOperationCommand(&doc, op).execute(), "add extrude");
    CHECK(doc.operations().size() == 1, "op count after extrude");
    CHECK(doc.bodyCount() > 0, "body exists after extrude");

    // 2. Regenerate
    onecad::app::history::RegenerationEngine regen(&doc);
    auto result = regen.regenerateAll();
    CHECK(result.status != onecad::app::history::RegenStatus::CriticalFailure, "regen ok");

    // 3. Save
    const QString tempPath = QDir::temp().absoluteFilePath(
        QStringLiteral("onecad_workflow_%1.onecad")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    auto saveResult = onecad::io::OneCADFileIO::save(tempPath, &doc);
    CHECK(saveResult.success, "save");

    // 4. Reload
    QString loadErr;
    auto loaded = onecad::io::OneCADFileIO::load(tempPath, loadErr);
    QFile::remove(tempPath);
    CHECK(loaded, "load");

    // 5. Verify round-trip integrity
    CHECK(loaded->sketchCount() == doc.sketchCount(), "sketch count match");
    CHECK(loaded->operations().size() == doc.operations().size(), "op count match");
    CHECK(loaded->appliedOpCount() == doc.appliedOpCount(), "applied cursor match");

    // 6. Re-regen loaded doc
    onecad::app::history::RegenerationEngine regen2(loaded.get());
    auto result2 = regen2.regenerateAll();
    CHECK(result2.status != onecad::app::history::RegenStatus::CriticalFailure, "loaded regen ok");
    CHECK(loaded->bodyCount() > 0, "loaded body exists");

    std::cout << "Document workflow integration test passed\n";
    return 0;
}
