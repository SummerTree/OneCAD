# Format changed C/C++ files

Run clang-format only on changed C/C++ files.

1. Get list of changed files: `git diff --name-only` (or `git diff --name-only HEAD` for staged + unstaged). Filter to `.cpp`, `.h`, `.hpp`, `.c`, `.cxx`, `.cc`.
2. If no changes, say so. Otherwise run `clang-format -i <file1> <file2> ...` for each changed file.
3. Use the project's .clang-format configuration if present. Do not reformat unrelated files.
