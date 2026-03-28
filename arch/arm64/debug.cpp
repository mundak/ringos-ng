namespace
{

#if defined(__UINTPTR_TYPE__)
  using uintptr_type = __UINTPTR_TYPE__;
#else
  using uintptr_type = unsigned long long;
#endif

  constexpr uintptr_type SYS_WRITE0 = 0x04;

}

void arch_debug_semihost_write(const char* message)
{
#if defined(__GNUC__) || defined(__clang__)
  if (message == nullptr)
  {
    return;
  }

  register uintptr_type operation asm("x0") = SYS_WRITE0;
  register const char* parameter asm("x1") = message;
  asm volatile("hlt #0xF000" : "+r"(operation) : "r"(parameter) : "memory");
#else
  (void) message;
#endif
}

void arch_debug_break()
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("brk #0");
#endif
}
