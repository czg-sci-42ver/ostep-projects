#include "fcntl.h"
#include "types.h"
#include "user.h"

int main() {
  int count = getreadcount();
  printf(1, "Before calling read(): %d\n", count);
  int fd = open("README", O_RDONLY);
  char buf[69];
  read(fd, buf, 4);
  printf(1, "read: %s\n", buf);
  count = getreadcount();
  printf(1, "After calling read(): %d\n", count);
  close(fd);
  exit();
}
