#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE (4096)

#define MAX_INODE_NUM 4096
#define MAP_ENTRY_SIZE 16
#define MAP_MAX_SIZE MAX_INODE_NUM / MAP_ENTRY_SIZE

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
typedef struct __Inum_Index_Map {
  uint inum;
  uint index;
} Inum_Index_Map;
/*
1. here I add one map to make non-consecutive inodes stored consecutively.
(maybe also can be implemented by hash but this will maybe change inode_addr
structure)
2.
> The imap is a structure that takes an inode number
> as input and produces the disk address
so
*/
typedef struct __Block_Offset_Addr {
  uint block;
  uint offset;
} Block_Offset_Addr;
typedef struct __Inode_Map {
  Block_Offset_Addr inode_addr[MAP_ENTRY_SIZE]; // index -> addr
  Inum_Index_Map maps[MAP_ENTRY_SIZE];          // inum -> index
  // uint align[MAP_ENTRY_SIZE]; // to fit in BSIZE
} Inode_Map;
typedef struct __Map_Num_Entry_Map {
  uint map_end_index;
  /*
  1. -1 imply only inited but no entry.
  2. not use num which has always offset 1 with the index.
  3. this is entry index in one imap which points to the last used one.
  */
  int entry_index;
} Map_Num_Entry_Map;
typedef struct __Map_Num_Index_Map {
  uint map_num;
  /*
  1. -1 imply not known
  2. maps[index]=index should come into existence.
  it will also index inode_addr implicitly.
  */
  int index; // this is index in one Inode_Map->maps instead of based on all
             // Inode_Maps
} Map_Num_Index_Map;
/*
TODO > the number of the last byte in the file
*/
typedef enum __File_Type {
  DIRECTORY = MFS_DIRECTORY,
  REGULAR_FILE = MFS_REGULAR_FILE
} File_Type;

/*
specify how to modify one specific data block in the dir.
*/
typedef enum __UPDATE_PDIR { OVERWRITE = 0, APPEND = 1 } UPDATE_PDIR;

#define DIRECT_POINTER_SIZE 14
/*
> note that the inode looks as big as the
> data block, which generally isn’t the case; in most systems, data blocks
> are 4 KB in size, whereas an inode is much smaller, around 128 bytes

> Each inode should be simple: a size field (the number of the last byte in the
file), a type field (regular or directory), > and 14 direct pointers; thus, the
maximum file size is 14 times the 4KB block size, or 56 KB.
*/
typedef struct __Inode {
  uint size;
  File_Type type;
  uint data_ptr[DIRECT_POINTER_SIZE];
  /*
  keep aligned to Inode_Map, so any combination of them can fit in the block.
  */
  char align[sizeof(Inode_Map) - 64];
} Inode;
/*
here log_end points to the last used block, so log_end and imap_begin may
overlap.
*/
typedef struct __Checkpoint {
  Block_Offset_Addr log_end; // points to the current free block_addr
  Block_Offset_Addr imap_addr[MAP_MAX_SIZE]; // just imap stored addr
} Checkpoint;

/*
similar to xcheck, i.e. xv6
> assume there are a maximum of 4096 inodes
5MB size
*/
#define FSSIZE (4096 * 5)
#define BSIZE 4096
#define IMG_SIZE (FSSIZE * BSIZE)
/*
for simplicity, assume imap max num 10
*/
#define ROOT_INUM 0
#define READ_FAILURE_MSG "Read failure"
#define NAME_MAX_LEN 28

#define BUFFER_SIZE (1000)

#define TIMEOUT_SECOND 5
#define NEW_INODE_BEGIN 0

#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/select.h>

#endif // __MFS_h__
