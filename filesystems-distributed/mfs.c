#include "./mfs.h"
#include "udp/udp.h"
static struct sockaddr_in addrSnd;
static int sd;
static char message[BUFFER_SIZE];
static struct sockaddr_in addrRcv;
int MFS_Init(char *hostname, int port) {
  sd = UDP_Open(20000);
  struct sockaddr_in *addr = &addrSnd;
  bzero(addr, sizeof(struct sockaddr_in));
  if (hostname == NULL) {
    return 0; // it's OK just to clear the address
  }

  addr->sin_family = AF_INET;   // host byte order
  addr->sin_port = htons(port); // short, network byte order

  struct in_addr *in_addr;
  struct hostent *host_entry;
  if ((host_entry = gethostbyname(hostname)) == NULL) {
    perror("gethostbyname");
    return -1;
  }
  in_addr = (struct in_addr *)host_entry->h_addr;
  addr->sin_addr = *in_addr;

  return 0;
}

int UDP_Read_Timeout(int socket_fd, struct sockaddr_in *addr, char *buffer,
                     int n) {
  /*
  1. also can use O_NONBLOCK https://stackoverflow.com/q/6715736/21294350
  2. https://stackoverflow.com/q/9798948/21294350
  */
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(socket_fd, &read_fds);
  struct timeval tv = {TIMEOUT_SECOND, 0};
  /*
  > the pselect() or select() function shall block until at least one of the
  requested  operations > becomes  ready
  */
  assert(select(socket_fd + 1, &read_fds, NULL, NULL, &tv) != -1);
  for (int fd = STDERR_FILENO + 1; fd < socket_fd + 1; fd++) {
    if (FD_ISSET(fd, &read_fds)) {
      if (socket_fd == fd) {
        printf("to read socket socket_fd\n");
      }
      UDP_Read(fd, &addrRcv, message, BUFFER_SIZE);
      return 0;
    }
  }
  return -1;
}

int MFS_Lookup(int pinum, char *name) {
  assert(pinum >= 0);
  sprintf(message, "Lookup,%d,%s", pinum, name);
  printf("message %s\n", message);
  UDP_Write(sd, &addrSnd, message, strlen(message) + 1);
  // while (UDP_Read_Timeout(sd, &addrRcv, message, BUFFER_SIZE)==-1) {
  //   UDP_Write(sd, &addrSnd, message, strlen(message)+1);
  // }
  UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
  return atoi(message);
}
int MFS_Write(int inum, char *buffer, int block) {}

void write_inode() {}