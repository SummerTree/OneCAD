#pragma once

#include "StlExporter.h" // MeshExportResult

#include <QString>

namespace onecad::app {
class Document;
}

namespace onecad::io {

struct ObjExportOptions {
    bool visibleOnly = true;
    bool writeNormals = true;
    bool groupByBody = true;
};

class ObjExporter {
public:
    static MeshExportResult exportDocument(const QString& filepath,
                                            const app::Document* document,
                                            const ObjExportOptions& options = {});
private:
    ObjExporter() = delete;
};

} // namespace onecad::io
