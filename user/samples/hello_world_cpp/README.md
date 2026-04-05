# Hello World C++ Sample

The sample exercises the hosted C++ path:

- C++20 translation units compiled with the bundled `clang++`
- upstream libc++ headers staged under the installed sysroot at `include/c++/v1`
- PE and COFF startup that runs `.CRT$X*` initializers before `main()` and `atexit()` handlers during `exit()`
- a currently validated header subset centered on libc++ core template headers such as `type_traits`
- the current constraints of `-fno-exceptions`, `-fno-rtti`, and `-fno-threadsafe-statics`
