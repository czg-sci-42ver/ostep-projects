#include <stdio.h>
#include "udp.h"
#include "server.h"
/*
UDP
*/
#define BUFFER_SIZE (1000)
static int sd;
static struct sockaddr_in addr;
static char reply[BUFFER_SIZE];
/*
LFS
*/
#define CMD_SIZE 10
/*
https://stackoverflow.com/a/4146406/21294350
*/
static Inode_Map imap_gbl[MAP_MAX_SIZE];
static Checkpoint checkpoint;
static Map_Num_Entry_Map imap_global_index;
static void *mmap_file_ptr;

/*
these are temporary variable which doesn't needs from main to its called
functions.
1. imaps_index_map is to record the current imap access index.
so most of time no need to pass its state across functions.
*/
static Map_Num_Index_Map imaps_index_map;
static Inode inode_inst;
static Block_Offset_Addr *tmp_inode_addr;

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

  /*
  here MAP_PRIVATE is no use.
  > When the msync() function is called on MAP_PRIVATE mappings, any modified
  data shall not be writ ...
  */
  void *img_ptr = mmap(0, IMG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  assert(img_ptr != NULL);
  assert(fclose(img_stream) != EOF);
  return img_ptr;
}

/*
similar to search_dir
*/
uint search_invalid_dir_entry(Inode *pinode_ptr, int *invalid_cnt) {
  uint offset_loc = 0, ptr_index = 0;
  MFS_DirEnt_t dir_entry;
  uint invalid_cnt_loc = *invalid_cnt;
  uint check_cnt = 0;
  if (invalid_cnt_loc == 0) {
    check_cnt = 1;
  }
  while (offset_loc != pinode_ptr->size) {
    /*
    this branch has some overheads if CPU branch predicter is not good
    it can be removed by changing the `offset_loc!=inode_inst.size` condition.
    */
    if (offset_loc % BSIZE == 0 && offset_loc != 0) {
      ptr_index++;
    }
    read_data(&dir_entry, mmap_file_ptr, pinode_ptr->data_ptr[ptr_index],
              offset_loc % BSIZE, sizeof(MFS_DirEnt_t));
    if (dir_entry.inum == -1) {
      if (check_cnt) {
        invalid_cnt_loc++;
      } else {
        return offset_loc;
      }
    }
    offset_loc += sizeof(MFS_DirEnt_t);
  }
  *invalid_cnt = invalid_cnt_loc;
  return -1;
}

/*
1. must call after create_dir, so size will not be 0.
2. based on chapter 8/16, although the whole changed dir contents are written to
the new blocks. but it is based on the assumption that the data uses the whole
block. After taking that keeping Inode consistent in account, I move the whole
data block to use `data_ptr` in Inode.
*/
UPDATE_PDIR append_dir_contents(MFS_DirEnt_t *dir_entry, Inode *pinode_ptr,
                                void *img_ptr, Checkpoint *ck_ptr) {
  Block_Offset_Addr tmp_iaddr;
  int invalid_cnt = -1;
  uint target_offset = search_invalid_dir_entry(pinode_ptr, &invalid_cnt);
  /*
  here for simplicity, all inits are not dependent on the `reuse_offset`.
  */
  if (target_offset == -1) {
    target_offset = pinode_ptr->size;
  }
  uint single_imap_offset = target_offset % BSIZE;
  uint end_block_index = target_offset / BSIZE;
  UPDATE_PDIR ret = OVERWRITE;
  if (ck_ptr->log_end.offset != 0) {
    ck_ptr->log_end.block++; // use one whole new block for the data.
  };
  uint old_end_block = pinode_ptr->data_ptr[end_block_index];
  /*
  pinode use the new block.
  */
  if (single_imap_offset == 0 && target_offset == -1) {
    pinode_ptr->data_ptr[++end_block_index] = ck_ptr->log_end.block;
    ret = APPEND;
  } else {
    pinode_ptr->data_ptr[end_block_index] = ck_ptr->log_end.block;
  }
  /*
  mark the current block has been used and update the offset to point to that.
  */
  ck_ptr->log_end.block++;
  ck_ptr->log_end.offset = 0;
  assert(end_block_index <= DIRECT_POINTER_SIZE);
  tmp_iaddr.offset = 0;
  tmp_iaddr.block = pinode_ptr->data_ptr[end_block_index];
  /*
  although we access the data block, here we use Inode_Addr to set the block and
  offset to access.
  */
  Block_Offset_Addr *tmp_ptr = &tmp_iaddr;
  if (ret == OVERWRITE) {
    /*
    move the old block to the new one.
    */
    write_block_offset(mmap_file_ptr + BSIZE * old_end_block, mmap_file_ptr,
                       tmp_ptr, BSIZE);
  }
  tmp_iaddr.offset = single_imap_offset;
  pinode_ptr->size += sizeof(MFS_DirEnt_t);
  write_block_offset(dir_entry, mmap_file_ptr, tmp_ptr, sizeof(MFS_DirEnt_t));
  return ret;
}

void create_file(Inode *file_inode) {
  memset(file_inode, 0, sizeof(Inode));
  memset(file_inode->align, 0, sizeof(file_inode->align));
  memset(file_inode->data_ptr, 0, sizeof(file_inode->data_ptr));
  file_inode->size = 0;
  file_inode->type = REGULAR_FILE;
  /*
  See create_dir comments.
  */
}

void create_file_update_pdir(int finum, void *img_ptr, char *dir_name,
                             Inode *pinode_ptr, Checkpoint *ck_ptr,
                             Inode *inode_inst) {
  create_file(inode_inst);
  MFS_DirEnt_t dir_entry;
  dir_entry.inum = finum;
  strncpy(dir_entry.name, dir_name, sizeof(dir_entry.name));
  append_dir_contents(&dir_entry, pinode_ptr, img_ptr, ck_ptr);
}

