# Build OneCAD

Build the OneCAD target with Ninja. From repo root:

1. If build directory does not exist or CMake was not run: `make init` or `mkdir -p build && cd build && cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cd ..`
2. Run `cmake --build build` (or `make`).
3. If build fails, analyze compiler errors and propose minimal fixes. Do not change unrelated code.

Prefer `make run` for build + run in one step.
