#ifndef __MISC_HELPER_H
#define __MISC_HELPER_H
#include <fcntl.h>
#include <pthread.h>
#include "mapreduce.h"
#include "hash.h"
#include <sys/stat.h>
#define Open(pathname, flags,mode) ({assert(open(pathname, flags,mode)!=-1)})
void* Map_wrpapper(void* func);
typedef struct file_info {
  int index;
  int size;
} File_info;
typedef struct key_map {
  char key[100];
//   char *value_list[];
  pthread_mutex_t lock;
} Key_map;
typedef struct reduce_arg {
  int partition_number;
} Reduce_arg ;
char * get_next(char *key, int partition_number);

#endif