# Build and run prototype

Build the specified prototype target and run it.

1. If user provided a target name (e.g. proto_sketch_solver, proto_elementmap_rigorous): build that target with `cmake --build build --target <target>` then run `./build/tests/<target>`.
2. If no target specified: list available prototypes from `tests/CMakeLists.txt` (grep for `add_executable(proto_` or read the file) and suggest which to run based on context.
3. Report any build or runtime failures.

Common targets: proto_sketch_geometry, proto_sketch_constraints, proto_sketch_solver, proto_loop_detector, proto_face_builder, proto_elementmap_rigorous, proto_regeneration, test_compile.
