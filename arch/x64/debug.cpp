void arch_debug_semihost_write(const char* message)
{
  (void) message;
}

void arch_debug_break()
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("int3");
#endif
}
