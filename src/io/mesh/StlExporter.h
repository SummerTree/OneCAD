#pragma once

#include <QString>
#include <vector>

namespace onecad::app {
class Document;
}

namespace onecad::io {

struct StlExportOptions {
    bool binary = true;
    bool visibleOnly = true;
};

struct MeshExportResult {
    bool success = false;
    QString errorMessage;
    int bodyCount = 0;
    int triangleCount = 0;
};

class StlExporter {
public:
    static MeshExportResult exportDocument(const QString& filepath,
                                            const app::Document* document,
                                            const StlExportOptions& options = {});
private:
    StlExporter() = delete;
};

} // namespace onecad::io
