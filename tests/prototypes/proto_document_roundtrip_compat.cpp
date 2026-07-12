#include "app/document/Document.h"
#include "app/document/DatumPlane.h"
#include "app/history/RegenerationEngine.h"
#include "core/loop/LoopDetector.h"
#include "core/loop/RegionUtils.h"
#include "core/sketch/Sketch.h"
#include "io/OneCADFileIO.h"

#include <QDir>
#include <QFile>
#include <QUuid>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::string buildClosedRegionSketch(onecad::app::Document& doc) {
    auto sketch = std::make_unique<onecad::core::sketch::Sketch>();
    const auto p1 = sketch->addPoint(0.0, 0.0);
    const auto p2 = sketch->addPoint(20.0, 0.0);
    const auto p3 = sketch->addPoint(20.0, 15.0);
    const auto p4 = sketch->addPoint(0.0, 15.0);

    sketch->addLine(p1, p2);
    sketch->addLine(p2, p3);
    sketch->addLine(p3, p4);
    sketch->addLine(p4, p1);

    return doc.addSketch(std::move(sketch));
}

std::string detectFirstRegion(onecad::core::sketch::Sketch& sketch) {
    onecad::core::loop::LoopDetectorConfig config = onecad::core::loop::makeRegionDetectionConfig();
    onecad::core::loop::LoopDetector detector(config);
    const auto result = detector.detect(sketch);
    if (!result.success || result.faces.empty()) {
        return {};
    }
    return onecad::core::loop::regionKey(result.faces[0].outerLoop);
}

} // namespace

