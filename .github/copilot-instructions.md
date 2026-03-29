Before generating or modifying any code, read and follow the rules in [docs/contributing.md](../docs/contributing.md).

Any locally run agent must do its work in a dedicated git worktree. Do not make changes directly in the primary checkout. On Windows, create agent worktrees as visible sibling directories of the main checkout rather than under a hidden repo-local `.worktrees` folder so VS Code can recognize them. *ALWAYS* create a new worktree for new task, as existing ones might be used by other agents in the background.

When a formatting step requires `tools/clang-format.exe`, do not assume the `tools/` directory exists inside the dedicated worktree. If the worktree does not contain `tools/`, use the sibling primary checkout's `tools/clang-format.exe` instead, or fall back to `clang-format` on `PATH`.

Do not use VS Code editor-integrated tools for this workspace.

- Do not use CMake editor integration for configure, build, or test operations.
- Do not use editor diagnostics or problems integration to inspect errors.
- Prefer terminal commands and the repository's own scripts or Docker wrappers for build, test, and error inspection.
