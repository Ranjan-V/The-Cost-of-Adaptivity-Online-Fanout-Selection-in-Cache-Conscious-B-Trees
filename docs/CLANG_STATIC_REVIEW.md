# Clang Static Review

Status: **IMPLEMENTED, NOT EXECUTED**.

| File | Issue | Fix | Semantic effect |
|---|---|---|---|
| `include/monitor/monitor.h` | Private `window_size_` was initialized but never used, which Clang can reject under `-Werror` | Removed the field; retained and explicitly ignored the legacy constructor argument | None: the value did not participate in monitoring before the change |
| `include/adaptive/segmented_adaptive_btree.h` | `unique_ptr` is now used directly | Added the required `<memory>` include rather than relying on a transitive include | None |
| root build | Compiler identity and warning policy were implicit in shell scripts | Added explicit GCC/Clang CMake presets with C++11 extensions disabled and strict warnings | Build configuration only |

No variable-length arrays or required post-C++11 language features were found
in the reviewed core path. Full Clang diagnostics remain
`RUNTIME_VALIDATION_REQUIRED`; the public failed CI annotation exposed only an
exit code, not the compiler diagnostic.
