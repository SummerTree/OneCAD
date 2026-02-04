---
name: review-kernel
description: Pre-commit checklist for kernel changes. Use before committing ElementMap, OCCT, or solver modifications.
license: MIT
metadata:
  category: review
  priority: high
---

## When to Use

- Before committing kernel changes
- Self-review for OCCT code
- ElementMap modifications

## REQUIRED: Run Before Committing Kernel Changes

```bash
cmake --build build --target proto_elementmap_rigorous
./build/tests/proto_elementmap_rigorous
```

## Checklist

### 1. ElementMap Hash Stability
- [ ] No changes to descriptor field ordering
- [ ] Hash function unchanged (14-field)
- [ ] If changed: document migration, update golden test

### 2. OCCT Handle<> Safety
- [ ] All Handle<> checked for IsNull() before use
- [ ] Builder IsDone() checked before Shape()
- [ ] BRep_Tool results null-checked

### 3. Shape Evolution Tracking
- [ ] Modified/Generated/Deleted properly tracked
- [ ] update() called with operation name
- [ ] Deleted IDs returned and handled

### 4. Memory Ownership
- [ ] unique_ptr for single-owner shapes
- [ ] No raw new without parent/owner
- [ ] No dangling pointers to deleted shapes

### 5. Numerical Robustness
- [ ] Tolerances use constants:: values
- [ ] No floating-point equality (use approx())
- [ ] Degenerate geometry prevented

## Additional Tests for Specific Areas

| Change Area | Test to Run |
|-------------|-------------|
| Sketch entities | proto_sketch_geometry |
| Constraints | proto_sketch_constraints |
| Solver | proto_sketch_solver |
| Loop detection | proto_loop_detector |
| Face building | proto_face_builder |
| Full pipeline | proto_regeneration |