/*
1. here use the memory and cache to amortize the frequent access to the random
memory addr.
2. this func will move data to the disk.
*/
void create_dir_update_pdir(int pinum, int dinum, void *img_ptr, char *dir_name,
                            Inode *pinode_ptr, Checkpoint *ck_ptr,
                            Map_Num_Entry_Map *num_entry_map,
                            Inode *inode_inst) {
  /*
  assume consecutive write so use `ck_ptr->log_end`
  */
  create_dir(pinum, dinum, img_ptr, inode_inst, num_entry_map, ck_ptr);

  MFS_DirEnt_t dir_entry;
  dir_entry.inum = dinum;
  strncpy(dir_entry.name, dir_name, sizeof(dir_entry.name));
  append_dir_contents(&dir_entry, pinode_ptr, img_ptr, ck_ptr);
}

void create_dir(int pinum, int dinum, void *img_ptr, Inode *dir_inode,
                Map_Num_Entry_Map *num_entry_map, Checkpoint *ck_ptr) {
  memset(dir_inode, 0, sizeof(Inode));
  memset(dir_inode->align, 0, sizeof(dir_inode->align));
  memset(dir_inode->data_ptr, 0, sizeof(dir_inode->data_ptr));
  MFS_DirEnt_t dot = {".", dinum};
  MFS_DirEnt_t dot_dot = {"..", pinum};
  /*
  always one whole new data block
  notice only init ck_ptr->log_end.offset may be 0
  because the inodes and imaps are always written at last so offset should not
  be 0.
  */
  if (ck_ptr->log_end.offset != 0) {
    ck_ptr->log_end.block++;
    ck_ptr->log_end.offset = 0;
  }
  uint tmp_data_block = ck_ptr->log_end.block;
  dir_inode->data_ptr[0] = ck_ptr->log_end.block++; // mark used
  dir_inode->size = sizeof(MFS_DirEnt_t) * 2;
  dir_inode->type = DIRECTORY;
  memmove(img_ptr + BSIZE * tmp_data_block, &dot, sizeof(MFS_DirEnt_t));
  memmove(img_ptr + BSIZE * tmp_data_block + sizeof(MFS_DirEnt_t), &dot_dot,
          sizeof(MFS_DirEnt_t));
  /*
  with `update_dir` for the parent dir, maybe one new data block for the dir
  will be created, so let the caller decide where to write the inode and then
  imap.
  */
}

/*
will update num_entry_map implicitly.
*/
void add_imap_to_ck(Checkpoint *ck_ptr, Map_Num_Entry_Map *num_entry_map) {
  ck_ptr->imap_addr[++num_entry_map->map_end_index].block =
      ck_ptr->log_end.block;
  ck_ptr->imap_addr[num_entry_map->map_end_index].offset =
      ck_ptr->log_end.offset;
  ck_ptr->log_end.offset += sizeof(Inode_Map);
}
void modify_imap_in_ck(Checkpoint *ck_ptr, uint target_map_index) {
  ck_ptr->imap_addr[target_map_index].block = ck_ptr->log_end.block;
  ck_ptr->imap_addr[target_map_index].offset = ck_ptr->log_end.offset;
  ck_ptr->log_end.offset += sizeof(Inode_Map);
}

/*
array pointer
https://stackoverflow.com/a/20570097/21294350
*/
void update_entry_in_imap(Inode_Map (*imap_ptr)[MAP_MAX_SIZE], uint dinum,
                          Map_Num_Entry_Map *num_entry_map, uint block,
                          uint offset) {
  Inode_Map *single_map_ptr =
      array_ptr_to_elem_ptr(imap_ptr, num_entry_map->map_end_index);
  uint entry_index;
  entry_index = ++num_entry_map->entry_index;
  single_map_ptr->maps[entry_index].inum = dinum;
  single_map_ptr->maps[entry_index].index = entry_index;
  single_map_ptr->inode_addr[entry_index].block = block;
  single_map_ptr->inode_addr[entry_index].offset = offset;
}

void update_entry_in_pinum_imap(Map_Num_Index_Map *map,
                                Inode_Map (*imap_ptr)[MAP_MAX_SIZE], uint dinum,
                                uint block, uint offset) {
  Inode_Map *single_map_ptr = array_ptr_to_elem_ptr(imap_ptr, map->map_num);
  uint entry_num = map->index;
  single_map_ptr->maps[entry_num].inum = dinum;
  single_map_ptr->maps[entry_num].index = entry_num;
  single_map_ptr->inode_addr[entry_num].block = block;
  single_map_ptr->inode_addr[entry_num].offset = offset;
}

