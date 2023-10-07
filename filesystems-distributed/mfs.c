#include "./mfs.h"
#include "./mfs_helper.h"
#include "udp/udp.h"
/*
UDP
*/
static struct sockaddr_in addrSnd;
static int sd;
static char message[BUFFER_SIZE];
static char read_buf[BSIZE];
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
      UDP_Read(fd, &addrRcv, buffer, BUFFER_SIZE);
      return 0;
    }
  }
  return -1;
}

void resend_if_fail(char *message, void *read_buf) {
  assert(message != read_buf);
  UDP_Write(sd, &addrSnd, message, strlen(message) + 1);
  while (UDP_Read_Timeout(sd, &addrRcv, read_buf, BUFFER_SIZE) == -1) {
    UDP_Write(sd, &addrSnd, message, strlen(message) + 1);
  }
}

int MFS_Lookup(int pinum, char *name) {
  check_inum(pinum);
  sprintf(message, "Lookup,%d,%s", pinum, name);
  resend_if_fail(message, read_buf);
  int rc = atoi(message);
  if (rc < 0) {
    printf("rc %d MFS_Lookup: error\n", rc);
  } else {
    printf("client:: got reply in MFS_Lookup [ret:%d contents:(%s)\n", 0,
           read_buf);
  }
  return rc;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
  check_inum(inum);
  sprintf(message, "Stat,%d", inum);
  resend_if_fail(message, read_buf);
  char *size_str, *type_str = read_buf;
  size_str = strsep(&type_str, ",");
  if (type_str != NULL) {
    m->size = atoi(size_str);
    m->type = strncmp(type_str, "REGULAR_FILE", strlen("REGULAR_FILE") + 1) == 0
                  ? REGULAR_FILE
                  : DIRECTORY;
    sprintf(message, "%d,%s", m->size, type_str);
    printf("client:: got reply in MFS_Stat [ret:%d contents:(%d,%s)\n", 0,
           m->size, type_str);
    return 0;
  } else {
    printf("MFS_Stat: error\n");
    return -1;
  }
}

int MFS_Write(int inum, char *buffer, int block) {
  check_inum(inum);
  sprintf(message, "Write,%d,%d", inum, block);
  resend_if_fail(message, read_buf);
  resend_if_fail(buffer, read_buf);
  int ret = atoi(read_buf);
  char func_str[20] = "MFS_Write";
  if (ret != -1) {
    printf("client:: got reply in %s [ret:%d contents:(%s)\n", func_str, 0,
           buffer);
    return 0;
  } else {
    printf("%s: error\n", func_str);
    return -1;
  }
}

int MFS_Read(int inum, char *buffer, int block) {
  check_inum(inum);
  char func_str[20] = "MFS_Read";
  sprintf(message, "Read,%d,%d", inum, block);
  resend_if_fail(message, buffer);
  if (strncmp(buffer, READ_FAILURE_MSG, strlen(READ_FAILURE_MSG)) != 0) {
    printf("client:: got reply in %s [ret:%d contents:(%s)\n", func_str, 0,
           buffer);
  }
  assert(UDP_Read_Timeout(sd, &addrRcv, buffer, BUFFER_SIZE) != -1);
  int ret = atoi(buffer);
  if (ret != -1) {
    return 0;
  } else {
    printf("%s: error\n", func_str);
    return -1;
  }
}

int MFS_Creat(int pinum, int type, char *name) {
  check_inum(pinum);
  sprintf(message, "Creat,%d,%s,%s", pinum, filetype_str(type), name);
  resend_if_fail(message, read_buf);
  int ret = atoi(read_buf);
  char func_str[20] = "MFS_Creat";
  if (ret != -1) {
    printf("client:: got reply in %s [ret:%d contents:(%s)\n", func_str, 0,
           read_buf);
    return 0;
  } else {
    printf("%s: error\n", func_str);
    return -1;
  }
}

void write_inode() {}