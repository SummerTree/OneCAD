/**
 * @file StepExporter.h
 * @brief STEP file export using OpenCASCADE XDE path
 */

#pragma once

#include <QString>
#include <QStringList>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class TopoDS_Shape;

namespace onecad::app {
class Document;
}

namespace onecad::io {

struct StepExportResult {
    bool success = false;
    QString errorMessage;
    int bodyCount = 0;
};

struct ExportOptions {
    QString schema = "AP214IS";
    bool visibleOnly = true;
};

class StepExporter {
public:
    static StepExportResult exportDocument(const QString& filepath,
                                            const app::Document* document);

    static StepExportResult exportDocument(const QString& filepath,
                                            const app::Document* document,
                                            const ExportOptions& options);

    static StepExportResult exportShapes(const QString& filepath,
                                          const std::vector<TopoDS_Shape>& shapes);

    static StepExportResult exportShape(const QString& filepath,
                                         const TopoDS_Shape& shape);

private:
    StepExporter() = delete;
};

} // namespace onecad::io
