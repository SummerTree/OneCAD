# Configure project (CMake)

Configure the project using CMake with Ninja generator and compile_commands.json export.

From repo root:
- `mkdir -p build && cd build`
- `cmake .. -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt`
- Use CMakePresets.json if available; otherwise use the command above.

After configuration, build with `cmake --build build` or `make`.
