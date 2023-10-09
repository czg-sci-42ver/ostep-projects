#include "../mfs.h"
#include "../mfs_helper.h"
#define RETRY_TIMES 10
void send_ret(char *reply, int ret);
Block_Offset_Addr *get_inode_addr(Map_Num_Index_Map *imaps_index_map,
                                  Inode_Map (*imap)[MAP_MAX_SIZE]);
void create_dir(int pinum, int dinum, void *img_ptr, Inode *dir_inode,
                Map_Num_Entry_Map *num_entry_map, Checkpoint *ck_ptr);
void static inline memmove_inode_block(void *inode_ptr, void *img_ptr_src,
                                       void *img_ptr_dst,
                                       Block_Offset_Addr *inode_block);
void static inline move_inode_update_ck(Inode *file_inode, void *img_ptr,
                                        Checkpoint *ck_ptr);
Block_Offset_Addr *get_inum_index(uint inum, Map_Num_Index_Map *map,
                                  Inode_Map (*imap)[MAP_MAX_SIZE]);
static inline Inode_Map *array_ptr_to_elem_ptr(Inode_Map (*imap)[MAP_MAX_SIZE],
                                               uint index);
uint modify_inode_addr(Inode_Map (*imap)[MAP_MAX_SIZE],
                       Map_Num_Index_Map *dir_index, uint target_data_block,
                       uint offset, Block_Offset_Addr **target_inode_addr);