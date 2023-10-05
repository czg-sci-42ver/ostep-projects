#include <stdio.h>
#include "udp.h"
#include "../mfs.h"
#include "../mfs_helper.h"

#define BUFFER_SIZE (1000)
#define CMD_SIZE 10

uint block_index;
static Inode_Map imap[MAP_MAX_SIZE];
static Checkpoint checkpoint;
static uint imap_num = 0;
static void *mmap_file_ptr;

void *file_mmap(FILE *img_stream) {
  int fd = fileno(img_stream);
  if (fd == -1) {
    fprintf(stderr, "fileno failed\n");
    exit(EXIT_FAILURE);
  }
  struct stat stat_buf;
  assert(fstat(fd, &stat_buf) != -1);
  assert(stat_buf.st_size <= IMG_SIZE);
  /*
  borrow from xcheck_contest_3.c
  */
  Ftruncate(fd, IMG_SIZE);

  void *img_ptr = mmap(0, IMG_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert(img_ptr != NULL);
  assert(fclose(img_stream) != EOF);
  return img_ptr;
}

void *init_img(FILE *img_stream, Inode_Map *imap_ptr,
               Checkpoint *checkpoint_ptr) {
  void *img_ptr = file_mmap(img_stream);
  /*
  ftruncate will zero.
  */
  // memset(img_ptr, 0, IMG_SIZE);
  Checkpoint check_region;
  Inode_Map imap;
  memset(&imap, 0, sizeof(Inode_Map));
  Inode root_inode;
  memset(root_inode.data_ptr, 0, sizeof(root_inode.data_ptr));
  MFS_DirEnt_t dot = {".", ROOT_INUM};
  MFS_DirEnt_t dot_dot = {"..", ROOT_INUM};
  block_index = 1;
  root_inode.data_ptr[0] = block_index++;
  root_inode.size = sizeof(MFS_DirEnt_t) * 2;
  root_inode.type = DIRECTORY;
  imap.inode_addr[ROOT_INUM] = block_index++;
  check_region.log_end = block_index;
  check_region.imap_ptr = block_index;
  /*
  1. TODO how to generate this error manually.
  > The basic idea is to start with the last checkpoint region, find the end of
  > the log (which is included in the CR), and then use that to read through
  > the next segments
  2.
  */
  memmove(img_ptr, &check_region, sizeof(Checkpoint));
  memmove(img_ptr + BSIZE, &dot, sizeof(MFS_DirEnt_t));
  memmove(img_ptr + BSIZE + sizeof(MFS_DirEnt_t), &dot_dot,
          sizeof(MFS_DirEnt_t));
  assert(root_inode.size < BSIZE);
  memmove(img_ptr + BSIZE * 2, &root_inode, sizeof(Inode));
  memmove(img_ptr + BSIZE * 3, &imap, sizeof(Inode_Map));
  /*
  although the README doesn't request 2 Checkpoint, the chapter use 2.
  > LFS actually keeps two CRs, one at either end of the disk, and
  > writes to them alternately
  IMHO, this still can lose some data if log is not written.
  So I don't implement it here.
  */
  // memmove(img_ptr+IMG_SIZE-BSIZE, &check_region, sizeof(Checkpoint));
  if (msync(img_ptr, IMG_SIZE, MS_SYNC) == -1) {
    perror("Could not sync the file to disk");
  }
  *checkpoint_ptr = check_region;
  *imap_ptr = imap;
  return img_ptr;
}

/*
img_ptr is mmap returned ptr.
*/
void read_img(void *img_ptr, Inode_Map *imap_ptr, Checkpoint *checkpoint_ptr) {
  memmove(checkpoint_ptr, img_ptr, sizeof(Checkpoint));
  uint imap_begin = checkpoint_ptr->imap_ptr;
  uint imap_end = checkpoint_ptr->log_end;
  char *read_ptr = img_ptr + BSIZE * imap_begin;
  do {
    if (*read_ptr == 0) {
      break;
    }
    memmove(&imap_ptr[imap_num++], read_ptr, sizeof(Inode_Map));
    read_ptr += sizeof(Inode_Map);
  } while ((void *)read_ptr <= img_ptr + BSIZE * (imap_end + 1));
}

/*
since the files are all on the server, so the actual manipulation is done on the
server.
*/
int lookup(int pinum, char *name) {
  /*
  not delete for future usage maybe.
  */
  // for (int i=0; i<imap_num; i++) {
  //     for (int j=0; j<MAP_ENTRY_SIZE; j++) {
  //         if (imap[i].inode_addr[j]) {

  //         }
  //     }
  // }
  int pinode_addr =
      imap[pinum / MAP_ENTRY_SIZE].inode_addr[pinum % MAP_ENTRY_SIZE];
  if (pinode_addr == 0) {
    /*
    invalid pinum
    */
    return -1;
  }
  Inode dir_inode;
  memmove(&dir_inode, mmap_file_ptr + BSIZE * pinode_addr, sizeof(Inode));
  if (dir_inode.type != DIRECTORY) {
    /*
    invalid pinum
    */
    return -1;
  }
  MFS_DirEnt_t *dir_entry_list = calloc(BSIZE, 1), *tmp_entry;
  uint block_index = 0;
  uint offset = 0;
  int inum = -1;
  uint to_write = BSIZE;
  while (1) {
    if (block_index == DIRECT_POINTER_SIZE || to_write != BSIZE) {
      break;
    }
    offset = dir_inode.size - block_index * BSIZE;
    to_write = offset > BSIZE ? BSIZE : offset;
    memmove(dir_entry_list,
            mmap_file_ptr + BSIZE * (dir_inode.data_ptr[block_index++]),
            to_write);
    tmp_entry = dir_entry_list;
    while (*((char *)tmp_entry) != 0) {
      if (strncmp(tmp_entry->name, name, strlen(name) + 1) == 0) {
        return tmp_entry->inum;
      }
      tmp_entry += sizeof(MFS_DirEnt_t);
    }
  }
  return inum;
}

// server code
int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "argc error\n");
    exit(EXIT_FAILURE);
  }
  uint port_num = atoi(argv[1]);
  int sd = UDP_Open(port_num);
  FILE *img_stream;
  /*
  same as filesystems-checker
  I use mmap to copy the entire img because `MFS_Lookup`/`MFS_Stat`, etc.,
  will use more info than checkpoint and the inode map.
  */
  if ((img_stream = fopen(argv[2], "r+")) == NULL) {
    if (errno == ENOENT) {
      assert((img_stream = fopen(argv[2], "w+")) != NULL);
      /*
      here pass imap, etc ptr because at that time they are not global
      variables.
      */
      mmap_file_ptr = init_img(img_stream, imap, &checkpoint);
    }
  } else {
    mmap_file_ptr = file_mmap(img_stream);
    read_img(mmap_file_ptr, imap, &checkpoint);
  }
  assert(sd > -1);

  char *tmp_str;
  uint inum;
  int ret = 0;
  while (1) {
    struct sockaddr_in addr;
    char message[BUFFER_SIZE];
    printf("server:: waiting...\n");
    UDP_Read(sd, &addr, message, BUFFER_SIZE);
    char *sep_ptr = message;
    char reply[BUFFER_SIZE];
    /*
    cmd
    */
    tmp_str = strsep(&sep_ptr, ",");
    /*
    char can use switch, str just if-else
    after all is using jmp instruction.
    https://stackoverflow.com/a/4014900/21294350
    */
    if (strncmp(tmp_str, "Lookup", strlen("Lookup")) == 0) {
      tmp_str = strsep(&sep_ptr, ",");
      inum = atoi(tmp_str);
      ret = lookup(inum, sep_ptr);
      sprintf(reply, "%d", ret);
      UDP_Write(sd, &addr, reply, BUFFER_SIZE);
    }
  }
  if (munmap(mmap_file_ptr, IMG_SIZE) == -1) {
    fprintf(stderr, "munmap failed\n");
    // exit(EXIT_FAILURE);
  }
  return 0;
}
