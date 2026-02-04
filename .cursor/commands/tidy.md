# Run clang-tidy on changed files

Run clang-tidy on changed C/C++ files using compile_commands.json from build/.

1. Ensure project is configured with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` so `build/compile_commands.json` exists.
2. Get changed files (e.g. `git diff --name-only` filtered to .cpp/.h/.hpp).
3. Run clang-tidy with the compile_commands.json. Example: `clang-tidy -p build <files>`.
4. Focus on correctness and safety (e.g. OCCT handle usage, null checks). Skip style-only warnings unless the user asked for style.

If build/ does not exist or compile_commands.json is missing, run configure first.
