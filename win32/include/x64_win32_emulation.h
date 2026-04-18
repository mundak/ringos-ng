#pragma once

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "x64_pe64_image.h"

const x64_pe64_import_resolver* get_x64_win32_import_resolver();
