#include <assert.h>
#define Ftruncate(fildes, length)                                              \
  do {                                                                         \
    assert(ftruncate(fildes, length) != -1);                                   \
  } while (0)
#define check_inum(inum)                                                       \
  do {                                                                         \
    assert(inum >= 0);                                                         \
  } while (0)
#define check_inum_exist(inum, inode_addr, map, imap)                          \
  do {                                                                         \
    memset(&map, 0, sizeof(map));                                              \
    inode_addr = get_inum_index(inum, &map, imap);                             \
    if (inode_addr == NULL) {                                                  \
      /*                                                                       \
      invalid inum                                                             \
      */                                                                       \
      return -1;                                                               \
    }                                                                          \
  } while (0)
/*
better use ptr.
(Inode*,void *,Inode_Addr *,ssize_t)
*/
#define write_block_offset(inode_inst, img_ptr, block_ptr, size)               \
  do {                                                                         \
    memmove(img_ptr + BSIZE * (block_ptr->block) + block_ptr->offset,          \
            inode_inst, size);                                                 \
  } while (0)
/*
(Inode,void *,Inode_Addr *,ssize_t)
*/
#define read_inode(inode_inst, img_ptr, block_ptr, size)                       \
  do {                                                                         \
    memmove(&inode_inst,                                                       \
            img_ptr + BSIZE * (block_ptr->block) + block_ptr->offset, size);   \
  } while (0)

#define check_inode_type(inode_inst, img_ptr, block_ptr, size, target_type)    \
  do {                                                                         \
    read_inode(inode_inst, img_ptr, block_ptr, size);                          \
    if (inode_inst.type != target_type) {                                      \
      /*                                                                       \
      invalid pinum                                                            \
      */                                                                       \
      return -1;                                                               \
    }                                                                          \
  } while (0)

/*
dst may be just one ptr, so not use sizeof()
*/
#define read_data(dst, img_ptr, block, offset, size)                           \
  memmove(dst, img_ptr + BSIZE * block + offset, size)
#define DIRECTORY_STR "DIRECTORY"
#define REGULAR_FILE_STR "REGULAR_FILE"
#define filetype_str(type) type == DIRECTORY ? DIRECTORY_STR : REGULAR_FILE_STR