void arch_debug_break()
{
  asm volatile("brk #0");
}
