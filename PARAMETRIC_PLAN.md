# OneCAD Next Phase Implementation Plan

**Focus**: Parametric Engine (Phase 3.2)
**Regen Strategy**: Full replay always (ignore cached BREP)
**UI Placement**: Right sidebar floating panel

---

## Current State Analysis

### Phase 3 Status (Solid Modeling - 85%)
- **3.1 I/O**: 100% - Full save/load/STEP working
- **3.2 Parametric**: 25% - Data structures only, no replay
- **3.3 Modeling Ops**: 100% - Extrude/Revolve/Fillet/Shell/Boolean done
- **3.4 Patterns**: 0% - Not started
- **3.5 UI Polish**: 40% - Theme/StartOverlay done, CommandSearch missing

### Key Finding
ShellTool/FilletChamferTool keyboard wiring is **already complete** (Viewport.cpp:1677-1684). TODOs in headers are stale.

---

## Scope: Parametric Engine (Phase 3.2)

**Goal**: Enable full parametric workflow — create, save, reload, edit parameters, regenerate.

**Components**:
1. DependencyGraph — track feature relationships
2. RegenerationEngine — rebuild bodies from operations
3. HistoryPanel — visualize feature tree

**Future phases** (deferred):
- Pattern Operations (3.4) — requires parametric tracking
- UI Polish (3.5) — CommandSearch, Box selection, Camera inertia

---

## Resolved Decisions

- **Regen strategy**: Full replay always — ignore cached BREP, rebuild from operations
- **History placement**: Right sidebar floating panel (Fusion 360 style)
- **Failure handling**: Partial load + prompt user to fix/delete failed features
- **Edit scope**: Extrude + Revolve only (v1), others display-only
- **Record ops**: All modeling ops (Extrude, Revolve, Fillet, Chamfer, Shell, Boolean)
- **Op params**: Full state snapshot (all tool settings)
- **Regen trigger**: On dialog OK (not live)
- **Reorder**: No reorder (fixed creation order)
- **UI style**: Tree view with dependencies
- **Test strategy**: Create proto_regen prototype test before UI work
- **Edge IDs**: Use ElementMap topological naming for edge/face references
- **Rollback**: Suppress downstream only (non-destructive, undoable via Cmd+Z)
- **Live preview**: On value change (spinbox change event, not keypress)
- **Panel toggle**: View menu (View → History Panel) + keyboard (Cmd+H)
- **Preview chain**: Full downstream regen during preview
- **Panel visibility**: Hidden by default, toggle to show

---

## Extended OperationRecord Schema

### New OperationType enum
```cpp
enum class OperationType {
    Extrude,     // Existing
    Revolve,     // Existing
    Fillet,      // NEW
    Chamfer,     // NEW
    Shell,       // NEW
    Boolean      // NEW
};
```

### New Parameter Structs
```cpp
// Fillet/Chamfer share structure
struct FilletChamferParams {
    enum class Mode { Fillet, Chamfer };
    Mode mode;
    double radius;                        // Fillet radius or chamfer distance
    std::vector<EdgeRef> edges;           // Selected edges (via ElementMap IDs)
    bool chainTangentEdges = true;        // Auto-expand to tangent chain
};

struct ShellParams {
    double thickness;
    std::vector<FaceRef> openFaces;       // Faces to remove
};

struct BooleanParams {
    enum class Op { Union, Cut, Intersect };
    Op operation;
    std::string toolBodyId;               // Body to boolean with
};

// Update OperationParams variant
using OperationParams = std::variant<
    ExtrudeParams,
    RevolveParams,
    FilletChamferParams,
    ShellParams,
    BooleanParams
>;

// Update ExtrudeInput to cover all input types
using OperationInput = std::variant<
    SketchRegionRef,     // Extrude from sketch
    FaceRef,             // Push/pull face or shell
    BodyRef              // Boolean target, fillet/chamfer target
>;
```

### HistoryIO Extensions
- Add serializers for new param types
- Update `serializeOperation()` switch statement
- Add migration for old files (default missing fields)

---

## Implementation Order

### Phase 0: Prototype Test (First!)
**File**: `tests/proto_regeneration.cpp`

Validate core regen logic before building UI:
```cpp
// Test cases:
// 1. Single extrude: Create sketch → extrude → serialize → clear → deserialize → regen → verify shape
// 2. Chain: Extrude → fillet → serialize → regen → verify fillet on correct edge
// 3. Failure: Delete sketch → regen → verify failure reported
// 4. Topological naming: Extrude → fillet edge by ElementMap ID → modify extrude → regen → verify fillet resolves

int main() {
    test_single_extrude_regen();
    test_extrude_fillet_chain();
    test_missing_input_failure();
    test_elementmap_edge_resolution();
    return 0;
}
```

### Phase 1: Core Engine (DependencyGraph + RegenerationEngine)
### Phase 2: Tool Integration (Add OperationRecords to Fillet/Shell/Boolean)
### Phase 3: File Load Wiring (DocumentIO regen path)
### Phase 4: UI (HistoryPanel, EditParameterDialog, RegenFailureDialog)

---

## Detailed Implementation Steps

### Step 1: DependencyGraph Class
**File**: `src/app/history/DependencyGraph.h/cpp`

```cpp
struct FeatureNode {
    std::string opId;
    OperationType type;
    std::unordered_set<std::string> inputBodyIds;
    std::unordered_set<std::string> inputSketchIds;
    std::unordered_set<std::string> outputBodyIds;
    bool suppressed = false;
};

class DependencyGraph {
public:
    void clear();
    void addOperation(const OperationRecord& op);
    void removeOperation(const std::string& opId);

    std::vector<std::string> topologicalSort() const;
    std::vector<std::string> downstreamOps(const std::string& opId) const;
    std::vector<std::string> upstreamOps(const std::string& opId) const;
    bool hasCycle() const;

    void setSuppressed(const std::string& opId, bool suppressed);
    bool isSuppressed(const std::string& opId) const;

private:
    std::unordered_map<std::string, FeatureNode> nodes_;
    std::unordered_map<std::string, std::unordered_set<std::string>> edges_; // opId → downstream opIds
};
```

### Step 2: RegenerationEngine Class
**File**: `src/app/history/RegenerationEngine.h/cpp`

```cpp
enum class RegenStatus { Success, PartialFailure, CriticalFailure };

struct RegenResult {
    RegenStatus status;
    std::vector<std::string> succeededOps;
    std::vector<std::string> failedOps;
    std::string errorMessage;
};

class RegenerationEngine {
public:
    explicit RegenerationEngine(Document* doc);

    RegenResult regenerateAll();
    RegenResult regenerateFrom(const std::string& opId);

    void setProgressCallback(std::function<void(int current, int total)> cb);

private:
    Document* doc_;
    DependencyGraph graph_;

    bool executeOperation(const OperationRecord& op);
    TopoDS_Shape buildExtrude(const ExtrudeParams& params, const ExtrudeInput& input);
    TopoDS_Shape buildRevolve(const RevolveParams& params, const ExtrudeInput& input);
};
```