/*
1. write the imap to the disk in `update_checkpoint_based_on_map_num`
2. See "maybe one child inode , one parent dir", so inode may be multiple.
3. although `check_inum_exist` calls `get_inum_index` to get pinum related imap
addr, here we don't depend on it to make functions less dependent.
4. inum and inode_inst_list should correspond to each other.
  assume the (dinum,pinum) order.
5. here the inodes are written first, then imap.
6. this will update the Checkpoint.
*/
void add_entry_to_imap(Map_Num_Entry_Map *num_entry_map,
                       Inode_Map (*imap_ptr)[MAP_MAX_SIZE], int *inum,
                       Checkpoint *ck_ptr, void *img_ptr,
                       Inode **inode_inst_list, uint inode_num) {
  assert(ck_ptr->log_end.offset == 0); // because after data blocks, the inodes
                                       // and imaps should be in one new block.
  assert(inode_num == 2 || inode_num == 1);
  Map_Num_Index_Map tmp_map;
  if (inode_num == 2) {
    get_inum_index(inum[1], &tmp_map, imap_ptr);
    assert(tmp_map.index != -1 && tmp_map.map_num != -1);
  }
  for (int i = 0; i < inode_num; i++) {
    move_inode_update_ck(inode_inst_list[i], img_ptr, ck_ptr);
  }
  uint index = num_entry_map->entry_index;
  uint tmp_offset = sizeof(Inode) * inode_num;
  assert(ck_ptr->log_end.offset == tmp_offset);
  if (index == MAP_ENTRY_SIZE - 1) {
    /*
    1. since always write the inode and imap to the new block, so the offset
    should not be BSIZE. the internal fragmentation is solved by the cleaner
    later.
    2. here not care about the order between the new imap and the pinum_imap.
    */
    add_imap_to_ck(ck_ptr, num_entry_map);
    num_entry_map->entry_index = 0;
  }
  /*
  1. although maybe 2 inodes, since one inode is pinode which is modified
  instead of created, so pinode will modify instead of add one entry.
  2. pinum_imap later same as the order of inum.
    when init_img where only one imap, the dinum equals pinum, so their imaps
  are also equal.
  */
  if (inode_num == 2) {
    modify_imap_in_ck(ck_ptr, tmp_map.map_num);
  } else {
    /*
    assume used in init_img.
    */
    modify_imap_in_ck(ck_ptr, 0);
  }
  /*
  here write the imaps with inode addrs.
  same as inode write order
  */
  update_entry_in_imap(imap_ptr, inum[0], num_entry_map, ck_ptr->log_end.block,
                       0);
  if (inode_num == 2) {
    /*
    see above "pinum_imap first"
    */
    update_entry_in_pinum_imap(&tmp_map, imap_ptr, inum[1],
                               ck_ptr->log_end.block, sizeof(Inode));
  }
}

/*
imap should point to one single imap.
*/
void static inline memmove_imap(Checkpoint *check_region, uint imap_index,
                                void *img_ptr, Inode_Map *imap) {
  memmove(img_ptr + BSIZE * (check_region->imap_addr[imap_index].block) +
              check_region->imap_addr[imap_index].offset,
          imap, sizeof(Inode_Map));
}

/*
1. assume inode map always stored last.
2. This just write, the related modification of ck_ptr offset should be done by
the caller.
*/
void write_checkpoint_and_EndImap(Checkpoint *ck_ptr,
                                  Map_Num_Entry_Map *num_entry_map,
                                  void *img_ptr,
                                  Inode_Map (*imap_ptr)[MAP_MAX_SIZE]) {
  /*
  maybe one child inode , one parent dir inode and one imap.
  */
  uint target_map = num_entry_map->map_end_index;
  // ck_ptr->imap_addr[target_map].block = end_block_index;
  // ck_ptr->imap_addr[target_map].offset = sizeof(Inode);
  memmove(img_ptr, ck_ptr, sizeof(Checkpoint));
  Inode_Map *imap_addr = array_ptr_to_elem_ptr(imap_ptr, 0);
  memmove_imap(&checkpoint, target_map, img_ptr, imap_addr + target_map);
}

void static inline init_checkpoint(Checkpoint *checkpoint_ptr) {
  checkpoint_ptr->log_end.block = 1;
  checkpoint_ptr->log_end.offset =
      0; // explicit assignment although static will init 0
}

void static inline sync_file(void *img_ptr, uint len) {
  if (msync(img_ptr, len, MS_SYNC) == -1) {
    perror("Could not sync the file to disk");
  }
}

void static inline init_num_entry_map(Map_Num_Entry_Map *num_entry_map) {
  num_entry_map->entry_index = -1;
}

void *init_img(FILE *img_stream, Inode_Map (*imap_ptr)[MAP_MAX_SIZE],
               Checkpoint *checkpoint_ptr, Map_Num_Entry_Map *num_entry_map) {
  void *img_ptr = file_mmap(img_stream);
  /*
  ftruncate will zero.
  */
  // memset(img_ptr, 0, IMG_SIZE);
  Inode dir_inode;
  init_checkpoint(checkpoint_ptr);
  init_num_entry_map(num_entry_map);
  create_dir(ROOT_INUM, ROOT_INUM, img_ptr, &dir_inode, &imap_global_index,
             checkpoint_ptr);
  int inum_list[1] = {ROOT_INUM};
  Inode *inode_list[1] = {&dir_inode};
  add_entry_to_imap(&imap_global_index, imap_ptr, inum_list, checkpoint_ptr,
                    img_ptr, &(inode_list[0]), 1);

  write_checkpoint_and_EndImap(checkpoint_ptr, &imap_global_index, img_ptr,
                               imap_ptr);
  /*
  1. TODO how to generate this error manually.
  > The basic idea is to start with the last checkpoint region, find the end of
  > the log (which is included in the CR), and then use that to read through
  > the next segments
  2.
  */

  /*
  although the README doesn't request 2 Checkpoint, the chapter use 2.
  > LFS actually keeps two CRs, one at either end of the disk, and
  > writes to them alternately
  IMHO, this still can lose some data if log is not written.
  So I don't implement it here.
  */
  // memmove(img_ptr+IMG_SIZE-BSIZE, &check_region, sizeof(Checkpoint));
  sync_file(img_ptr, IMG_SIZE);
  return img_ptr;
}