int main() {
    onecad::app::Document source;

    const std::string sketchId = buildClosedRegionSketch(source);
    auto* sketch = source.getSketch(sketchId);
    if (!sketch) {
        std::cerr << "Failed to create sketch\n";
        return 1;
    }

    const std::string regionId = detectFirstRegion(*sketch);
    if (regionId.empty()) {
        std::cerr << "Failed to detect a closed region\n";
        return 1;
    }

    onecad::app::OperationRecord op;
    op.opId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    op.type = onecad::app::OperationType::Extrude;
    op.input = onecad::app::SketchRegionRef{sketchId, regionId};
    op.params = onecad::app::ExtrudeParams{12.0, 0.0, onecad::app::ExtrudeMode::Blind, onecad::app::BooleanMode::NewBody};
    const std::string bodyId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    op.resultBodyIds.push_back(bodyId);
    source.addOperation(op);

    onecad::app::OperationMetadata metadata;
    metadata.recordSchemaVersion = 1;
    metadata.stepIndex = 0;
    metadata.uiAlias = QStringLiteral("PushPull");
    metadata.replayOnly = false;
    metadata.determinism.parallel = false;
    metadata.determinism.solverPolicyHash = QStringLiteral("solver-v1");
    metadata.anchor.hasWorldPoint = true;
    metadata.anchor.x = 0.5;
    metadata.anchor.y = 0.5;
    metadata.anchor.z = 0.0;
    source.setOperationMetadata(op.opId, metadata);
    source.setAppliedOpCount(source.operations().size());

    // A datum plane must survive the round-trip with its resolved frame intact.
    onecad::app::DatumPlane datum;
    datum.id = "datum-test-1";
    datum.name = "offset25";
    datum.kind = onecad::app::DatumPlane::Kind::OffsetFromPlane;
    datum.basePlaneId = "XY";
    datum.offset = 25.0;
    source.addDatumPlane(datum);

    onecad::app::history::RegenerationEngine regen(&source);
    const auto regenResult = regen.regenerateAll();
    if (regenResult.status == onecad::app::history::RegenStatus::CriticalFailure) {
        std::cerr << "Source regeneration failed\n";
        return 1;
    }

    const QString tempPath =
        QDir::temp().absoluteFilePath(QString("onecad_roundtrip_%1.onecad")
                                      .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    const auto saveResult = onecad::io::OneCADFileIO::save(tempPath, &source);
    if (!saveResult.success) {
        std::cerr << "Failed to save roundtrip file: " << saveResult.errorMessage.toStdString() << "\n";
        return 1;
    }

    QString loadError;
    auto loaded = onecad::io::OneCADFileIO::load(tempPath, loadError);
    QFile::remove(tempPath);

    if (!loaded) {
        std::cerr << "Failed to load roundtrip file: " << loadError.toStdString() << "\n";
        return 1;
    }

    if (loaded->sketchCount() != source.sketchCount()) {
        std::cerr << "Sketch count mismatch after roundtrip\n";
        return 1;
    }
    if (loaded->operations().size() != source.operations().size()) {
        std::cerr << "Operation count mismatch after roundtrip\n";
        return 1;
    }
    if (loaded->appliedOpCount() != source.appliedOpCount()) {
        std::cerr << "Applied operation cursor mismatch after roundtrip\n";
        return 1;
    }

    const auto loadedMeta = loaded->operationMetadata(op.opId);
    if (!loadedMeta.has_value() || loadedMeta->uiAlias != QStringLiteral("PushPull")) {
        std::cerr << "Operation metadata mismatch after roundtrip\n";
        return 1;
    }

    if (loaded->datumPlaneCount() != source.datumPlaneCount()) {
        std::cerr << "Datum plane count mismatch after roundtrip\n";
        return 1;
    }
    const auto* loadedDatum = loaded->getDatumPlane("datum-test-1");
    if (!loadedDatum || !loadedDatum->resolvedValid ||
        std::abs(loadedDatum->resolvedPlane.origin.z - 25.0) > 1e-6 ||
        loadedDatum->kind != onecad::app::DatumPlane::Kind::OffsetFromPlane) {
        std::cerr << "Datum plane mismatch after roundtrip\n";
        return 1;
    }

    // Document::clear() must reset datum planes too (regression: datums leaked
    // across File>New and were saved into unrelated documents).
    loaded->clear();
    if (loaded->datumPlaneCount() != 0) {
        std::cerr << "Document::clear() left datum planes behind\n";
        return 1;
    }

    // ── Atomic save: re-saving a directory package must not resurrect deleted
    // content (the old in-place save left stale files that load-by-listing read).
    {
        const QString pkgPath = QDir::temp().absoluteFilePath(
            QString("onecad_roundtrip_%1.onecadpkg")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        const std::size_t baseCount = source.sketchCount();

        const std::string extraSketchId = buildClosedRegionSketch(source);
        if (!onecad::io::OneCADFileIO::save(pkgPath, &source).success) {
            std::cerr << "Directory package save failed\n";
            return 1;
        }
        QString pkgError;
        auto pkgLoaded = onecad::io::OneCADFileIO::load(pkgPath, pkgError);
        if (!pkgLoaded || pkgLoaded->sketchCount() != baseCount + 1) {
            std::cerr << "Directory package roundtrip mismatch before deletion\n";
            return 1;
        }

        source.removeSketch(extraSketchId);
        if (!onecad::io::OneCADFileIO::save(pkgPath, &source).success) {
            std::cerr << "Directory package re-save failed\n";
            return 1;
        }
        pkgLoaded = onecad::io::OneCADFileIO::load(pkgPath, pkgError);
        if (!pkgLoaded || pkgLoaded->sketchCount() != baseCount) {
            std::cerr << "Deleted sketch resurrected on directory package re-save\n";
            return 1;
        }
        std::filesystem::remove_all(std::filesystem::path(pkgPath.toStdString()));
    }

    // ── Atomic save: a failing save must leave the previously saved file intact
    // (old behavior truncated the destination before writing anything).
    {
        namespace fs = std::filesystem;
        const QString atomicDir = QDir::temp().absoluteFilePath(
            QString("onecad_atomic_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        QDir().mkpath(atomicDir);
        const QString docPath = atomicDir + "/doc.onecad";

        if (!onecad::io::OneCADFileIO::save(docPath, &source).success) {
            std::cerr << "Initial atomic-save baseline failed\n";
            return 1;
        }
        const std::size_t savedSketchCount = source.sketchCount();

        // Make the parent directory unwritable so the next save cannot create
        // its temp artifact; the original file must survive untouched.
        const fs::path dirPath(atomicDir.toStdString());
        fs::permissions(dirPath, fs::perms::owner_read | fs::perms::owner_exec,
                        fs::perm_options::replace);
        const auto failedSave = onecad::io::OneCADFileIO::save(docPath, &source);
        fs::permissions(dirPath, fs::perms::owner_all, fs::perm_options::replace);

        if (failedSave.success) {
            std::cerr << "Save into a read-only directory unexpectedly succeeded\n";
            return 1;
        }
        QString survivedError;
        auto survived = onecad::io::OneCADFileIO::load(docPath, survivedError);
        if (!survived || survived->sketchCount() != savedSketchCount) {
            std::cerr << "Failed save destroyed the previously saved file\n";
            return 1;
        }

        // No temp artifacts may remain next to a successful save.
        const QStringList leftovers =
            QDir(atomicDir).entryList(QStringList() << ".*.saving.*" << "*.old.*",
                                      QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
        if (!leftovers.isEmpty()) {
            std::cerr << "Atomic save left temp artifacts behind\n";
            return 1;
        }
        fs::remove_all(dirPath);
    }

    std::cout << "Document roundtrip compatibility test passed\n";
    return 0;
}
