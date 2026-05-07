#include "Viewport.h"
#include "../tools/ModelingToolManager.h"
#include "../../app/selection/SelectionManager.h"
#include "../../app/selection/SelectionTypes.h"

namespace onecad::ui {

bool Viewport::activateRevolveTool() {
    if (m_inSketchMode || !m_selectionManager || !m_modelingToolManager) {
        setRevolveToolActive(false);
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        return false;
    }

    if (m_revolveToolActive) {
        setExtrudeToolActive(false);
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        return true;
    }

    const auto& selection = m_selectionManager->selection();
    if (selection.size() == 1 &&
        (selection.front().kind == app::selection::SelectionKind::SketchRegion ||
         selection.front().kind == app::selection::SelectionKind::Face)) {
        m_modelingToolManager->activateRevolve(selection.front());
        setExtrudeToolActive(false);
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        const bool activated = m_modelingToolManager->hasActiveTool();
        setRevolveToolActive(activated);
        return activated;
    }

    setRevolveToolActive(false);
    setLinearPatternToolActive(false);
    setFilletToolActive(false);
    setShellToolActive(false);
    return false;
}

void Viewport::setRevolveToolActive(bool active) {
    if (m_revolveToolActive == active) {
        return;
    }
    m_revolveToolActive = active;
    updateModelSelectionFilter();
    emit revolveToolActiveChanged(active);
}

void Viewport::setLinearPatternToolActive(bool active) {
    if (m_linearPatternToolActive == active) {
        return;
    }
    m_linearPatternToolActive = active;
    updateModelSelectionFilter();
    emit linearPatternToolActiveChanged(active);
}

} // namespace onecad::ui