/*
img_ptr is mmap returned ptr.
*/
void read_img(void *img_ptr, Inode_Map (*imap_ptr)[MAP_MAX_SIZE],
              Checkpoint *checkpoint_ptr) {
  memmove(checkpoint_ptr, img_ptr, sizeof(Checkpoint));
  Block_Offset_Addr imap_end = checkpoint_ptr->log_end;
  uint global_offset;
  char *read_ptr = NULL;
  while (1) {
    global_offset =
        BSIZE *
            checkpoint_ptr->imap_addr[imap_global_index.map_end_index].block +
        checkpoint_ptr->imap_addr[imap_global_index.map_end_index].offset;
    if (global_offset == 0) {
      break;
    }
    read_ptr = img_ptr + global_offset;
    assert((void *)read_ptr <=
           img_ptr + BSIZE * imap_end.block + imap_end.offset);
    memmove(array_ptr_to_elem_ptr(imap_ptr, imap_global_index.map_end_index++),
            read_ptr, sizeof(Inode_Map));
  }
  imap_global_index.map_end_index--; // because of using the index
  /*
  > and a new piece of the inode map to the end of the log
  assume inode map stored last.
  */
  imap_global_index.entry_index = imap_end.offset / sizeof(Inode_Map) - 1;
}

/*
1. since the files are all on the server, so the actual manipulation is done on
the server. See nfs chapter 8/18 where server send data and the client may store
in the cache in 11/18. So here server -> NFS and LFS.
2. inum -> inode_addr and index to get the imap related entry.
*/
Block_Offset_Addr *get_inum_index(uint inum, Map_Num_Index_Map *map,
                                  Inode_Map (*imap_ptr)[MAP_MAX_SIZE]) {
  Inode_Map *imap = array_ptr_to_elem_ptr(imap_ptr, 0);
  for (int i = 0; i <= imap_global_index.map_end_index; i++) {
    for (int j = 0; j < MAP_ENTRY_SIZE; j++) {
      if (imap[i].maps[j].inum == inum) {
        map->map_num = i;
        map->index = j;
        return &imap[i].inode_addr[imap[i].maps[j].index];
      }
    }
  }
  map->map_num = -1;
  map->index = -1;
  return NULL; // means not found related inode block addr.
}

int lookup_lfs(int pinum, char *name, Inode_Map (*imap)[MAP_MAX_SIZE]) {
  /*
  > The inode map is just an array, indexed by inode number.
  */
  Block_Offset_Addr *pinode_addr_ptr;
  check_inum_exist(pinum, pinode_addr_ptr, imaps_index_map, imap);
  Inode dir_inode;
  check_inode_type(dir_inode, mmap_file_ptr, pinode_addr_ptr, sizeof(Inode),
                   DIRECTORY);
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
      tmp_entry += 1;
    }
  }
  return inum;
}
/*
let the compiler decides whether to inline
https://stackoverflow.com/a/61534108/21294350
*/
void static inline memmove_inode_block(void *inode_ptr, void *img_ptr_src,
                                       void *img_ptr_dst,
                                       Block_Offset_Addr *inode_block) {
  if (img_ptr_src != NULL) {
    memmove(inode_ptr,
            img_ptr_src + BSIZE * (inode_block->block) + inode_block->offset,
            sizeof(Inode));
  } else if (img_ptr_dst != NULL) {
    memmove(img_ptr_dst + BSIZE * (inode_block->block) + inode_block->offset,
            inode_ptr, sizeof(Inode));
  } else {
    printf("wrong src/dst\n");
  }
}

int stat_lfs(int inum, MFS_Stat_t *m, Inode_Map (*imap)[MAP_MAX_SIZE]) {
  Block_Offset_Addr *inode_addr_ptr;
  check_inum_exist(inum, inode_addr_ptr, imaps_index_map, imap);
  Inode file_inode;
  /*
  here not use typedef because mmap_file_ptr, BSIZE, etc, are all dependent on
  the caller naming convention.
  */
  memmove_inode_block(&file_inode, mmap_file_ptr, NULL, inode_addr_ptr);
  m->size = file_inode.size;
  m->type = file_inode.type;
  return 0;
}

void modify_ck(Checkpoint *ck_ptr, Map_Num_Index_Map *num_index_map,
               uint log_end_offset, uint imap_offset, uint imap_block) {
  ck_ptr->log_end.block = imap_block;
  ck_ptr->log_end.offset = log_end_offset;
  ck_ptr->imap_addr[num_index_map->map_num].block = imap_block;
  ck_ptr->imap_addr[num_index_map->map_num].offset = imap_offset;
}

