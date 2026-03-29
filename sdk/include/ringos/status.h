#pragma once

#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RINGOS_STATUS_OK ((int32_t) 0)
#define RINGOS_STATUS_INVALID_ARGUMENT ((int32_t) - 1)
#define RINGOS_STATUS_BAD_HANDLE ((int32_t) - 2)
#define RINGOS_STATUS_WRONG_TYPE ((int32_t) - 3)
#define RINGOS_STATUS_BUFFER_TOO_SMALL ((int32_t) - 4)
#define RINGOS_STATUS_PEER_CLOSED ((int32_t) - 5)
#define RINGOS_STATUS_WOULD_BLOCK ((int32_t) - 6)
#define RINGOS_STATUS_TIMED_OUT ((int32_t) - 7)
#define RINGOS_STATUS_NO_MEMORY ((int32_t) - 8)
#define RINGOS_STATUS_FAULT ((int32_t) - 9)
#define RINGOS_STATUS_NOT_SUPPORTED ((int32_t) - 10)
#define RINGOS_STATUS_BAD_STATE ((int32_t) - 11)
#define RINGOS_STATUS_NOT_FOUND ((int32_t) - 12)

#ifdef __cplusplus
}
#endif