### Step 3: Wire Regeneration to File Load
**File**: `src/io/DocumentIO.cpp`

Modify `loadDocument()`:
1. Load sketches first (they don't depend on operations)
2. Load operation records from history/ops.jsonl
3. Build DependencyGraph from operations
4. Call `RegenerationEngine::regenerateAll()`
5. Skip BREP loading — bodies created by regen

### Step 4: HistoryPanel UI (Tree View)
**File**: `src/ui/history/HistoryPanel.h/cpp`

```cpp
class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget* parent = nullptr);

    void setDocument(Document* doc);
    void setDependencyGraph(DependencyGraph* graph);
    void refresh();

signals:
    void operationSelected(const QString& opId);
    void editRequested(const QString& opId);
    void suppressionToggled(const QString& opId, bool suppressed);
    void deleteRequested(const QString& opId);

private:
    QTreeWidget* m_tree;           // Tree view (not list)
    Document* m_doc = nullptr;
    DependencyGraph* m_graph = nullptr;

    void setupUi();
    void buildTree();
    QTreeWidgetItem* createFeatureItem(const OperationRecord& op);
    void updateItemStatus(QTreeWidgetItem* item, bool failed);
};
```

### Tree Structure
```
├── Sketch 1
│   └── Extrude 1 (10mm) [editable]
│       └── Fillet 1 (R2mm) [display-only]
├── Sketch 2
│   └── Revolve 1 (360°) [editable]
└── Boolean Cut (Body1 - Body2) [display-only]
```

### Item Visual States
| State | Icon | Text Style | Background |
|-------|------|------------|------------|
| Normal | Op-specific | Regular | None |
| Selected | Op-specific | Bold | Highlight |
| Failed | ⚠️ | Strikethrough | Light red |
| Suppressed | 👁️‍🗨️ | Gray italic | Light gray |
| Editable | ✏️ badge | Regular | None |

### Context Menu (Right-click)
- **Edit Parameters...** (Extrude/Revolve only)
- **Rollback to Here** — Suppress all ops after this one (non-destructive)
- **Suppress/Unsuppress**
- **Delete Feature**
- **Select Geometry** (highlights related body/face)

### Panel Visibility
- **Hidden by default** on app launch
- **Toggle**: View menu → History Panel (Cmd+H)
- **Persist**: Remember visibility state in preferences

### Step 5: MainWindow Integration
**File**: `src/ui/mainwindow/MainWindow.cpp`

1. Create `m_historyPanel` as floating widget (parent: viewport)
2. Position in `positionHistoryPanel()` — right side, below ViewCube
3. **Start hidden** (hidden by default)
4. Connect to document signals
5. Add to resize eventFilter
6. Wire context menu actions (Edit, Rollback, Suppress, Delete)

**View Menu Addition:**
```cpp
void MainWindow::createMenus() {
    // In View menu
    m_viewHistoryAction = new QAction("History Panel", this);
    m_viewHistoryAction->setShortcut(QKeySequence("Ctrl+H"));  // Cmd+H on Mac
    m_viewHistoryAction->setCheckable(true);
    m_viewHistoryAction->setChecked(false);  // Hidden by default
    connect(m_viewHistoryAction, &QAction::toggled, this, &MainWindow::toggleHistoryPanel);
    m_viewMenu->addAction(m_viewHistoryAction);
}

void MainWindow::toggleHistoryPanel(bool visible) {
    m_historyPanel->setVisible(visible);
    if (visible) {
        positionHistoryPanel();
        m_historyPanel->raise();
    }
    // Persist to preferences
    QSettings settings;
    settings.setValue("ui/historyPanelVisible", visible);
}
```

**Rollback Implementation (Undoable):**
```cpp
// New command: RollbackCommand
class RollbackCommand : public Command {
public:
    RollbackCommand(Document* doc, DependencyGraph* graph,
                    RegenerationEngine* regen, const std::string& toOpId)
        : doc_(doc), graph_(graph), regen_(regen), toOpId_(toOpId) {}

    void execute() override {
        // Store which ops were suppressed before (for undo)
        auto downstream = graph_->downstreamOps(toOpId_);
        for (const auto& id : downstream) {
            previousStates_[id] = graph_->isSuppressed(id);
            graph_->setSuppressed(id, true);
        }
        regen_->regenerateAll();
    }

    void undo() override {
        // Restore previous suppression states
        for (const auto& [id, wasSuppressed] : previousStates_) {
            graph_->setSuppressed(id, wasSuppressed);
        }
        regen_->regenerateAll();
    }

    std::string name() const override { return "Rollback to " + toOpId_; }

private:
    Document* doc_;
    DependencyGraph* graph_;
    RegenerationEngine* regen_;
    std::string toOpId_;
    std::unordered_map<std::string, bool> previousStates_;  // opId → wasSuppressed
};

void MainWindow::onRollbackToOp(const QString& opId) {
    auto cmd = std::make_unique<RollbackCommand>(
        m_document.get(), m_graph.get(), m_regenEngine.get(), opId.toStdString());
    m_commandProcessor->execute(std::move(cmd));
    m_historyPanel->refresh();
    m_viewport->update();
}
```

### Step 6: Partial Load + Failure Prompt
**File**: `src/io/DocumentIO.cpp`

**Workflow on load failure:**
```
1. Load sketches (no dependencies)
2. Build DependencyGraph
3. Regenerate operations in order
4. If op fails:
   a. Mark op as failed in graph
   b. Skip downstream ops (mark as skipped)
   c. Continue with independent ops
5. After all ops processed:
   a. If any failures → show RegenFailureDialog
   b. Dialog lists failed ops with error messages
   c. User options: "Delete Failed", "Suppress", "Cancel Load"
6. Apply user choice, finalize document
```

**RegenFailureDialog:**
```cpp
class RegenFailureDialog : public QDialog {
public:
    enum class Action { DeleteFailed, SuppressFailed, CancelLoad };

    RegenFailureDialog(const std::vector<FailedOp>& failures, QWidget* parent);
    Action selectedAction() const;

private:
    QListWidget* m_failureList;
    QButtonGroup* m_actionGroup;
};
```

### Step 7: Edit Parameter Dialog (Live Preview)
**File**: `src/ui/history/EditParameterDialog.h/cpp`

**Extrude dialog fields:**
- Distance (QDoubleSpinBox, mm)
- Draft angle (QDoubleSpinBox, degrees)
- Boolean mode (QComboBox: NewBody/Add/Cut/Intersect)

**Revolve dialog fields:**
- Angle (QDoubleSpinBox, degrees)
- Boolean mode (QComboBox)

**Live Preview Workflow:**
1. Double-click feature in HistoryPanel
2. Open EditParameterDialog with current values
3. **On spinbox valueChanged**: Emit `previewRequested` signal
4. MainWindow receives signal → call `RegenerationEngine::previewFrom(opId, newParams)`
5. Preview regenerates edited op AND all downstream ops
6. Viewport shows preview geometry (0.5 opacity for preview bodies)
7. **[OK]**: Commit params to OperationRecord, finalize preview as real geometry
8. **[Cancel]**: Discard preview, restore original geometry

```cpp
class EditParameterDialog : public QDialog {
    Q_OBJECT
public:
    EditParameterDialog(const OperationRecord& op,
                        RegenerationEngine* regen,
                        QWidget* parent);

    OperationParams getModifiedParams() const;

signals:
    void previewRequested(const QString& opId, const OperationParams& params);
    void previewCancelled();

private slots:
    void onValueChanged();  // Connected to all spinbox valueChanged signals

private:
    void setupExtrudeUi(const ExtrudeParams& params);
    void setupRevolveUi(const RevolveParams& params);

    OperationRecord originalOp_;  // For cancel/restore
    bool previewActive_ = false;
};
```

**RegenerationEngine additions:**
```cpp
class RegenerationEngine {
public:
    // Preview mode: temp regen from opId with new params
    RegenResult previewFrom(const std::string& opId, const OperationParams& newParams);

    // Commit preview as permanent state
    void commitPreview();

    // Discard preview, restore original state
    void discardPreview();

private:
    std::optional<RegenResult> activePreview_;
    std::vector<TopoDS_Shape> originalShapes_;  // Backup for restore
};
```

---

## File Checklist

**New files (Prototype Test - FIRST):**
- [ ] `tests/proto_regeneration.cpp`

**New files (Core):**
- [ ] `src/app/history/DependencyGraph.h`
- [ ] `src/app/history/DependencyGraph.cpp`
- [ ] `src/app/history/RegenerationEngine.h`
- [ ] `src/app/history/RegenerationEngine.cpp`

**New files (UI):**
- [ ] `src/ui/history/HistoryPanel.h`
- [ ] `src/ui/history/HistoryPanel.cpp`
- [ ] `src/ui/history/EditParameterDialog.h`
- [ ] `src/ui/history/EditParameterDialog.cpp`
- [ ] `src/ui/history/RegenFailureDialog.h`
- [ ] `src/ui/history/RegenFailureDialog.cpp`

**New files (Commands):**
- [ ] `src/app/commands/RollbackCommand.h`
- [ ] `src/app/commands/RollbackCommand.cpp`

**Modified files (Data):**
- [ ] `src/app/document/OperationRecord.h` — Add Fillet/Chamfer/Shell/Boolean types
- [ ] `src/app/document/Document.h/cpp` — Add DependencyGraph member
- [ ] `src/io/HistoryIO.cpp` — Serialize new operation types

**Modified files (Tools):**
- [ ] `src/ui/tools/FilletChamferTool.cpp` — Create OperationRecord on commit
- [ ] `src/ui/tools/ShellTool.cpp` — Create OperationRecord on commit
- [ ] `src/ui/tools/BooleanTool.cpp` — Create OperationRecord on commit (if exists)

**Modified files (Integration):**
- [ ] `src/io/DocumentIO.cpp` — Replace BREP load with regen
- [ ] `src/ui/mainwindow/MainWindow.h/cpp` — Add HistoryPanel
- [ ] `CMakeLists.txt` — Add new sources

---

## Verification Plan

### Test 1: Basic Regeneration
1. Create sketch with rectangle
2. Extrude to 10mm
3. Save file (verify ops.jsonl contains Extrude record)
4. Close and reopen
5. **Verify**: Body exists with correct dimensions (no BREP loaded)
6. **Verify**: HistoryPanel shows "Extrude 1 (10mm)"

### Test 2: Dependency Chain
1. Create sketch → extrude → fillet on edge
2. Save/reopen
3. **Verify**: Fillet applied to correct edge after regen
4. **Verify**: Tree shows Extrude → Fillet hierarchy

### Test 3: Partial Load Failure
1. Create extrude + fillet
2. Edit .onecad file: delete sketch JSON
3. Open file
4. **Verify**: RegenFailureDialog appears
5. Choose "Suppress Failed"
6. **Verify**: Document loads, extrude shows as suppressed/failed

### Test 4: History Panel Tree
1. Create: Sketch1→Extrude, Sketch2→Revolve, Boolean(Extrude, Revolve)
2. **Verify**: Tree shows correct hierarchy:
   ```
   ├── Sketch 1
   │   └── Extrude 1
   ├── Sketch 2
   │   └── Revolve 1
   └── Boolean Cut
   ```
3. Click Extrude → **Verify**: Body1 highlighted
4. Right-click Extrude → **Verify**: "Edit Parameters" enabled
5. Right-click Boolean → **Verify**: "Edit Parameters" disabled

### Test 5: Parameter Editing
1. Create extrude (10mm)
2. Double-click in HistoryPanel
3. Change distance to 20mm
4. Click OK
5. **Verify**: Body updated to 20mm height
6. Add fillet, save/reopen
7. **Verify**: Both extrude (20mm) and fillet regenerated

### Test 6: New Operation Records
1. Create extrude → apply fillet
2. **Verify**: ops.jsonl has Extrude AND Fillet records
3. Create shell on filleted body
4. **Verify**: ops.jsonl has Shell record with open faces
5. Boolean two bodies
6. **Verify**: ops.jsonl has Boolean record

### Test 7: Suppression
1. Create extrude → fillet
2. Right-click Extrude → Suppress
3. **Verify**: Extrude and Fillet both grayed, geometry hidden
4. Right-click Extrude → Unsuppress
5. **Verify**: Both restored

### Test 8: Rollback (with Undo/Redo)
1. Create: Extrude → Fillet → Shell
2. Right-click Extrude → "Rollback to Here"
3. **Verify**: Fillet and Shell suppressed (grayed)
4. **Verify**: Only Extrude geometry visible
5. Press Cmd+Z (Undo)
6. **Verify**: Fillet and Shell restored (unsuppressed)
7. Press Cmd+Shift+Z (Redo)
8. **Verify**: Fillet and Shell suppressed again
9. Right-click Fillet → Unsuppress
10. **Verify**: Fillet restored, Shell still suppressed

### Test 9: Live Preview
1. Create extrude (10mm)
2. Double-click Extrude in HistoryPanel
3. Change distance spinbox to 20mm (don't click OK)
4. **Verify**: Viewport shows preview at 20mm (0.5 opacity)
5. Change to 30mm
6. **Verify**: Preview updates to 30mm
7. Click Cancel
8. **Verify**: Body restored to 10mm

### Test 10: Live Preview with Downstream
1. Create: Extrude (10mm) → Fillet (R2mm)
2. Double-click Extrude
3. Change distance to 20mm
4. **Verify**: Preview shows 20mm extrude AND fillet regenerated on taller body
5. Click OK
6. **Verify**: Both bodies committed at new values

### Test 11: Panel Toggle
1. Launch app
2. **Verify**: History panel hidden
3. View menu → History Panel (check)
4. **Verify**: Panel appears
5. Press Cmd+H
6. **Verify**: Panel hides
7. Close and reopen app
8. **Verify**: Panel remembers hidden state



——-

# Gemini research

Architectural Blueprint for Implementing a Parametric Engine in OneCAD1. Introduction: The Parametric ImperativeThe contemporary landscape of Computer-Aided Design (CAD) software is bifurcated into two dominant paradigms: direct modeling, which manipulates geometry explicitly, and parametric modeling, which defines geometry through a history of operations and constraints. For the "OneCAD" application, the transition from a static viewing or direct editing environment to a fully-featured parametric engine represents a foundational architectural pivot. This transformation is not merely an accretion of new features but requires a complete reimplementation of how geometric data is defined, stored, managed, and visualized.Parametric design fundamentally alters the relationship between the user and the digital model. In a direct modeler, a cylinder is stored as a collection of faces and edges—a static B-Rep (Boundary Representation). In a parametric system, that same cylinder is stored as a "feature recipe": a circle sketch on a specific plane, constrained by a radius parameter, and an extrusion operation defined by a height parameter.1 The geometry is transient; the intent is persistent. Implementing this requires a sophisticated orchestration of three distinct computational engines: a Geometric Constraint Solver (GCS) to resolve 2D and 3D relationships, a Boundary Representation (B-Rep) Kernel to perform topological operations, and a Dependency Graph Manager to handle the propagation of changes through the model's history.3This report provides an exhaustive, technical roadmap for implementing this engine within the OneCAD ecosystem. It synthesizes findings from open-source solver architectures (specifically libslvs), industry-standard kernels (OpenCASCADE), and advanced algorithmic solutions to the notorious "Topological Naming Problem" (TNP) found in the Realthunder branch of FreeCAD.4 The analysis prioritizes robust C++ implementation strategies, ensuring that OneCAD can scale from simple part design to complex assemblies without succumbing to the stability issues that plague many nascent parametric tools.1.1 The High-Level ArchitectureThe proposed architecture for OneCAD's parametric engine rests on a triad of subsystems, each responsible for a specific domain of the design intent.SubsystemPrimary FunctionProposed TechnologyCritical Research InsightConstraint SolverResolves spatial relationships (2D/3D)libslvs (SolveSpace)Use symbolic algebra for Jacobian evaluation to ensure convergence.6Geometric KernelGenerates B-Rep topology (Solids/Surfaces)OpenCASCADE (OCCT)Use BRep_Builder for low-level updates to preserve vertex identity.8Dependency GraphManages history and recompute orderCustom DAG / App::DocumentMust handle Topological Naming via persistent hashing to prevent reference breakage.10The integration of these systems requires a strict "data flow" philosophy. User inputs (parameters) feed into the Solver. The Solver's output (resolved wireframes) feeds into the Kernel. The Kernel's output (solids) feeds into the persistent storage. At no point should downstream operations modify upstream data directly; all changes must propagate through the Dependency Graph to maintain logical consistency.2. Geometric Constraint Solving: The Mathematical CoreThe heart of any parametric system is the Sketcher—the environment where users define 2D profiles that drive 3D operations. Unlike a drawing program where lines are placed at static coordinates, a parametric sketcher defines geometry via a system of non-linear algebraic equations. The research overwhelmingly points to libslvs, the solver extracted from SolveSpace, as the optimal open-source engine for this task due to its lightweight C++ footprint, support for 3D constraints, and permissive GPLv3 licensing.62.1 Theoretical Foundations of libslvsGeometric constraint solving is fundamentally a root-finding problem. The system state is defined by a vector of parameters $\mathbf{q} = [x_1, y_1, \dots, x_n, y_n]$, and constraints are defined as error functions $\mathbf{F}(\mathbf{q}) = 0$. For example, a distance constraint between two points $(x_1, y_1)$ and $(x_2, y_2)$ is expressed as:$$(x_1 - x_2)^2 + (y_1 - y_2)^2 - d^2 = 0$$To solve this, libslvs utilizes a Newton-Raphson iteration method. This iterative process requires the calculation of the Jacobian matrix $\mathbf{J}$, which contains the partial derivatives of the constraint equations with respect to the geometric parameters.$$\mathbf{q}_{k+1} = \mathbf{q}_k - \mathbf{J}^{-1}(\mathbf{q}_k)\mathbf{F}(\mathbf{q}_k)$$Unlike generic numerical solvers, libslvs is optimized for geometric CAD. It employs a symbolic algebra system to generate the Jacobian entries precisely, rather than relying on finite difference approximations which can be numerically unstable in CAD applications where high precision is required.13 This architectural choice allows libslvs to handle singularities (such as a line collapsing to a point) more gracefully than generic solvers.2.2 Integrating libslvs into OneCADThe integration of libslvs requires mapping OneCAD’s internal object model to the flat, C-style structures used by the library. The core interface is defined in slvs.h, and effective utilization requires careful memory management and pointer arithmetic.152.2.1 The System StructureOneCAD must maintain a translation layer that populates the Slvs_System structure before every solve cycle. This structure acts as the container for all geometric entities and constraints.Parameter Mapping Strategy:In libslvs, geometry is defined by Slvs_Param (scalar variables). A 2D point is not a single entity but a pair of parameters ($u, v$). OneCAD’s wrapper class must iterate through its own SketchPoint objects and assign them indices in the sys.param array. Critically, the val field of these parameters must be initialized with the current coordinates of the geometry. This provides the "initial guess" for the Newton-Raphson solver. If the initial guess is too far from the solution, the solver may converge to an unintended solution (e.g., flipping a triangle) or fail to converge entirely.17Entity Definition:Once parameters are defined, OneCAD must define Slvs_Entity structures that group these parameters into recognizable geometric forms.Points: Defined as SLVS_E_POINT_2D or SLVS_E_POINT_3D. They reference the parameter indices created previously.Normals: For 3D sketches, OneCAD must define a workplane normal using SLVS_E_NORMAL. This usually involves four parameters representing a quaternion.Lines and Cubics: These entities reference the Point entities. A SLVS_E_LINE_SEGMENT does not contain coordinate data itself; it contains two integer indices pointing to the start and end Slvs_Entity points.2.2.2 The Solving LoopThe execution flow for a solve operation in OneCAD should follow a rigorous "Prepare-Solve-Apply" cycle to ensure data integrity 15:Preparation Phase:Clear the previous Slvs_System memory.Serialize all OneCAD sketch objects into the sys.param and sys.entity arrays.Serialize all geometric constraints into the sys.constraint array.Identify "dragged" entities. If the user is currently manipulating a point with the mouse, that point's parameters must be marked in the sys.dragged array. This instructs the solver to prioritize satisfying the mouse position while relaxing other degrees of freedom if necessary.17Execution Phase:Call Slvs_Solve(&sys, group_handle). This function is blocking and computationally intensive. For complex sketches, OneCAD should run this in a background thread to prevent UI freezing, although for typical sketches (<100 entities), it is near-instantaneous.Application Phase:Check sys.result.If SLVS_RESULT_OKAY: Iterate through sys.param, read the new val double, and update the coordinates of the corresponding OneCAD objects.If SLVS_RESULT_INCONSISTENT: Do not update the geometry. Instead, query sys.faileds to identify which constraints are conflicting. OneCAD should render these constraints in a warning color (e.g., red) to provide feedback to the user.192.3 Handling Degrees of Freedom (DOF)One of the most valuable features for a user is visual feedback on which parts of a sketch are fully defined and which are free to move. libslvs calculates the remaining degrees of freedom (sys.dof) after a solve.17 OneCAD should expose this information visually:Geometry Coloring: Points and lines with 0 DOF should be rendered in a distinct color (e.g., green or black) compared to those with remaining DOF (e.g., white or blue).Status Bar Feedback: Display the message "Fully Constrained" or "3 Degrees of Freedom" to guide the user toward a stable design.2.4 Advanced Constraint ImplementationTo be competitive, OneCAD must support a comprehensive suite of constraints. The libslvs architecture supports these natively, but OneCAD must provide the UI logic to create them.Distance/Length: Maps to SLVS_C_PT_PT_DISTANCE.Angle: Maps to SLVS_C_ANGLE. Note that libslvs handles angles using trigonometric constraints, which avoids the discontinuity issues of simple arctangent calculations.Tangency: Maps to SLVS_C_TANGENT. This enforces that a line and a circle (or two circles) share a point and a derivative vector.Symmetry: This is a high-value constraint for engineering. It requires identifying a line of symmetry and two points. libslvs enforces this by constraining the distance from the points to the line to be equal and the segment connecting the points to be perpendicular to the line.3. The Geometric Kernel: OpenCASCADE IntegrationWhile the solver handles the "skeleton" of the model (wires and profiles), the generation of surfaces and solids requires a robust Boundary Representation (B-Rep) kernel. OpenCASCADE Technology (OCCT) is the industry standard for open-source CAD, providing the complex algorithms needed for boolean operations, fillets, and shelling.20 For OneCAD, integrating OCCT involves wrapping its C++ classes into OneCAD's feature objects.3.1 The B-Rep Data StructureOneCAD must adopt the topological hierarchy defined by OCCT's TopoDS package. Understanding this hierarchy is crucial for the "Topological Naming" solution discussed later.TopoDS_Vertex: A zero-dimensional point in 3D space.TopoDS_Edge: A one-dimensional curve bounded by vertices.TopoDS_Wire: A connected sequence of edges (the output of the Sketcher).TopoDS_Face: A surface bounded by one or more wires.TopoDS_Solid: A volume bounded by a closed shell of faces.TopoDS_Compound: A container for grouping disparate shapes.Every parametric feature in OneCAD will hold a smart pointer (Handle) to a TopoDS_Shape. For example, an Extrude feature will own a TopoDS_Solid representing its current state.83.2 The BRep_Builder and Geometry UpdatesA critical challenge in parametric modeling is updating geometry efficiently. When the libslvs solver moves a vertex in a sketch, OneCAD must update the corresponding TopoDS_Wire without destroying and recreating the entire object graph, if possible. OCCT provides the BRep_Builder class for this purpose.8The Update Pattern:The TopoDS_Shape architecture relies on shared pointers. Multiple edges may share the same vertex. Therefore, modifying a vertex's coordinate via BRep_Builder::UpdateVertex instantly propagates that change to all connected edges.C++BRep_Builder builder;
TopoDS_Vertex V = mySketchVertex.GetTopoShape();
gp_Pnt new_coords(x, y, z);
builder.UpdateVertex(V, new_coords, tolerance);
This method is significantly faster than rebuilding the topology from scratch, which involves memory allocation and pointer linking. However, BRep_Builder must be used with caution. It does not automatically validate the geometry; moving a vertex too far might cause a self-intersection in the wire. OneCAD must run validity checks (BRepCheck_Analyzer) after such updates to ensure topological integrity.223.3 Feature Implementation StrategiesOneCAD needs to implement a library of standard features that bridge the gap between the Sketcher and the B-Rep.3.3.1 The Extrusion (Pad) FeatureThis is the most fundamental 3D operation.Input: A TopoDS_Wire from a Sketch and a gp_Vec (vector) defining direction and magnitude.Validation: Ensure the wire is planar and closed.Operation: Use BRepPrimAPI_MakePrism. This class takes the base shape and sweeps it linearly.Output: A TopoDS_Shape (usually a Solid) is returned.History Tracking: Crucially, BRepPrimAPI_MakePrism provides methods like Generated() and FirstShape(). OneCAD must query these to map the input edges (from the sketch) to the output faces (the walls of the extrusion). This mapping is the raw data required for the Topological Naming system.3.3.2 Boolean OperationsParametric modeling relies heavily on Constructive Solid Geometry (CSG).Cut/Fuse/Common: Implemented via BRepAlgoAPI_BooleanOperation.Fuzzy Booleans: Modern OCCT versions support "Fuzzy" booleans which use a larger tolerance to handle coplanar faces or slightly misaligned geometry. OneCAD should expose this tolerance parameter to the user to handle difficult geometric cases.214. The Topological Naming Problem (TNP): The Critical Failure ModeThe "Topological Naming Problem" (TNP) is the single most significant barrier to creating a robust parametric modeler. It occurs when a recompute operation changes the internal indexing of geometric entities, causing downstream features to attach to the wrong geometry or fail entirely. Without a solution to the TNP, OneCAD will be fragile and unsuitable for professional work.44.1 Analysis of the Failure MechanismConsider a user workflow:Create a Cube. OCCT indexes its faces 1 through 6.Create a Sketch on "Face 5" (the top face).Modify the Cube dimensions. OCCT rebuilds the B-Rep.Due to the internal traversal algorithm, the top face might now be indexed as "Face 2."The Sketch, still pointing to "Face 5," now jumps to the side of the cube or fails if the cube only has 4 faces (e.g., if it became a tetrahedron).4.2 The Realthunder Solution: Persistent Deep IndexingResearch into the "Realthunder" branch of FreeCAD reveals a robust algorithmic solution that OneCAD should emulate. The core concept is to replace unstable integer indices (1, 2, 3) with stable, history-based string identifiers (hashes).54.2.1 The ElementMap ArchitectureOneCAD must attach a persistent data structure, the ElementMap, to every feature object. This map functions as a translation layer between the stable IDs and the transient OCCT indices.Key: A Stable ID string (e.g., Face_Extrude1_Edge2).Value: The current transient memory address or index of the TopoDS_Shape.When a feature recomputes, it must not simply discard the old geometry. It must generate the new geometry and then populate the ElementMap by tracing the lineage of every new face and edge.104.2.2 The Hashing AlgorithmThe Stable ID is constructed by hashing the history of operations that created the element. OneCAD should implement a recursive naming schema:$$\text{ID}_{\text{face}} = \text{Hash}(\text{ParentFeatureTag} + \text{OperationType} + \text{SourceElementID})$$Example Scenario:Base Feature: A Rectangle Sketch has 4 edges. They are named Edge1, Edge2, Edge3, Edge4 (based on stable geometric sorting or creation order).Extrusion: The user extrudes this sketch. The extrusion creates:Top/Bottom Faces: Generated from the wire.Side Faces: Generated from the edges.Naming: The side face corresponding to Edge1 is assigned the name Face_Pad1_Edge1. Even if the array order of faces changes in OCCT, this face can always be identified by querying the Generated(Edge1) list from the BRepPrimAPI.4.2.3 Implementation DetailsOneCAD needs to implement a NamingService class in C++.C++class NamingService {
public:
    // Called after a feature recomputes
    void MapGeometry(Feature* feature, const TopoDS_Shape& newShape) {
        // 1. Get the algorithm history (e.g., BRepAlgoAPI)
        // 2. For every input element (edges of the sketch):
        //    a. Find what it generated (new faces)
        //    b. Construct the Stable Name (e.g., "Face_from_Edge1")
        //    c. Store in feature->ElementMap
    }
};
This service must be integrated into the end of every Execute() method in the Dependency Graph. The ElementMap must be serialized to disk (JSON/XML) so that names persist across sessions.114.3 Handling Topology Changes (Ambiguity)Sometimes, topology changes fundamentally (e.g., a face is split into two). The ElementMap allows OneCAD to handle this gracefully.Split: If FaceA becomes FaceA_1 and FaceA_2, the map can store both under a composite key or use an index modifier.Deletion: If FaceA disappears, the map entry is empty. When a downstream Sketch tries to attach to FaceA, OneCAD can look up the map, see the deletion, and present a specific error ("Reference FaceA lost") rather than crashing or attaching randomly. This "Ambiguity Solving" capability is a hallmark of high-end CAD systems.275. Dependency Management: The Parametric GraphWhile the solver handles the mathematics of a single sketch, the Dependency Graph manages the logic of the entire document. A parametric model is a Directed Acyclic Graph (DAG) where nodes are features and edges represent data dependencies.285.1 The App::Document ArchitectureFollowing the architecture patterns found in FreeCAD and other open-source systems, OneCAD should implement a central App::Document class that owns the DAG.30App::DocumentObject: The base class for all nodes. It holds:Properties: Parameters (Length, Width) and Links (pointer to Sketch).State: "Touched" (Dirty), "Valid", "Error".ViewProvider: A separate object responsible for rendering the geometry in the 3D view (Separation of Concerns).5.2 The Recompute EngineThe efficiency of OneCAD depends on a smart recompute engine. A naive implementation that rebuilds every feature on every frame will be unusably slow. OneCAD must implement a "Lazy Recompute" strategy.The Recompute Algorithm:Trigger: User changes a parameter on SketchA.Mark Dirty: The engine marks SketchA as Touched.Propagation: The engine traverses the dependency edges. Extrude1 depends on SketchA, so it is marked Touched. Fillet1 depends on Extrude1, so it is marked Touched.Topological Sort: The engine performs a topological sort on the list of Touched objects to ensure parents are computed before children.29Execution: The engine calls onExecute() on each sorted object sequentially.If onExecute() fails (e.g., Solver divergence), the propagation stops, and the object is marked Error. Downstream objects are marked Invalid.5.3 Cycle DetectionParametric systems must strictly forbid circular dependencies (e.g., Sketch A depends on Pad B, which depends on Sketch A). OneCAD must implement cycle detection logic in the Link property setter.Algorithm: Before establishing a link $A \to B$, perform a Depth First Search (DFS) starting from $A$. If $B$ is encountered in the upstream ancestry of $A$, reject the link and throw an exception. This check must happen at the UI level to prevent the user from creating invalid states.326. Data Persistence and File FormatThe storage of parametric data differs fundamentally from direct modeling formats like STL or STEP. OneCAD requires a format that stores the logic, not just the triangles.6.1 OCAF vs. Custom SerializationThe Open CASCADE Application Framework (OCAF) provides a built-in document structure based on a label tree.20OCAF Pros: Handles Undo/Redo stacks, binary storage, and external file linking out of the box.OCAF Cons: The API is notoriously complex and verbose, presenting a steep learning curve.Custom Recommendation: For a modern application, a hybrid approach is often better. Use XML or JSON for the high-level graph structure (Features, Parameters, Dependencies) and use OCCT's BRepTools::Write to binary blobs for the cached geometry. This allows the file to be human-readable/debuggable (the JSON part) while maintaining performance for large geometry (the binary part).6.2 The "OneCAD" File StructureThe recommended file format is a ZIP container (similar to .docx or .fcstd):Document.xml: The serialized DAG, parameter values, and ElementMap data.PartShape.brp: The cached B-Rep geometry for the final result (allows quick viewing without recomputing).GuiDocument.xml: View-specific data (camera position, object colors, visibility).347. User Experience: Interaction and FeedbackImplementing the math is only half the battle; the UX determines the usability. Parametric CAD UX is defined by feedback loops.7.1 Visualizing Constraints and DOFAs noted in the Solver section, visualizing Degrees of Freedom (DOF) is critical.Interaction Logic: When a user drags an unconstrained point, OneCAD must use libslvs's "dragged" parameter functionality. This solves the system by minimizing the distance between the mouse cursor and the dragged point, subject to all other constraints.17 This provides the "rubber band" feel that users expect.Conflict Resolution: If the solver returns SLVS_RESULT_INCONSISTENT, OneCAD should not just fail silently. It should attempt to identify the "Minimal Conflict Set." This involves iteratively disabling constraints and re-solving to find which specific constraint is causing the lock. These constraints should be highlighted in red in the UI.197.2 The "Wait" Cursor and ThreadingRecomputing a complex history tree can take seconds. OneCAD must not block the main UI thread.Thread Architecture: The App::Document recompute method should run in a worker thread. The 3D View (OpenGL/Vulkan) remains responsive. When the worker thread finishes, it swaps the old TopoDS_Shape with the new one and signals the View to repaint. This requires careful mutex locking on the OCCT data structures, as OCCT is not fully thread-safe for concurrent modification of shared shapes.8. Implementation RoadmapImplementing this architecture is a multi-phase process. The following roadmap structures the development to minimize risk.Phase 1: The Sketcher Core (Months 1-3)Goal: A standalone 2D constraint solver application.Tasks:Compile libslvs as a static library.Implement the C++ wrapper for Slvs_System.Build the 2D UI for creating points, lines, circles.Implement dragging logic and DOF visualization.Deliverable: A program where you can draw a triangle, constrain sides to be equal, and drag a corner to see it behave as an isosceles triangle.Phase 2: The Kernel Bridge (Months 4-6)Goal: Generating 3D solids from sketches.Tasks:Integrate OpenCASCADE.Implement SketchObject that converts libslvs output to TopoDS_Wire.Implement Extrude feature using BRepPrimAPI.Implement the App::Document DAG to link Sketch to Extrude.Deliverable: A program where modifying a dimension in the Sketch automatically updates the 3D Extrusion.Phase 3: The Topological Stability (Months 7-12)Goal: Robust referencing (Solving the TNP).Tasks:Implement the ElementMap structure and serialization.Implement the NamingService with hashing algorithms.Create a Fillet feature that uses Stable IDs to reference edges.Stress test: Create a block, fillet an edge, then change the block size to ensure the fillet stays attached.Deliverable: A robust parametric modeler capable of editing history without model corruption.9. ConclusionImplementing a parametric engine in OneCAD is a significant engineering undertaking that elevates the software from a simple utility to a professional design tool. The path requires moving from explicit geometry storage to a dynamic, history-based procedural generation model.The research indicates that the most viable path involves a hybrid integration of libslvs for constraint solving, OpenCASCADE for B-Rep manipulation, and a custom Dependency Graph reinforced by Realthunder’s Topological Naming algorithm. By rigorously adhering to the "Prepare-Solve-Apply" cycle for solving and the "Stable Hashing" strategy for naming, OneCAD can overcome the common pitfalls of parametric CAD development. This architecture ensures not just geometric correctness, but the stability and predictability required by professional engineers. The result will be a system where design intent is preserved, enabling users to iterate and refine their models with confidence.

——-

# ChatGPT research

Designing a Parametric Engine with OCCT

A feature‐based CAD engine typically stores a history tree (CSG/feature tree) of operations and parameters.  A simple (“full‐replay”) strategy is to recompute the entire feature tree whenever a parameter changes.  In practice, one constructs a directed acyclic graph (DAG) of features so that changes propagate in order.  For example, OCCT’s Function Mechanism lets you define each feature as a “function” with input and output attributes: the framework builds a dependency graph and iterates it (cone → cylinders → final “nail” in the classic example) ￼ ￼.  In the simplest engine, one would just visit all functions in topological order every time.  This guarantees correctness (like re‐evaluating the entire CSG tree) but can be inefficient for large models.

More advanced engines use dirty‐flag/caching and selective rebuild.  When a parameter or feature changes, mark that node “dirty” and propagate the flag to all dependent features.  Unchanged subtrees are skipped or cached.  For example, Kleinert et al. describe a lazy‐evaluation engine (the “grunk” framework) that caches feature outputs: on a parameter change it invalidates only the caches of dependent nodes, leaving all other results intact ￼ ￼.  Concretely, “already computed intermediate results unaffected by the parameter change can be cached and reused; only caches of results depending on the changed parameter are invalidated” ￼.  Thus only the dirty branches of the graph are recomputed (“lazy” generation), greatly reducing work.  A visualization of such a dependency graph is shown below – the cone feature is independent, the cylinders depend on it, and the final “nail” depends on all ￼ ￼:

Figure: Example dependency graph for a parametric “nail” model. Each node is a feature (cone or cylinder) and arrows indicate dependencies. OCCT’s Function mechanism builds and traverses such a graph ￼ ￼.

Key strategies: Start with a full replay approach (simply recompute all features) to ensure correctness.  Then introduce a DAG or dependency graph to skip unaffected features.  Use dirty flags and caches so that on a change you only recompute children of the changed node ￼ ￼.  OCCT’s OCAF framework can help: it supports storing attributes and param dependencies in a document, and even has undo/redo built‐in ￼.  In OCAF one could implement each feature as a TFunction with attribute references (as in the “nail” example) so that OCCT itself iterates them in correct order ￼ ￼.  At minimum, maintain a feature tree (e.g. in a custom data structure or OCAF) and on updates traverse it either fully or selectively.

Expression Evaluation & Constraint Solving

Parametric dimensions are often given by expressions or formulas (e.g. “Length = Diameter + 10”).  The engine needs an expression parser/evaluator.  One approach (used in many param systems) is to parse each expression to an abstract syntax tree (AST) and evaluate on demand.  For example, the OpenBrIM platform’s param engine parses JavaScript‐style expressions, resolves identifiers (parameter names, with scope rules), and computes arithmetic/logical functions ￼ ￼.  It does this lazily: each formula is stored as an AST at definition time, but only evaluated when its value is needed (cached thereafter) ￼.  A similar design can be used: support numeric parameters (with optional units) and string references, and implement an evaluator with +,–,*,/,% power, comparisons, trig functions, etc. ￼.  Caching results of expressions (and invalidating when inputs change) mirrors the feature caching above.

For sketch constraints, one needs a geometric constraint solver.  A sketch typically consists of 2D entities (lines, circles, etc.) plus dimensional and geometric constraints (lengths, angles, coincidences, tangencies, etc.).  These can be translated into algebraic equations in the coordinates of sketch points ￼.  A common solution is to use a numerical solver (e.g. Gauss-Newton/Newton-Raphson) that iteratively adjusts point coordinates to satisfy all constraints.  In practice, mature solutions exist: OpenCASCADE includes Ledas LGS 2D/3D solvers (used by many CAD systems) that handle sketches and assembly constraints ￼ ￼.  LGS supports geometric constraints (coincidence, parallel, perpendicular, tangency, etc.) and dimension constraints (distances, angles, radii) and moves the entities to satisfy all ￼.  If LGS is not available, open-source alternatives like SolveSpace’s solver or custom code (using e.g. Eigen for solving the Jacobian) can be used.  Note that Newton-based solvers converge quickly but need a good initial guess and must detect over/under-constrained cases ￼.  The key is to formulate constraints as equations (or a residual to minimize) and solve them numerically.

In summary: implement an expression engine for param formulas (parse + AST + lazy eval ￼ ￼).  Store each dimension/parameter as a named variable that expressions can reference (for example, using a distance-based name resolution or scoping as in  ￼).  For sketches, integrate a constraint solver: either OCCT’s LGS or another library.  The solver takes the sketch entities and constraints, then computes the precise positions/angles.  This solver should run whenever sketch parameters or constraints change (triggered by dirty flags in the sketch nodes).

Sketches and Feature Parameterization

Sketches and 3D features must store their parameters and dependencies in the model.  A common approach is to attach parameters as attributes in the model tree.  For example, in OCCT’s OCAF one can build a document tree where each feature (sketch, extrusion, etc.) is a node with attributes for its parameters (lengths, angles, etc.) and references to other shapes.  In the “nail” example, the cone object stores its position, radius and height.  The first cylinder’s attributes are defined by references to the cone’s attributes (position = cone.position+cone.height, radius = cone.radius, height = 3×cone.height) ￼.  This means the cylinders depend on the cone via those references.  OCCT’s Function Mechanism uses exactly this: each attribute can be a reference with a multiplier, and the framework automatically updates all dependents when the base parameter changes ￼ ￼.

In practice, for sketches one might store each geometric entity and constraint as OCAF attributes (or in a custom class).  Constraint solver inputs (e.g. a circle’s center and radius) are parameters, and constraints refer to those.  In 3D, features like extrude, rotate, fillet have numeric parameters (depth, angles, offsets).  These should be stored in the feature node (for example, an OCCT attribute holding a double or an expression).  The result of a feature (a new shape) is also stored (often as a TopoDS_Shape attribute).  In OCCT/OCAF terms, “Shape attributes” can hold the geometry and parameter attributes hold numeric data; OCAF automatically links them in the document so that, for example, a shape attribute “sits deeper” in the tree and can be restored on reload ￼.

Dependencies are thus managed via these references.  In OCAF one implements TFunction_Driver::Arguments() and ::Results() so the mechanism knows which attributes are inputs/outputs of each feature.  The system then builds a DAG of functions and can iterate it to recompute outputs in order ￼ ￼.  In simpler custom models, you would instead build your own graph or scan the feature list each update.  Key points: ensure each parameter is uniquely identified and can reference others (e.g. via names or pointers), and store features in a structure that preserves order.  Use OCAF’s undo/redo and attribute system to get built-in history and naming support ￼.

Practical Implementation with OCCT
	•	Use OCAF for data management: OCCT’s Application Framework (OCAF) is designed for parametric modeling. It lets you attach named attributes (parameters, expressions, shapes) to a document hierarchy ￼.  OCAF handles persistence (open/save), undo/redo and can store “history” of operations. You can define each feature as an OCAF “function” (with input/output attributes) so that OCCT can traverse the dependency graph automatically ￼ ￼.  Predefined attribute types (integer, real, string, reference) can hold your parameter values; use the “Real” or “Integer” attributes to store dimensions, and “Reference” to link one param to another.
	•	Feature shape storage: For each feature, create a shape attribute (TopoDS_Shape) to hold the result of the OCCT modeling algorithm (e.g. extruded solid).  Attach parameters as separate attributes on the same OCAF label or subtree.  Use XDE extended attributes if you want to export metadata: OCCT’s XDE modules allow writing extra info (names, layers, custom data) into STEP/IGES ￼, which can help preserve param info on data exchange.
	•	Geometry kernels: Leverage OCCT’s modeling classes (e.g. BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder, BRepFeat_MakeCut, etc.) to implement feature operations. Each time a feature’s parameters change, call the appropriate OCCT construction functions to rebuild its shape (either during full replay or selective update).  Since OCCT primitives are robust, most of the complexity is in managing the feature graph rather than the geometry itself.
	•	Expression engine: OCCT has no built-in formula parser, so integrate one.  Options include embedding a scripting engine (e.g. JavaScript V8 or Python), or using a math parser library (muparser, ExprTk, etc.).  The key is to evaluate parameter formulas and update the numeric attributes before rebuilding geometry.  Use lazy evaluation if possible: store the raw expression string in OCAF and evaluate only when needed (as in  ￼) to avoid unnecessary work.
	•	Constraint solver: OCCT’s licensing no longer includes Ledas LGS by default, but if available (or in older versions) you can use it for sketch solving.  Otherwise, you may link to an open solver like SolveSpace or write your own Newton solver.  The solver should take a list of 2D entities and constraints, then solve for the unknown coordinates.  Once solved, extract the numeric values and store them as the parameter attributes of the sketch.
	•	Efficiency and caching: Implement caching of feature outputs as suggested above ￼ ￼.  For example, after computing a feature’s shape, store it in OCAF.  On a parameter change, only recompute features whose input attributes have changed (mark them dirty).  You can use OCAF notifications or a manual flag system: when an attribute changes, propagate flags to dependent labels.  OCCT’s Data Exchange Extended (XDE) caching can also help if you use OCAF’s built‐in “is modified” flags.  The grunk example shows one way to do this: maintain a “cache valid” flag per node and clear children’s caches on updates ￼ ￼.
	•	Extensibility: Design your system in layers.  For example, use a plugin architecture (as in grunk ￼) so you can add new feature types or geometry operations later.  Store param data in a human-readable or version-controlled way (grunk uses YAML ￼).  Use OCAF’s undo/redo for “rollback” capability ￼.  For versioning, consider exporting the param tree or using a version control system on OCAF’s files.  Also consider storing sensitive edit history or copy-on-write structures if needed.

In summary, a basic parametric engine on OCCT can start by using OCAF to hold a linear feature list and performing full replays on each change.  Then add dependency tracking and lazy regeneration via OCAF references or a custom graph.  Implement an expression evaluator (referencing parameters) and integrate a 2D constraint solver for sketches.  Cache intermediate results to skip unchanged parts (cf. grunk’s DAG caching ￼ ￼).  Leveraging OCCT’s strengths (brep modeling, OCAF attribute system, extended data) will make the engine robust and extensible, enabling future features like partial regen, geometric inference, and built-in undo/history ￼ ￼.

Sources: Scholarly and technical resources on CAD parametrics and OCCT features ￼ ￼ ￼ ￼ ￼ ￼ were used to inform these recommendations.