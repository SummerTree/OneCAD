# Run sketch engine prototypes

Build and run all sketch-related prototypes in sequence and report results.

1. Build and run in order: proto_sketch_geometry, proto_sketch_constraints, proto_sketch_solver, proto_loop_detector, proto_face_builder.
2. For each: `cmake --build build --target <target>` then `./build/tests/<target>`.
3. Report pass/fail for each. If any fail, summarize the failure (compile error, assertion, or test output) and suggest next steps.