/*
1. here assume data,inode,imap all adjacent and only one data/inode block.
2. here only copy the whole imap because it is accessed each time.
while only copy necessary data/inode blocks similar to chapter 9/16.
3. since data always BSIZE
  inode block is written as a whole
  imap blocks are written all.
  so minimal unit is block which is efficient in some way.
4. > The checkpoint region should contain a disk pointer to the current end of
the log; it should also contain pointers to pieces of the inode map here I use
`Inode_Addr` as one disk pointer. I only store the unique block address
sequences of imap to save the space (also because imap are consecutive if COW
the whole imap arrays).
5. Notice:
  here log_end.offset is not changed, so if needed, change outside.
6. > Thus, whenever you write to the disk, you'll just write all file system
updates to the end of the log, and then update the checkpoint region as need be.
  > For example, if you are adding a new block to a file, you would write the
data block, new version of the inode, and a new piece of the inode map to the
end of the log;
  TODO use only "a new piece of the inode map".
7. This function only write but doesn't modify the imap, etc.
*/
void write_data_inode_imap_checkpoint(Block_Offset_Addr *target_inode_block,
                                      Inode *inode_inst, char *data_buf,
                                      Inode_Map (*imap)[MAP_MAX_SIZE],
                                      uint old_block, Checkpoint *ck_ptr,
                                      Map_Num_Index_Map *num_index_map,
                                      void *img_ptr,
                                      Map_Num_Entry_Map *num_entry_map) {
  uint target_inode_block_num = target_inode_block->block;
  memmove(mmap_file_ptr + BSIZE * (target_inode_block_num - 1), data_buf,
          BSIZE);
  /*
  1. assume adjacent pattern as 9/16 in chapter lfs.
  2. here write the whole inode to keep the offset structure similar to chapter
  9/16 2rd image I[k] and also for quick access adjacent with imap. here for
  simplicity and due to no cleaning, not change other inum->inode_addr maps in
  this new inode block.

  so with the offset still much internal fragmentation if not changing the
  imap... so I will change the imap although high overheads but it saves much
  space by no redundant inode blocks.
  3. only write one inode,
  so no need to change inode addr stored in the old block which the old inode
  resides in. so comment the two function calls.
  // modify_inum_index(old_block,target_inode_block_num);
  // memmove(mmap_file_ptr + BSIZE * target_inode_block_num, mmap_file_ptr +
  BSIZE * old_block, BSIZE);
  4. comment them because they are run in update_checkpoint_based_on_map_num.
  // memmove(mmap_file_ptr + BSIZE * (target_inode_block_num+1), imap,
  sizeof(Inode_Map)*imap_global_index.map_num);
  // memmove(mmap_file_ptr, ck_ptr, sizeof(Checkpoint));
  */

  memmove_inode_block(inode_inst, NULL, mmap_file_ptr, target_inode_block);
  /*
  here only one entry of imap array is modified, so not call
  `add_entry_to_imap`.
  */
  // add_entry_to_imap(&imap_global_index,imap,inum_list,checkpoint_ptr,img_ptr,dir_inode,1);
  modify_ck(ck_ptr, num_index_map, sizeof(Inode_Map) + sizeof(Inode),
            sizeof(Inode), target_inode_block_num);
  write_checkpoint_and_EndImap(ck_ptr, num_entry_map, img_ptr, imap);
}

/*
> writes a block of size 4096 bytes at the block offset specified by block
Not show append or overwrite
*/
int write_lfs(int inum, int block, Inode_Map (*imap)[MAP_MAX_SIZE],
              void *img_ptr, Map_Num_Entry_Map *num_entry_map) {
  /*
  since only sequential write is ensured,
  so the block doesn't need to be adjacent with the `checkpoint.log_end`
  But to decrease the external fragmentation and avoid the overwrite of the
  otherwise scattered disk space, I add the sequential ensure.
  */
  int overwrite = 0, target_data_block = checkpoint.log_end.block + 1;
  /*
  due to MFS_Creat's existence, here assume file inode exists,
  so unused Inode->data_ptr will be reset to 0.
  */
  check_inum_exist(inum, tmp_inode_addr, imaps_index_map, imap);

  check_inode_type(inode_inst, mmap_file_ptr, tmp_inode_addr, sizeof(Inode),
                   REGULAR_FILE);
  /*
  since
  > we won't be implementing cleaning
  based on lfs chapter 9/16, not allow the overlap.
  */
  int i;
  for (i = 0; inode_inst.data_ptr[i] != 0; i++) {
    if (block == inode_inst.data_ptr[i]) {
      overwrite = 1;
      break;
    }
  }
  /*
  due to no indirect ptr for large files, here think as one error if this file
  is too large.
  */
  if (i == DIRECT_POINTER_SIZE) {
    return -1;
  }
  char *block_buf = calloc(BSIZE, 1);
  UDP_Read(sd, &addr, block_buf, BSIZE);
  if (overwrite) {
    char *orig_block_buf = calloc(BSIZE, 1);
    memmove(orig_block_buf, img_ptr + BSIZE * inode_inst.data_ptr[i], BSIZE);
    if (strncmp(orig_block_buf, block_buf, BSIZE) == 0) {
      free(orig_block_buf);
      return 0; // not overwrite the same data.
    }
    free(orig_block_buf);
    /*
    assume default to next available block if using the chapter 4/16 pattern.
    */
    inode_inst.data_ptr[i] = target_data_block; // modify one data block addr
  }
  if (block != target_data_block) {
    return -1;
  }
  if (!overwrite) {
    inode_inst.data_ptr[i] = target_data_block; // add one data block
    inode_inst.size += BSIZE;
  }
  Block_Offset_Addr *target_inode_addr = NULL;
  uint old_block = modify_inode_addr(
      imap, &imaps_index_map, target_data_block + 1, 0, &target_inode_addr);
  write_data_inode_imap_checkpoint(
      target_inode_addr, &inode_inst, block_buf, imap, old_block, &checkpoint,
      &imaps_index_map, mmap_file_ptr, num_entry_map);
  free(block_buf);
  sync_file(img_ptr, IMG_SIZE);
  return 0;
}

/*
(Inode_Map *)(imap)+index is also ok.
*/
static inline Inode_Map *array_ptr_to_elem_ptr(Inode_Map (*imap)[MAP_MAX_SIZE],
                                               uint index) {
  assert(index <= MAP_MAX_SIZE);
  return &(*imap)[index];
}

/*
return ptr pointing to data in the imap.
*/
Block_Offset_Addr *get_inode_addr(Map_Num_Index_Map *imaps_index_map,
                                  Inode_Map (*imap)[MAP_MAX_SIZE]) {
  Inode_Map *target_imap =
      array_ptr_to_elem_ptr(imap, imaps_index_map->map_num);
  Block_Offset_Addr *target_inode_addr =
      target_imap->inode_addr + target_imap->maps[imaps_index_map->index].index;
  return target_inode_addr;
}

