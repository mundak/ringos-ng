#include <stdint.h>

int _fltused = 0;
uintptr_t __security_cookie = 0;

void __chkstk(void) { }

void __chkstk_ms(void) { }

void __security_check_cookie(uintptr_t cookie)
{
  (void) cookie;
}

void __security_init_cookie(void)
{
  if (__security_cookie == 0)
  {
    __security_cookie = (uintptr_t) 0xA5A5A5A5u;
  }
}
