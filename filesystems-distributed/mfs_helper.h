#include <assert.h>
#define Ftruncate(fildes, length)                                              \
  do {                                                                         \
    assert(ftruncate(fildes, length) != -1);                                   \
  } while (0);
