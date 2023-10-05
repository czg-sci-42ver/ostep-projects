#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE (4096)

typedef struct __MFS_Stat_t {
  int type; // MFS_DIRECTORY or MFS_REGULAR
  int size; // bytes
            // note: no permissions, access times, etc.
} MFS_Stat_t;

/*
1. See LFS chapter 8/16
inum -> inode -> "one or more data" -> each data has many "(name, inode number)"
2. align to BSIZE -> more easy to access
*/
typedef struct __MFS_DirEnt_t {
  char name[28]; // up to 28 bytes of name in directory (including \0)
  int inum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

/*
> assume each piece of the inode map has 16 entries

*/
typedef unsigned int uint;
#define MAP_ENTRY_SIZE 16
typedef struct __Inode_Map {
  uint inode_addr[MAP_ENTRY_SIZE];
} Inode_Map;
/*
TODO > the number of the last byte in the file

*/
typedef enum __File_Type {
  DIRECTORY = MFS_DIRECTORY,
  REGULAR_FILE = MFS_REGULAR_FILE
} File_Type;
#define DIRECT_POINTER_SIZE 14
typedef struct __Inode {
  uint size;
  File_Type type;
  uint data_ptr[DIRECT_POINTER_SIZE];
} Inode;
typedef struct __Checkpoint {
  uint log_end;
  uint imap_ptr;
} Checkpoint;

/*
similar to xcheck, i.e. xv6
> assume there are a maximum of 4096 inodes
5MB size
*/
#define FSSIZE (4096 * 5)
#define BSIZE 4096
#define IMG_SIZE (FSSIZE * BSIZE)
#define MAP_MAX_SIZE 10
#define ROOT_INUM 0

#define BUFFER_SIZE (1000)

#define TIMEOUT_SECOND 5

#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/select.h>

#endif // __MFS_h__
