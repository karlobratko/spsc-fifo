#ifdef NDEBUG
#define SPSC_FIFO_NDEBUG
#endif

#ifdef CACHE_LINE_SIZE
#define SPSC_FIFO_CACHE_LINE_SIZE CACHE_LINE_SIZE
#endif

#define SPSC_FIFO_IMPLEMENTATION
#include "../spsc-fifo.h"