int read_lfs(int inum, int block, Inode_Map (*imap)[MAP_MAX_SIZE]) {
  check_inum_exist(inum, tmp_inode_addr, imaps_index_map, imap);

  read_inode(inode_inst, mmap_file_ptr, tmp_inode_addr, sizeof(Inode));
  uint exist = 0;
  int i;
  for (i = 0; inode_inst.data_ptr[i] != 0; i++) {
    if (block == inode_inst.data_ptr[i]) {
      exist = 1;
    }
  }
  if (!exist) {
    return -1; // invalid block
  }
  char to_send[BSIZE] = {0};
  if (inode_inst.type == DIRECTORY) {
    MFS_DirEnt_t dir_entry;
    char entry[sizeof(MFS_DirEnt_t)]; // this has small redundant space
    uint offset = 0, to_send_num;
    if (inode_inst.size < BSIZE) {
      to_send_num = inode_inst.size;
    } else if (i * BSIZE < inode_inst.size) {
      to_send_num = BSIZE;
    }
    while (offset != to_send_num) {
      if (offset != 0) {
        strncat(to_send, "\n", 2);
      }
      read_data(&dir_entry, mmap_file_ptr, block, offset, sizeof(MFS_DirEnt_t));
      sprintf(entry, "%s,%d", dir_entry.name, dir_entry.inum);
      strncat(to_send, entry, strlen(entry));
      offset += sizeof(MFS_DirEnt_t);
    }
    UDP_Write(sd, &addr, to_send, to_send_num);
  } else if (inode_inst.type == REGULAR_FILE) {
    read_data(&to_send, mmap_file_ptr, block, 0, BSIZE);
    UDP_Write(sd, &addr, to_send, BSIZE);
  }
  return 0;
}

void send_ret(char *reply, int ret) {
  sprintf(reply, "%d", ret);
  UDP_Write(sd, &addr, reply, BUFFER_SIZE);
}

/*
https://softwareengineering.stackexchange.com/a/127990
naming local variables
*/
int search_dir(char *file_name, Inode *pinode_ptr, MFS_DirEnt_t *dir_entry,
               uint *offset) {
  uint offset_loc = *offset, ptr_index = 0;
  assert(offset_loc == 0);
  /*
  1. this is to send the all directory data when multiple blocks instead of one
  specific.
  2. this is originally for read_lfs with `to_send`, etc.
  */
  while (offset_loc != pinode_ptr->size) {
    /*
    this branch has some overheads if CPU branch predicter is not good
    it can be removed by changing the `offset_loc!=inode_inst.size` condition.
    */
    if (offset_loc % BSIZE == 0 && offset_loc != 0) {
      ptr_index++;
    }
    read_data(dir_entry, mmap_file_ptr, pinode_ptr->data_ptr[ptr_index],
              offset_loc % BSIZE, sizeof(MFS_DirEnt_t));
    if (strncmp(dir_entry->name, file_name, strlen(file_name)) == 0) {
      *offset = offset_loc;
      return 0;
    }
    offset_loc += sizeof(MFS_DirEnt_t);
  }
  return -1;
}

int find_available_inum(Inode_Map (*imap_ptr)[MAP_MAX_SIZE]) {
  /*
  here assume consecutive, otherwise it is difficult to allocate a new inum
  which has no collision.
  */
  Inode_Map *imap = array_ptr_to_elem_ptr(imap_ptr, 0);
  for (int i = 0; i <= imap_global_index.map_end_index; i++) {
    /*
    skip the ROOT_INUM
    */
    for (int j = 1; j < MAP_ENTRY_SIZE; j++) {
      if (imap[i].maps[j].inum == 0 || imap[i].maps[j].inum == -1) {
        return i * MAP_ENTRY_SIZE + j;
      }
    }
  }
  return -1;
}

/*
here assume all written to one new block.
*/
void static inline move_inode_update_ck(Inode *file_inode, void *img_ptr,
                                        Checkpoint *ck_ptr) {
  memmove_inode_block(file_inode, NULL, img_ptr, &ck_ptr->log_end);
  ck_ptr->log_end.offset += sizeof(Inode);
}

void static inline creat_update_imap_ck(int pinum, int inum, Inode *file_inode,
                                        Inode *inode_inst,
                                        Inode_Map (*imap)[MAP_MAX_SIZE],
                                        Checkpoint *ck_ptr, void *img_ptr,
                                        Map_Num_Index_Map *imaps_index_map,
                                        Map_Num_Entry_Map *num_entry_map) {
  int inum_list[2] = {inum, pinum};
  Inode *inode_list[2] = {file_inode, inode_inst};
  add_entry_to_imap(num_entry_map, imap, inum_list, ck_ptr, img_ptr,
                    &(inode_list[0]), 2);
  write_checkpoint_and_EndImap(ck_ptr, num_entry_map, img_ptr, imap);
  /*
  write pinum_imap
  */
  memmove_imap(&checkpoint, imaps_index_map->map_num, img_ptr,
               (Inode_Map *)(imap) + imaps_index_map->map_num);
}

