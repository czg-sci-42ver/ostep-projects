/*
based on paper p13 and README
*/

#include <stdlib.h>
#include <string.h>
#include "sort.h"

#define NO_MAP_PARALLEL_FILES
#define SFF

void Map_FILE(FILE *file_name) {
    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, file_name) != -1) {
        char *token, *dummy = line;
        while ((token = strsep(&dummy, " \t\n\r")) != NULL) {
            MR_Emit(token, "1");
        }
    }
    free(line);
    fclose(file_name);
}

void Reduce(char *key, Getter get_next, int partition_number) {
    int count = 0;
    char *value;
    /*
    TODO here partition_number may be no use.
    */
    while ((value = get_next(key, partition_number)) != NULL)
        count++;
    printf("%s %d\n", key, count);
}

int main(int argc, char *argv[]) {
    MR_Run_FILE(argc, argv, Map_FILE, 10, Reduce, 10, MR_DefaultHashPartition);
}

static Key_map key_value_list[100];
/*
TODO this is locked version to store into global data struct.
*/
void MR_Emit(char *key, char *value){
  
}

static int file_group_size;

void* Map_wrpapper(void* file_name){
  FILE **file=(FILE **)file_name;
  for (int i=0; i<file_group_size; i++) {
    if (file[i]) {
      Map_FILE(file[i]);
    }
  }
  return NULL;
}

void MR_Run_FILE(int argc, char *argv[], 
	    Mapper_FILE map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition){
        pthread_t threads[num_mappers];
        int thread_num=num_mappers;
        /*
        TODO if num_mappers < argc, how assign new files?
        maybe needs combine some file_str into one.
        */
        int file_num = argc-1;
        FILE **fp=malloc(file_num*sizeof(FILE*));
        
        struct stat stat_buf;
        int file_index;
        file_group_size=file_num/num_mappers+1;
        FILE* fp_group[num_mappers][file_group_size];

        #ifdef SFF
        int thread_target,group_ele_index;
        File_info files_info[file_num];
        #endif

        memset(fp_group, 0, num_mappers*file_group_size*sizeof(FILE*));

        #ifdef NO_MAP_PARALLEL_FILES
        thread_num=num_mappers>argc?argc:num_mappers;
        #endif
        for (int i=1; i<argc; i++) {
          file_index=i-1;
          fp[file_index] = fopen(argv[i], "r");
          printf("read %dth file %s\n",i,argv[i]);
          assert(fp[file_index] != NULL);
          #ifdef SFF
          fstat(fileno(fp[file_index]),&stat_buf);
          files_info[file_index].index=file_index;
          files_info[file_index].size=stat_buf.st_size;
          #endif
        }
        #ifdef SFF
        quickSort(files_info,0,file_num-1);
        for (int i=0; i<file_num; i++) {
          printf("%dth size:%d\n",files_info[i].index,files_info[i].size);
        }
        for (int i=0; i<file_num; i++) {
          group_ele_index = i/num_mappers;
          thread_target=i%num_mappers;
          fp_group[thread_target][group_ele_index]=fp[files_info[i].index];
          printf("thread_target %d group_ele_index %d stores fp_index: %d\n",thread_target,group_ele_index,files_info[i].index);
        }
        // free(fp);
        #endif


        for (int i=0; i<thread_num; i++) {
          /*
          need pass a wrapper func pointer. https://stackoverflow.com/a/559671/21294350
          > Cast between function pointers of different calling conventions
          */
          pthread_create(&threads[i], NULL, Map_wrpapper, fp_group[i]); 
        }
        
        /*
        TODO po maybe should wait for mappers
        > After the mappers are finished, your library should have stored the key/value pairs in such a way that the Reduce() function can be called.
        */

        for (int i=0; i<thread_num; i++) {
          /*
          here use assert to ensure the right behaviors, so not care about the return values.
          */
          pthread_join(threads[i],NULL); 
        }

        /*
        your MR library should use this function to decide which partition (and hence, which reducer thread) 
        gets a particular key/list of values to process.
        */
        HashTable *ht = create_table(num_reducers);
        MR_DefaultHashPartition(, num_reducers);
}