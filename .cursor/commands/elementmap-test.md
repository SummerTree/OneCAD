# Run ElementMap validation

Build and run proto_elementmap_rigorous to validate the topological naming system.

1. Run `cmake --build build --target proto_elementmap_rigorous`.
2. Run `./build/tests/proto_elementmap_rigorous`.
3. Report success or failure. This is **required** before merging any changes to `src/kernel/elementmap/` or ElementMap-related code.

If the user is about to commit kernel changes, remind them that this test must pass.
