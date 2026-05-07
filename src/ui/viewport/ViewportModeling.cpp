/**
 * @file ViewportModeling.cpp
 * @brief Activation methods for Fillet, PushPull, and Shell modeling tools.
 */
#include "Viewport.h"
#include "../tools/ModelingToolManager.h"
#include "../../app/selection/SelectionManager.h"
#include "../../app/selection/SelectionTypes.h"

namespace onecad::ui {

bool Viewport::activateFilletTool() {
    if (m_inSketchMode || !m_selectionManager || !m_modelingToolManager) {
        setFilletToolActive(false);
        setLinearPatternToolActive(false);
        return false;
    }

    const auto& selection = m_selectionManager->selection();
    if (selection.size() >= 1 &&
        selection.front().kind == app::selection::SelectionKind::Edge) {
        m_modelingToolManager->activateFillet(selection.front());
        setExtrudeToolActive(false);
        setRevolveToolActive(false);
        setLinearPatternToolActive(false);
        setShellToolActive(false);
        const bool activated = m_modelingToolManager->hasActiveTool();
        setFilletToolActive(activated);
        update();
        return activated;
    }

    setFilletToolActive(false);
    return false;
}

bool Viewport::activateShellTool() {
    if (m_inSketchMode || !m_selectionManager || !m_modelingToolManager) {
        setShellToolActive(false);
        setLinearPatternToolActive(false);
        return false;
    }

    const auto& selection = m_selectionManager->selection();
    if (selection.size() == 1 &&
        selection.front().kind == app::selection::SelectionKind::Body) {
        m_modelingToolManager->activateShell(selection.front());
        setExtrudeToolActive(false);
        setRevolveToolActive(false);
        setLinearPatternToolActive(false);
        setFilletToolActive(false);
        const bool activated = m_modelingToolManager->hasActiveTool();
        setShellToolActive(activated);
        update();
        return activated;
    }

    setShellToolActive(false);
    return false;
}

bool Viewport::activateLinearPatternTool() {
    if (m_inSketchMode || !m_selectionManager || !m_modelingToolManager) {
        setLinearPatternToolActive(false);
        return false;
    }

    if (m_linearPatternToolActive) {
        setExtrudeToolActive(false);
        setRevolveToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        return true;
    }

    const auto& selection = m_selectionManager->selection();
    if (selection.size() == 1 &&
        (selection.front().kind == app::selection::SelectionKind::Body ||
         selection.front().kind == app::selection::SelectionKind::Face ||
         selection.front().kind == app::selection::SelectionKind::Edge)) {
        m_modelingToolManager->activateLinearPattern(selection.front());
        setExtrudeToolActive(false);
        setRevolveToolActive(false);
        setFilletToolActive(false);
        setShellToolActive(false);
        const bool activated = m_modelingToolManager->hasActiveTool();
        setLinearPatternToolActive(activated);
        update();
        return activated;
    }

    setLinearPatternToolActive(false);
    return false;
}

} // namespace onecad::ui
