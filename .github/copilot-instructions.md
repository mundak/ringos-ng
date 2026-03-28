Before generating or modifying any code, read and follow the rules in [docs/contributing.md](../docs/contributing.md).

Any locally run agent must do its work in a dedicated git worktree. Do not make changes directly in the primary checkout.

Do not use VS Code editor-integrated tools for this workspace.

- Do not use CMake editor integration for configure, build, or test operations.
- Do not use editor diagnostics or problems integration to inspect errors.
- Prefer terminal commands and the repository's own scripts or Docker wrappers for build, test, and error inspection.
