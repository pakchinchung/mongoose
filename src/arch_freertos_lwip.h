#pragma once

#if MG_ARCH == MG_ARCH_FREERTOS_LWIP

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
#include <sys/stat.h>
#include <sys/time.h>
#else
struct timeval {
  time_t tv_sec;
  long tv_usec;
};
#endif

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/sockets.h>

#if LWIP_SOCKET != 1
// Sockets support disabled in LWIP by default
#error Set LWIP_SOCKET variable to 1 (in lwipopts.h)
#endif

#if LWIP_POSIX_SOCKETS_IO_NAMES != 0
// LWIP_POSIX_SOCKETS_IO_NAMES must be disabled in posix-compatible OS
// enviroment (freertos mimics to one) otherwise names like `read` and `write`
// conflict
#error LWIP_POSIX_SOCKETS_IO_NAMES must be set to 0 (in lwipopts.h) for FreeRTOS
#endif

#define MG_INT64_FMT "%lld"
#define MG_DIRSEP '/'

// Re-route calloc/free to the FreeRTOS's functions, don't use stdlib
static inline void *mg_calloc(int cnt, size_t size) {
  void *p = pvPortMalloc(cnt * size);
  if (p != NULL) memset(p, 0, size);
  return p;
}
#define calloc(a, b) mg_calloc((a), (b))
#define free(a) vPortFree(a)
#define malloc(a) pvPortMalloc(a)
#define gmtime_r(a, b) gmtime(a)
#define mkdir(a, b) (-1)

#endif  // MG_ARCH == MG_ARCH_FREERTOS_LWIP
