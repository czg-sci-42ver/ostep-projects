#include <stdio.h>
#include "udp.h"
#include "../mfs.h"

// client code
int main(int argc, char *argv[]) {
  /*
  1. send to localhost:10000
  2. function as part of "MFS_Init".
  */
  int rc = MFS_Init("localhost", 10000);
  assert(rc != -1);

  rc = MFS_Lookup(ROOT_INUM, ".");
  if (rc == -1) {
    exit(EXIT_FAILURE);
  }
  MFS_Stat_t file_stat;
  rc = MFS_Stat(ROOT_INUM, &file_stat);
  if (rc == -1) {
    exit(EXIT_FAILURE);
  }
  char data_to_send[BSIZE] = {0};
  sprintf(data_to_send, "test first\n");
  rc = MFS_Write(2, data_to_send, 10);
  if (rc == -1) {
    exit(EXIT_FAILURE);
  }
  return 0;
}
