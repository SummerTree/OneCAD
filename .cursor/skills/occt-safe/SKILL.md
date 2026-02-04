---
name: occt-safe
description: OCCT Handle<> null-check patterns. Use when writing kernel code to prevent crashes.
license: MIT
metadata:
  category: kernel
  priority: high
---

## When to Use

- Writing any OCCT-touching code
- Debugging crashes in kernel
- Code review of kernel changes

## CRITICAL: Always Null-Check Handle<>

OCCT Handle<> objects can be null. Dereferencing without check = crash.

## Safe Patterns

### Before accessing shape
```cpp
Handle(TopoDS_Shape) shape = ...;
if (shape.IsNull()) {
    // Handle error
    return;
}
// Now safe to use
```

### Before BRep operations
```cpp
Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
if (surface.IsNull()) {
    std::cerr << "No surface on face" << std::endl;
    return false;
}
```

### Shape validity check
```cpp
if (shape.IsNull()) return false;
BRepCheck_Analyzer analyzer(shape);
if (!analyzer.IsValid()) {
    // Shape is malformed
}
```

### Builder result check
```cpp
BRepPrimAPI_MakeBox mkBox(10, 10, 10);
if (!mkBox.IsDone()) {
    // Construction failed
    return;
}
TopoDS_Shape box = mkBox.Shape();
```

## Common Crash Points

1. `BRep_Tool::Surface(face)` - face may have no surface
2. `BRep_Tool::Curve(edge)` - edge may be degenerate
3. `TopoDS::Face(shape)` - shape may not be a face
4. `TopExp_Explorer` iteration - shapes may be empty
5. `BRepAlgoAPI_*` results - Boolean ops can fail

## ElementMap Interaction

```cpp
const ElementMapEntry* entry = emap.find(topId);
if (!entry || entry->shape.IsNull()) {
    // Entry doesn't exist or shape was deleted
}
```
