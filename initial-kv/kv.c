/*
here use "fprintf(stdout...)" because the tests use this behavior. 
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define DATABASE_FILE "database.txt"
typedef struct kv_map{
  int key;
  char value[20];
} Kv_map;
#define MAP_SIZE 100
#define BIG_SIZE 10000
#define LINE_SIZE 100
// #define LOG
/*
here for simplicity, I only use static array instead of the list and the hash table.
*/
Kv_map maps[MAP_SIZE];
uint origin_map_end;

void write_map(char *kv_str,uint* index){
  char *token;
  uint map_num= *index;
  char *line=kv_str;
  maps[map_num].key = atoi(strsep(&line, ","));
  assert(line!=NULL);
  /*
  ignore '\n' by
  > overwriting the delimiter with a null byte ('\0')
  */
  token=strsep(&line, ",\n");
  strncpy(maps[map_num++].value, token,strlen(token)+1);
  /*
  to skip "\n" sometimes.
  */
  strsep(&line, ",\n");
  assert(line==NULL);
  *index=map_num;
}

void search_map(uint key,uint* index){
  if (maps[0].key==-2) {
    goto search_map_end;
  }
  uint end=*index;
  for (int i=0; i<end; i++) {
    /*
    here assume key -1 is not used.
    */
    if (maps[i].key==-1) {
      continue;
    }
    if (key==maps[i].key) {
      printf("%d,%s\n",maps[i].key,maps[i].value);
      return;
    }
  }
  search_map_end: fprintf(stdout, "%d not found\n", key);
  return;
}

void delete_kv(uint key,uint* index){
  if (maps[0].key==-2) {
    goto delete_kv_end;
  }
  uint end=*index;
  for (int i=0; i<end; i++) {
    if (key==maps[i].key) {
      maps[i].key=-1;
      return;
    }
  }
  delete_kv_end: fprintf(stdout, "%d not found\n", key);
  return;
}

void print_kv(uint* index){
  if (maps[0].key==-2) {
    goto print_kv_end;
  }
  uint end=*index;
  for (int i=0; i<end; i++) {
    if (maps[i].key!=-1) {
      printf("%d,%s\n",maps[i].key,maps[i].value);
    }
  }
  print_kv_end: fprintf(stdout, "empty kv map\n");
  return;
}

void store_kv(uint* index,FILE * stream_ptr){
  char reset_char=0;
  if (maps[0].key==-2) {
    // fwrite(&reset_char, 1, origin_map_end, stream_ptr);
    ftruncate(stream_ptr->_fileno, 0);
    return;
  }
  uint end=*index;
  char *write_contents=calloc(BIG_SIZE,1);
  assert(write_contents!=NULL);
  char *line_str=malloc(LINE_SIZE);
  assert(line_str!=NULL);
  #ifdef LOG
  printf("to write %d kv maps\n",end);
  #endif
  for (int i=0; i<end; i++) {
    if (maps[i].key!=-1) {
      memset(line_str,0,LINE_SIZE);
      if (i!=0) {
        #ifdef LOG
        printf("strlen(\\n) %ld\n",strlen("\n"));
        #endif
        sprintf(line_str,"\n%d,%s",maps[i].key,maps[i].value);
      }else {
        sprintf(line_str,"%d,%s",maps[i].key,maps[i].value);
      }
      #ifdef LOG
      printf("to append write_contents with %s\n",line_str);
      #endif
      strncat(write_contents, line_str,strlen(line_str));
      #ifdef LOG
      printf("after append, write_contents %s\n",write_contents);
      #endif
    }
  }
  #ifdef LOG
  printf("to write write_contents %s\n",write_contents);
  #endif
  fwrite(write_contents, 1, strlen(write_contents), stream_ptr);
  free(write_contents);
  free(line_str);
}

void clear_kv(uint* index){
  maps[0].key=-2;
  *index=0;
}

int main(int argc, char *argv[]){
  if (argc<2) {
    return 0;
  }
  char *token;
  FILE * db_file;
  ssize_t nread;
  char *line = NULL;
  size_t len = 0;
  uint map_num=0;
  int key;

  if((db_file=fopen(DATABASE_FILE,"r+"))==NULL){ // probably not exist
    if (errno==ENOENT) {
      db_file=fopen(DATABASE_FILE,"w+");
    }
  }
  while ((nread = getline(&line, &len, db_file)) != -1) {
    write_map(line, &map_num);
  }
  origin_map_end=db_file->_offset;
  for (int i=1; i<argc; i++) {
    token = strsep(&argv[i], ",");
    switch (*token) {
      case 'p':
        write_map(argv[i], &map_num);
        break;
      case 'g':
      /*
      https://stackoverflow.com/a/92439/21294350
      */
      {
        key=atoi(argv[i]);
        strsep(&argv[i], ",");
        assert(argv[i]==NULL);
        search_map(key, &map_num);
        break;
      }
      case 'd':
        key=atoi(argv[i]);
        strsep(&argv[i], ",");
        assert(argv[i]==NULL);
        delete_kv(key,&map_num);
        break;
      case 'c':
        clear_kv(&map_num);
      case 'a':
        print_kv(&map_num);
      default:
        fprintf(stdout, "bad command\n");
        break;
    }
  }
  fseek(db_file,0,SEEK_SET);
  store_kv(&map_num, db_file);
  fclose(db_file);
}