int creat_file_lfs(int pinum, char *type, char *name,
                   Inode_Map (*imap)[MAP_MAX_SIZE], Checkpoint *ck_ptr,
                   void *img_ptr, Map_Num_Entry_Map *num_entry_map) {
  if (strlen(name) + 1 > NAME_MAX_LEN) {
    return -1;
  }
  check_inum_exist(pinum, tmp_inode_addr, imaps_index_map, imap);
  read_inode(inode_inst, img_ptr, tmp_inode_addr, sizeof(Inode));
  MFS_DirEnt_t dir_entry;
  uint offset = 0;
  int ret = search_dir(name, &inode_inst, &dir_entry, &offset);
  /*
  find one existing entry.
  */
  if (ret == 0) {
    return 0;
  }
  uint inum = find_available_inum(imap);
  Inode file_inode;
  if (strncmp(type, DIRECTORY_STR, strlen(DIRECTORY_STR)) == 0) {
    /*
    1. here for simplicity use the consecutive inum.
    */
    create_dir_update_pdir(pinum, inum, img_ptr, name, &inode_inst, ck_ptr,
                           num_entry_map, &file_inode);
    creat_update_imap_ck(pinum, inum, &file_inode, &inode_inst, imap, ck_ptr,
                         img_ptr, &imaps_index_map, num_entry_map);
  } else if (strncmp(type, REGULAR_FILE_STR, strlen(REGULAR_FILE_STR)) == 0) {
    create_file_update_pdir(inum, img_ptr, name, &inode_inst, ck_ptr,
                            &file_inode);
    creat_update_imap_ck(pinum, inum, &file_inode, &inode_inst, imap, ck_ptr,
                         img_ptr, &imaps_index_map, num_entry_map);
  }
  sync_file(img_ptr, IMG_SIZE);
  return 0;
}

/*
based on `get_inode_addr` this will change data stored in imap.
*/
uint modify_inode_addr(Inode_Map (*imap)[MAP_MAX_SIZE],
                       Map_Num_Index_Map *dir_index, uint target_data_block,
                       uint offset, Block_Offset_Addr **target_inode_addr_ptr) {
  Block_Offset_Addr *target_inode_addr = *target_inode_addr_ptr;
  target_inode_addr = get_inode_addr(dir_index, imap);
  uint old_block = target_inode_addr->block;
  target_inode_addr->block = target_data_block;
  target_inode_addr->offset = offset;
  *target_inode_addr_ptr = target_inode_addr;
  return old_block;
}

/*
> For directory entries that are not yet in use (in an allocated 4-KB directory
block), the inode number should be set to -1. > This way, utilities can scan
through the entries to check if they are valid.
*/
int unlink_file_lfs(int pinum, char *name, Inode_Map (*imap)[MAP_MAX_SIZE],
                    Checkpoint *ck_ptr, void *img_ptr,
                    Map_Num_Entry_Map *num_entry_map) {
  Map_Num_Index_Map dir_index;
  check_inum_exist(pinum, tmp_inode_addr, dir_index, imap);
  read_inode(inode_inst, img_ptr, tmp_inode_addr, sizeof(Inode));
  MFS_DirEnt_t dir_entry;
  uint offset = 0;
  int ret = search_dir(name, &inode_inst, &dir_entry, &offset);
  /*
  > Note that the name not existing is NOT a failure by our definition
  just similar to `rm not_exist_file` which only warns but not throw errors.
  */
  if (ret == -1) { // not found
    return 0;
  }
  Inode file_inode;
  check_inum_exist(dir_entry.inum, tmp_inode_addr, imaps_index_map, imap);
  read_inode(file_inode, img_ptr, tmp_inode_addr, sizeof(Inode));
  if (file_inode.type == DIRECTORY) {
    int invalid_cnt = 0;
    search_invalid_dir_entry(&file_inode, &invalid_cnt);

    if (invalid_cnt * sizeof(MFS_DirEnt_t) !=
        (file_inode.size - 2 * sizeof(MFS_DirEnt_t))) {
      return -1;
    }
  }
  /*
  1. since here only invalidate instead of remove, so pinode size doesn't
  change.
  2. if removing one entry inside the multiple data block,
  then all later blocks need to be move backward, this overhead is a little
  high, so I only invalidate.
  */
  dir_entry.inum = -1;
  memset(dir_entry.name, 0, sizeof(dir_entry.name));

  assert(ck_ptr->log_end.offset != 0); // ensure img has been inited
  uint orig_block_index = offset / BSIZE;
  ck_ptr->log_end.block++;
  ck_ptr->log_end.offset = 0;
  uint target_data_block = ck_ptr->log_end.block;
  /*
  data
  */
  memmove(img_ptr + BSIZE * (target_data_block),
          img_ptr + BSIZE * (inode_inst.data_ptr[orig_block_index]), BSIZE);
  memmove(img_ptr + BSIZE * (target_data_block) + offset % BSIZE, &dir_entry,
          sizeof(MFS_DirEnt_t));
  /*
  inode
  */
  inode_inst.data_ptr[orig_block_index] = target_data_block;
  ck_ptr->log_end.block++;
  uint inode_imap_block = ck_ptr->log_end.block;
  move_inode_update_ck(&inode_inst, img_ptr, ck_ptr);
  /*
  imap and checkpoint
  use imap to unlink the file data and file inode, and let the cleaner reset
  these blocks later.
  1. modify the imap data
  */
  Block_Offset_Addr *target_inode_addr = NULL;
  modify_inode_addr(imap, &dir_index, inode_imap_block, 0,
                    &target_inode_addr); // dir imap
  Inode_Map *file_imap = array_ptr_to_elem_ptr(imap, imaps_index_map.map_num);
  file_imap->maps[imaps_index_map.index].inum =
      -1; // mark unlinked. See `get_inum_index` how this works.

  /*
  write the imaps
  */
  uint target_map_num = dir_index.map_num;
  Inode_Map *dir_imap = array_ptr_to_elem_ptr(imap, target_map_num);
  ck_ptr->imap_addr[target_map_num].block = inode_imap_block;
  assert(ck_ptr->log_end.offset == sizeof(Inode));
  ck_ptr->imap_addr[target_map_num].offset = ck_ptr->log_end.offset;
  ck_ptr->log_end.offset += sizeof(Inode_Map);
  memmove_imap(ck_ptr, target_map_num, img_ptr, dir_imap);

  /*
  two imaps may be same
  but inodes is specific with each inum, so they can't be same.
  */
  if (imaps_index_map.map_num != target_map_num) {
    target_map_num = imaps_index_map.map_num;
    ck_ptr->imap_addr[target_map_num].block = inode_imap_block;
    ck_ptr->imap_addr[target_map_num].offset = ck_ptr->log_end.offset;
    ck_ptr->log_end.offset += sizeof(Inode_Map);
    memmove_imap(ck_ptr, target_map_num, img_ptr, dir_imap);
  }

  /*
  checkpoint
  */
  memmove(img_ptr, &checkpoint, sizeof(Checkpoint));
  sync_file(img_ptr, IMG_SIZE);

  return 0;
}

