/**
 * @file LinearPatternTool.h
 * @brief Tool for creating linear body patterns.
 */
#ifndef ONECAD_UI_TOOLS_LINEARPATTERNTOOL_H
#define ONECAD_UI_TOOLS_LINEARPATTERNTOOL_H

#include "ModelingTool.h"

#include "../../kernel/elementmap/ElementMap.h"
#include "../../render/tessellation/TessellationCache.h"

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>

#include <memory>
#include <string>

namespace onecad::app {
class Document;
namespace commands {
class CommandProcessor;
}
}

namespace onecad::ui {
class Viewport;
}

namespace onecad::ui::tools {

class PatternOptionsOverlay;

class LinearPatternTool : public ModelingTool {
public:
    explicit LinearPatternTool(Viewport* viewport, app::Document* document);
    ~LinearPatternTool() override;

    void setDocument(app::Document* document);
    void setCommandProcessor(app::commands::CommandProcessor* processor);

    void begin(const app::selection::SelectionItem& selection) override;
    void cancel() override;
    bool isActive() const override { return active_; }
    bool isDragging() const override { return dragging_; }

    bool handleMousePress(const QPoint& screenPos, Qt::MouseButton button) override;
    bool handleMouseMove(const QPoint& screenPos) override;
    bool handleMouseRelease(const QPoint& screenPos, Qt::MouseButton button) override;
    bool confirm() override;
    std::optional<Indicator> indicator() const override;

    void onSelectionChanged(const std::vector<app::selection::SelectionItem>& selection);

private:
    enum class State {
        WaitingForSource,
        WaitingForDirection,
        Ready,
        Dragging
    };

    void resetState();
    bool prepareSource(const app::selection::SelectionItem& selection);
    bool trySetDirectionFromSelection(const app::selection::SelectionItem& selection);
    bool extractEdgeDirection(const app::selection::SelectionItem& selection,
                              gp_Pnt* outOrigin,
                              gp_Dir* outDirection,
                              QString* outSummary) const;
    bool extractFaceDirection(const app::selection::SelectionItem& selection,
                              gp_Pnt* outOrigin,
                              gp_Dir* outDirection,
                              QString* outSummary) const;
    void ensureOverlay();
    void updateOverlay();
    void updatePreview();
    void clearPreview();
    double computeDraggedSpacing(const QPoint& screenPos) const;
    std::vector<render::SceneMeshStore::Mesh> buildPreviewMeshes() const;

    Viewport* viewport_ = nullptr;
    app::Document* document_ = nullptr;
    app::commands::CommandProcessor* commandProcessor_ = nullptr;

    std::unique_ptr<PatternOptionsOverlay> overlay_;
    render::TessellationCache previewTessellator_;
    mutable kernel::elementmap::ElementMap previewElementMap_;

    app::selection::SelectionItem sourceSelection_{};
    app::selection::SelectionItem directionSelection_{};
    std::string sourceBodyId_;
    TopoDS_Shape sourceShape_;
    gp_Pnt directionOrigin_;
    gp_Dir direction_;
    bool directionValid_ = false;
    QString directionSummary_;

    State state_ = State::WaitingForSource;
    bool active_ = false;
    bool dragging_ = false;
    QPoint dragStart_;
    double dragStartSpacing_ = 10.0;
    double currentSpacing_ = 10.0;
    int instanceCount_ = 2;
    bool fuseResult_ = true;
};

} // namespace onecad::ui::tools

#endif // ONECAD_UI_TOOLS_LINEARPATTERNTOOL_H
