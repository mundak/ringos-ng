# Contributing to this project

## Language & Standard

* **C++20** (`-std=c++20`). Use modern features (structured bindings, concepts,
  `std::format`, `constexpr`, etc.) where they improve clarity.
* Compile with `-Wall -Wextra -Wpedantic` (GCC/Clang) or `/W4` (MSVC).
  **Zero warnings policy.**

---

## Post-Edit Formatting

After every source file modification (`.cpp`, `.h`, `.inl`):

1. **Run the `coding-style` agent** (`.github/agents/coding-style.md`) on all
   changed files to catch violations that clang-format cannot detect (naming,
   type usage, `auto` misuse, logging, etc.). Fix every reported issue before
   proceeding.
2. Run `tools/clang-format.exe -i <file>` on Windows, or `clang-format -i
  <file>` when the formatter is already on your `PATH`, on the entire modified
  file so it conforms to the project's `.clang-format` configuration.

---

## Formatting Overview

The style is based on **WebKit** with significant customizations toward an
**Allman/BSD** brace style.

* **Indent width:** 2 spaces (no tabs)
* **Continuation indent:** 2 spaces
* **Column limit:** 120 characters
* **Access modifier offset:** -2 (aligned with the class keyword)
* Source files must end with a blank line.

---

## Braces (Allman Style)

Braces go on their **own line** for all constructs: classes, structs, unions,
enums, functions, control statements (`if`, `for`, `while`, `switch`),
namespaces, lambdas, `catch`, and `else`.

**Always use braces**, even for single-line bodies. Never omit braces for `if`,
`else`, `for`, `while`, or `do` statements.

Do **not** add an extra empty line immediately inside namespace braces. The
first declaration follows the opening brace directly, and the closing brace
follows the last declaration directly.

```cpp
// Good
if (condition)
{
  do_something();
}
else
{
  do_other();
}

// Bad — never omit braces
if (condition)
  do_something();
```

---

## Functions

* **All function definitions** must live in `.cpp` files. Headers contain only
  declarations.
* Return type stays on the same line as the function name.

```cpp
// header (.h) — declaration only
class app
{
public:
  int32_t count() const;
  void init();
};

// source (.cpp) — all definitions
int32_t app::count() const
{
  return m_count;
}
```

### Function Arguments & Parameters

Arguments and parameters are **not bin-packed** — each goes on its own line
when they don't fit on one line (`AlignAfterOpenBracket: AlwaysBreak`).

```cpp
void some_function(
  int32_t first_param,
  int32_t second_param,
  const std::string& third_param)
{
  another_call(
    value_a,
    value_b,
    value_c);
}
```

### Constructor Initializer Lists

Break **before** the comma. Each initializer on its own line.

```cpp
my_class::my_class()
  : m_member_a(0)
  , m_member_b(nullptr)
  , m_member_c("default")
{
}
```

---

## Templates

* Template declarations always break before the templated entity.
* No space after the `template` keyword before `<`.
* **Template function definitions** must live in a separate `.inl` file,
  included at the end of the corresponding `.h` file.
* Type template parameter names use `snake_case` and end with `_t`.
* Non-type template parameter names use plain `snake_case`.

```cpp
// container.h
#pragma once

template<typename value_t>
class container
{
public:
  void add(const value_t& value);
  value_t get(int32_t index) const;
};

#include "container.inl"
```

```cpp
// container.inl
template<typename value_t>
void container<value_t>::add(const value_t& value)
{
  // ...
}

template<typename value_t>
value_t container<value_t>::get(int32_t index) const
{
  // ...
}
```

---

## Naming Conventions

Follow the **C++ Standard Library naming convention** (`snake_case` everywhere).

| Entity | Style | Example |
|--------|-------|---------|
| Files & directories | `snake_case` | `main_loop.cpp`, `editor_panel.h` |
| Namespaces | `snake_case` | `mai`, `math` |
| Classes / structs | `snake_case` | `app`, `vulkan_context` |
| Functions / methods | `snake_case` | `poll_events()`, `render_frame()` |
| Local variables | `snake_case` | `window_width`, `graphics_family` |
| Public struct members | `snake_case` | `arch_id`, `user_base` |
| Template type parameters | `snake_case` ending in `_t` | `value_t`, `process_t` |
| Non-type template parameters | `snake_case` | `capacity`, `index_bits` |
| Getter / accessor methods | `get_`, `is_`, `has_` prefixes | `get_handle()`, `is_ready()`, `has_messages()` |
| Private class members | `m_` prefix | `m_window`, `m_vk_ctx` |
| Constants / macros | `UPPER_SNAKE_CASE` | `VK_NULL_HANDLE`, `MAI_DEBUG` |

Use `m_` only for private class members. Public struct fields, including ABI or
plain-data carrier types, use unprefixed `snake_case`.

Getter-style accessors should use a `get_` prefix. Boolean accessors should use
`is_` or `has_` as appropriate. Do not name getters with a `_value` suffix.