// server code
int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "argc error\n");
    exit(EXIT_FAILURE);
  }
  uint port_num = atoi(argv[1]);
  sd = UDP_Open(port_num);
  assert(sd > -1);
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
      mmap_file_ptr =
          init_img(img_stream, &imap_gbl, &checkpoint, &imap_global_index);
    }
  } else {
    mmap_file_ptr = file_mmap(img_stream);
    read_img(mmap_file_ptr, &imap_gbl, &checkpoint);
  }

  char *tmp_str;
  uint inum, pinum;
  int ret = 0;
  while (1) {
    char message[BUFFER_SIZE] = {0};
    printf("server:: waiting...\n");
    UDP_Read(sd, &addr, message, BUFFER_SIZE);
    char *sep_ptr = message;
    /*
    cmd
    */
    tmp_str = strsep(&sep_ptr, ",");
    /*
    char can use switch, str just if-else
    after all is using jmp instruction.
    https://stackoverflow.com/a/4014900/21294350
    */
    if (strncmp(tmp_str, "Lookup", strlen("Lookup") + 1) == 0) {
      tmp_str = strsep(&sep_ptr, ",");
      inum = atoi(tmp_str);
      assert(strstr(sep_ptr, ",") == NULL);
      ret = lookup_lfs(inum, sep_ptr, &imap_gbl);
      send_ret(reply, ret);
    } else if (strncmp(tmp_str, "Stat", strlen("Stat") + 1) == 0) {
      tmp_str = strsep(&sep_ptr, ",");
      assert(sep_ptr == NULL);
      inum = atoi(tmp_str);
      MFS_Stat_t file_stat;
      ret = stat_lfs(inum, &file_stat, &imap_gbl);
      /*
      > these functions return the number of characters printed (excluding the
      null byte used to end output to strings) so no need to memset all zero to
      ensure explicit ending.
      */
      if (ret == -1) {
        sprintf(reply, "%d", ret);
      } else {
        sprintf(reply, "%d,%s", file_stat.size,
                file_stat.type == DIRECTORY ? "DIRECTORY" : "REGULAR_FILE");
      }
      UDP_Write(sd, &addr, reply, BUFFER_SIZE);
    } else if (strncmp(tmp_str, "Write", strlen("Write") + 1) == 0) {
      inum = atoi(strsep(&sep_ptr, ","));
      int block = atoi(strsep(&sep_ptr, ","));
      assert(sep_ptr == NULL);
      sprintf(reply, "need data"); // this is fake msg which is not manipulated
                                   // by the client
      UDP_Write(sd, &addr, reply, BUFFER_SIZE);
      ret =
          write_lfs(inum, block, &imap_gbl, mmap_file_ptr, &imap_global_index);
      send_ret(reply, ret);
    } else if (strncmp(tmp_str, "Read", strlen("Read") + 1) == 0) {
      inum = atoi(strsep(&sep_ptr, ","));
      int block = atoi(strsep(&sep_ptr, ","));
      assert(sep_ptr == NULL);
      ret = read_lfs(inum, block, &imap_gbl);
      if (ret == -1) {
        /*
        here assume no data block has the same
        */
        sprintf(reply, READ_FAILURE_MSG);
        UDP_Write(sd, &addr, reply, BUFFER_SIZE);
      }
      send_ret(reply, ret);
    } else if (strncmp(tmp_str, "Creat", strlen("Creat") + 1) == 0) {
      pinum = atoi(strsep(&sep_ptr, ","));
      char *filetype = strsep(&sep_ptr, ",");
      char *name = strsep(&sep_ptr, ",");
      assert(sep_ptr == NULL);
      ret = creat_file_lfs(pinum, filetype, name, &imap_gbl, &checkpoint,
                           mmap_file_ptr, &imap_global_index);
      send_ret(reply, ret);
    } else if (strncmp(tmp_str, "Unlink", strlen("Unlink") + 1) == 0) {
      pinum = atoi(strsep(&sep_ptr, ","));
      char *name = strsep(&sep_ptr, ",");
      assert(sep_ptr == NULL);
      ret = unlink_file_lfs(pinum, name, &imap_gbl, &checkpoint, mmap_file_ptr,
                            &imap_global_index);
      send_ret(reply, ret);
    }
  }
  if (munmap(mmap_file_ptr, IMG_SIZE) == -1) {
    fprintf(stderr, "munmap failed\n");
    // exit(EXIT_FAILURE);
  }
  return 0;
}
