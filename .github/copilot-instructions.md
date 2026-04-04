Before generating or modifying any code, read and follow the rules in [docs/contributing.md](../docs/contributing.md).

Before starting a fresh task, consider creating a separate git worktree, especially when there are existing changes in the main checkout directory. On Windows, create agent worktrees as visible sibling directories of the main checkout rather than under a hidden repo-local `.worktrees` folder so VS Code can recognize them. *ALWAYS* create a new worktree for new task, as existing ones might be used by other agents in the background.

Do not use VS Code editor-integrated tools for this workspace.

- Do not use CMake editor integration for configure, build, or test operations.
- Do not use editor diagnostics or problems integration to inspect errors.
- Prefer terminal commands and the repository's own scripts or Docker wrappers for build, test, and error inspection.
