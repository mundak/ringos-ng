#pragma once

#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
enum ringos_handle : uint64_t
#else
enum ringos_handle
#endif
{
  RINGOS_HANDLE_INVALID = 0,
};

#ifdef __cplusplus
}
#endif

