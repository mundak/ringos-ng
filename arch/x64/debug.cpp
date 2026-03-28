void arch_debug_break()
{
  asm volatile("int3");
}
