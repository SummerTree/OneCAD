---
name: constraint
description: Add new constraint types to sketch constraint system. Use when implementing Collinear, Lock, etc.
invocable: true
author: OneCAD
license: MIT
metadata:
  category: sketching
  priority: high
---

## When to Use

- Adding new geometric constraint
- Extending solver system
- DOF calculation changes

## Architecture

- Types enum: `src/core/sketch/SketchTypes.h:35-61`
- Base class: `src/core/sketch/SketchConstraint.h`
- Implementations: `src/core/sketch/constraints/Constraints.h/cpp`
- Solver: `src/core/sketch/solver/ConstraintSolver.h/cpp`

## Existing Types (18)

**Positional:** Coincident, Horizontal, Vertical, Fixed, Midpoint, OnCurve, Concentric
**Relational:** Parallel, Perpendicular, Tangent, Equal
**Dimensional:** Distance, HorizontalDistance, VerticalDistance, Angle, Radius, Diameter
**Symmetry:** Symmetric

## Required Interface

```cpp
class MyConstraint : public SketchConstraint {
    ConstraintType type() const override;
    std::string typeName() const override;
    std::string toString() const override;
    std::vector<EntityID> referencedEntities() const override;
    int degreesRemoved() const override;  // DOF reduction
    bool isSatisfied(const Sketch& sketch, double tolerance) const override;
    double getError(const Sketch& sketch) const override;
    void serialize(QJsonObject& json) const override;
    bool deserialize(const QJsonObject& json) override;
    gp_Pnt2d getIconPosition(const Sketch& sketch) const override;
};
```

## Steps to Add New Constraint

1. **Add to enum** in SketchTypes.h
2. **Create class** in Constraints.h (inherit SketchConstraint or DimensionalConstraint)
3. **Implement methods** in Constraints.cpp
4. **Register factory** in SketchConstraint.cpp:16-31:
   ```cpp
   ConstraintFactory::registerType<MyConstraint>("MyConstraint");
   ```
5. **Add convenience method** to Sketch.h:
   ```cpp
   ConstraintID addMyConstraint(EntityID e1, EntityID e2);
   ```
6. **Update DOF table** in ConstraintSolver.h:388-432
7. **Add solver translation** in ConstraintSolver::translateConstraint()
8. **(Optional)** Add inference in AutoConstrainer

## DOF Reference

| Constraint | DOF Removed |
|------------|-------------|
| Coincident | 2 |
| Horizontal/Vertical | 1 |
| Fixed | 2 |
| Distance/Angle/Radius | 1 |
| Parallel/Perpendicular | 1 |
| Tangent | 1 |
| Equal | 1 |

## Validation Test

Run after changes:
```bash
cmake --build build --target proto_sketch_constraints
./build/tests/proto_sketch_constraints
```
