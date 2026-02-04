---
name: sketch-tool
description: Create new sketch tools (PolygonTool, SplineTool, etc.) following OneCAD patterns. Use when implementing drawing tools.
license: MIT
metadata:
  category: sketching
  priority: high
---

## When to Use

- Implementing new sketch drawing tool
- Following tool lifecycle pattern
- Integrating snap/constraint systems

## Architecture

- Base class: `src/core/sketch/tools/SketchTool.h`
- Manager: `src/core/sketch/tools/SketchToolManager.h/cpp`
- Existing tools: LineTool, ArcTool, CircleTool, RectangleTool, EllipseTool, MirrorTool, TrimTool

## State Machine

```cpp
enum class State { Idle, FirstClick, Drawing };
```

- Idle → FirstClick (on first click)
- FirstClick → Drawing (multi-point tools) or Idle (two-click tools)
- ESC cancels, returns to Idle

## Required Interface

```cpp
class MyTool : public SketchTool {
    void onMousePress(const Vec2d& pos, Qt::MouseButton button) override;
    void onMouseMove(const Vec2d& pos) override;
    void onMouseRelease(const Vec2d& pos, Qt::MouseButton button) override;
    void onKeyPress(Qt::Key key) override;
    void cancel() override;
    void render(SketchRenderer& renderer) override;
    std::string name() const override;
};
```

## Registration Steps

1. Add enum to `ToolType` in SketchToolManager.h
2. Add case in `createTool()` factory (SketchToolManager.cpp:216-236)
3. Connect UI button in ContextToolbar

## Key Patterns

### Snap integration
```cpp
Vec2d finalPos = getSnappedPos(cursorPos);
if (snapResult_.snapped && !snapResult_.pointId.empty()) {
    useExistingPointId = snapResult_.pointId;
}
```

### Degenerate prevention
```cpp
if (length < constants::MIN_GEOMETRY_SIZE) return;
```

### Auto-constraint
```cpp
auto constraints = autoConstrainer_->inferLineConstraints(...);
applyInferredConstraints(constraints, entityId);
```

### Preview rendering
```cpp
void render(SketchRenderer& renderer) override {
    if (state_ == State::FirstClick) {
        renderer.setPreviewLine(startPoint_, currentPoint_);
    }
}
```

### Creation flag for signals
```cpp
bool wasCreated() const { return created_; }
void clearCreatedFlag() { created_ = false; }
```

## CRITICAL: Coordinate System

SketchPlane::XY() uses non-standard mapping:
- Sketch X → World Y+ (0,1,0)
- Sketch Y → World X- (-1,0,0)
