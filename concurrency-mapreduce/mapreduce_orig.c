/*
based on paper p13 and README
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "misc_helper.h"

#define NO_MAP_PARALLEL_FILES

void Map(char *file_name) {
    FILE *fp = fopen(file_name, "r");
    assert(fp != NULL);

    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, fp) != -1) {
        char *token, *dummy = line;
        while ((token = strsep(&dummy, " \t\n\r")) != NULL) {
            MR_Emit(token, "1");
        }
    }
    free(line);
    fclose(fp);
}

void Reduce(char *key, Getter get_next, int partition_number) {
    int count = 0;
    char *value;
    while ((value = get_next(key, partition_number)) != NULL)
        count++;
    printf("%s %d\n", key, count);
}

int main(int argc, char *argv[]) {
    MR_Run(argc, argv, Map, 10, Reduce, 10, MR_DefaultHashPartition);
}

void MR_Emit(char *key, char *value){

}

void* Map_wrpapper(void* file_name){
  char *file=(char *)file_name;
  Map(file);
  return NULL;
}

void MR_Run(int argc, char *argv[], 
	    Mapper map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition){
        pthread_t threads[num_mappers];
        int thread_num=num_mappers;
        #ifdef NO_MAP_PARALLEL_FILES
        thread_num=num_mappers>argc?argc:num_mappers;
        #endif
        /*
        TODO if num_mappers < argc, how assign new files?
        maybe needs combine some file_str into one.
        */
        for (int i=0; i<thread_num; i++) {
          /*
          need pass a wrapper func pointer. https://stackoverflow.com/a/559671/21294350
          > Cast between function pointers of different calling conventions
          */
          pthread_create(&threads[i], NULL, Map_wrpapper, argv[i]); 
        }
}