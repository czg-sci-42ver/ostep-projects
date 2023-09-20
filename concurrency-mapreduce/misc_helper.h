#ifndef __MISC_HELPER_H
#define __MISC_HELPER_H
#include <assert.h>
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
  char *value_list[];
  pthread_mutex_t lock;
} Key_map;


#endif