---

## Type Usage

* **Prefer explicit, fixed-width types** over implicit types: `int32_t` over
  `int`, `uint32_t` over `unsigned int`, `int64_t` over `long long`, etc.
* Use standard fixed-width integer names directly: `uint8_t`, `uint16_t`,
  `uint32_t`, `uint64_t`, `intptr_t`, `uintptr_t`, etc.
* Do **not** introduce custom aliases such as `uint16_type`, `u32`, or
  compiler builtin wrappers for standard fixed-width integers.
* Use `<cstdint>` for hosted code and `<stdint.h>` for freestanding code when
  the C++ wrapper is unavailable in the target toolchain.
* Use `float`, `double`, `bool`, `char`, and `size_t` as-is where semantically
  appropriate.

---

## `auto` Usage

* **Do not use `auto`** for simple or obvious types. Write the type explicitly.
* **Use `auto` only when** the type is a complex iterator or a heavily templated
  expression where spelling it out reduces readability.
* When in doubt, spell out the type.

```cpp
// Good
int32_t count = 0;
std::string name = get_name();

// Good — complex iterator
auto it = my_map.find(key);

// Bad — do not use auto for simple types
auto count = 0;
auto name = get_name();
```

---

## Pointers & References

Pointer and reference markers bind to the **left** (to the type).

```cpp
int32_t* ptr = nullptr;
const std::string& name = get_name();
```

---

## Static Functions

**Do not use `static` for file-local free functions.** Use an anonymous
`namespace { }` instead.

```cpp
// Good — anonymous namespace
namespace {
  void helper() { }
}

// Bad — static free function
static void helper() { }
```

---

## File Layout

Each module lives in its own subdirectory under `src/`:

```
src/
├── main.cpp           # entry point
├── app/               # application shell (window, imgui, loop, layout)
├── panels/            # UI panels (editor, assembly, console, debugger)
├── compiler/          # lexer, parser, AST, codegen (future)
└── debugger/          # GDB stub client (future)
```

Headers use `.h`, source files use `.cpp`, template definitions use `.inl`.

---

## Header Guards

Use `#pragma once` (supported by all target compilers).

---

## Namespaces

* All namespace content is **indented**.
* **Do not** use closing comments on namespace braces (no `// namespace foo`).
* Namespaces should only be used for **grouping related functions from the same
  area** (e.g., utility modules, subsystem boundaries). Do not wrap entire
  applications or single classes in namespaces unnecessarily.

```cpp
namespace math {

  float lerp(float a, float b, float t);
  float clamp(float value, float min, float max);

}
```

---

## Includes

* Includes are **sorted** alphabetically.
* Include blocks are **regrouped** automatically (system vs. project headers
  separated).

Order:

1. Corresponding header (for `.cpp` files).
2. Project headers (alphabetical).
3. Third-party headers (`imgui.h`, `GLFW/glfw3.h`, etc.).
4. Standard library headers.

Separate each group with a blank line.

---

## Spacing Rules

* **Space after C-style cast:** `(int32_t) value`
* **Space before control-statement parentheses:** `if (`, `for (`, `while (`
* **No space before function call parentheses:** `foo()`, `bar(x)`
* **Space before C++11 braced-init lists:** `vec {1, 2, 3}`
* **No extra spaces inside container literals:** `{1, 2, 3}` not `{ 1, 2, 3 }`

---

## Blank Lines

* Maximum **1** consecutive empty line.
* **Separate definition blocks** with a blank line (between function
  definitions, class definitions, etc.).

---

## Preprocessor Directives

Preprocessor directives are **not indented**.

```cpp
#pragma once

#include <vector>

#ifdef MAI_DEBUG
#define MAI_TRACE(x) spdlog::trace(x)
#endif
```

## Agent Workflow

AI agents picking up tasks should:

1. Read the task description in `docs/tasks/m<N>-tasks.md`.
2. Check that all prerequisite tasks are merged or present on the branch.
3. Create a feature branch: `agent/<task-id>-<short-name>`
   (e.g. `agent/t0.1-cmake`).
4. Implement the deliverables.
5. Write (or update) the suggested tests.
6. Run `cmake -B build && cmake --build build && ctest --test-dir build`
   locally.
7. **Run the `coding-style` agent** (`.github/agents/coding-style.md`) on all
   changed or created source files. Fix every reported violation before
   proceeding.
8. **Run `clang-format -i`** on every modified source file.
9. **Update `docs/tasks/progress.md`** — mark the task as `done` and add the
   completion date. Commit this change with the task deliverables.
10. Commit with a descriptive message referencing the task ID.
11. **Rebase onto the latest changes** from the target branch and resolve any
  conflicts. **Never create merge commits.** Re-run the full build and test
  suite to confirm nothing is broken after the rebase.
12. Open a PR targeting `main` unless a different integration branch is
  explicitly requested.
13. **Add the PR number** to your row in `docs/tasks/progress.md` and push
    the update.
