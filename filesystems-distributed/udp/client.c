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

  char message[BUFFER_SIZE];
  sprintf(message, "hello world");

  rc = MFS_Lookup(ROOT_INUM, ".");
  if (rc < 0) {
    printf("client:: failed to send\n");
    exit(1);
  }

  printf("client:: wait for reply...\n");
  printf("client:: got reply [size:%d contents:(%s)\n", rc, message);
  return 0;
}
