---
name: proto-test
description: Run or create prototype tests for OneCAD kernel/sketch validation. Use for testing, validation, TDD workflows.
license: MIT
metadata:
  category: testing
  priority: high
---

## When to Use

- Running tests: `make test`, specific proto targets
- Creating new test: validation of new features
- TDD workflow: write test → implement → verify

## Run Tests

### Default tests (kernel validation)
```bash
make test
```
Runs: proto_custom_map, proto_tnaming, proto_elementmap_rigorous

### Specific test
```bash
cmake --build build --target proto_<name>
./build/tests/proto_<name>
```

### All 15 proto tests
| Target | Validates |
|--------|-----------|
| proto_elementmap_rigorous | **REQUIRED before kernel changes** |
| proto_sketch_geometry | Point, Line, Arc, Circle, Ellipse entities |
| proto_sketch_constraints | Constraint satisfaction |
| proto_sketch_solver | DOF calculation, solver |
| proto_loop_detector | Loop/region detection |
| proto_face_builder | Wire→Face construction |
| proto_regeneration | Full regeneration (351 LOC, largest) |

## Create New Test

1. Create `tests/prototypes/proto_<name>.cpp`
2. Add to `tests/CMakeLists.txt`:
```cmake
add_executable(proto_<name> prototypes/proto_<name>.cpp)
target_link_libraries(proto_<name> PRIVATE onecad_core)
target_include_directories(proto_<name> PRIVATE ${CMAKE_SOURCE_DIR}/src)
```

### Validation patterns
```cpp
// Tolerance comparison
bool approx(double a, double b, double tol = 1e-6);

// Shape validity
BRepCheck_Analyzer analyzer(shape);
bool valid = analyzer.IsValid();

// Constraint satisfaction
bool satisfied = constraint.isSatisfied(sketch, 1e-6);
```
