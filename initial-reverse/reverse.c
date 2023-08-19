#include <stdio.h>    // getline, fileno, fopen, fclose, fprintf
#include <stdlib.h>   // exit, malloc
#include <string.h>   // strdup
#include <sys/stat.h> // fstat
#include <sys/types.h>

/*
1. no need to use do{}while(0) after comparing the assembled codes.
2. Assumptions 7
*/
#define handle_error(msg)                                                      \
    {\
    fprintf(stderr, msg);                                                      \
    exit(EXIT_FAILURE);                                                        \
    }                   
  // } while (0)

typedef struct linkedList {
  char *line;
  struct linkedList *next;
} LinkedList;

int main(int argc, char *argv[]) {
  FILE *in = NULL, *out = NULL;
  in = stdin;
  out = stdout;

  if (argc == 2 && (in = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "reverse: cannot open file '%s'\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  if (argc == 3) {
    if ((in = fopen(argv[1], "r")) == NULL) { // Assumptions 4
      fprintf(stderr, "reverse: cannot open file '%s'\n", argv[1]);
      exit(EXIT_FAILURE);
    }
    if ((out = fopen(argv[2], "w")) == NULL) {
      fprintf(stderr, "reverse: cannot open file '%s'\n", argv[2]);
      exit(EXIT_FAILURE);
    }

    struct stat sb1, sb2;
    if (fstat(fileno(in), &sb1) == -1 || fstat(fileno(out), &sb2) == -1)
      handle_error("reverse: fstat error\n");
    if (sb1.st_ino == sb2.st_ino) // see https://man7.org/linux/man-pages/man2/lstat.2.html
      handle_error("reverse: input and output file must differ\n"); // Assumptions 1
  } else if (argc > 3)
    handle_error("usage: reverse <input> <output>\n"); // Assumptions 6

  LinkedList *head = NULL;
  char *line = NULL;
  size_t len = 0;
  // store lines to list in reverse order
  while (getline(&line, &len, in) != -1) { // Assumptions 2,3
    /*
    from README: The routine malloc() is useful for memory allocation. Perhaps for adding elements to a list?
    */
    LinkedList *node = malloc(sizeof(LinkedList)); // Assumptions 5
    if (node == NULL) {
      free(line);
      handle_error("reverse: malloc failed\n");
    }
    if ((node->line = strdup(line)) == NULL) {
      free(line);
      handle_error("reverse: strdup failed\n");
    }
    node->next = head;
    head = node;
  }

  /*
  from README: Then, we printed out the list in reverse order. Then we made sure to handle error cases. And so forth...
  */
  // print reversed lines
  while (head != NULL) {
    LinkedList *temp = head;
    fprintf(out, "%s", head->line);
    head = head->next;
    free(temp->line);
    free(temp);
  }

  free(line);
  fclose(in);
  fclose(out);
  // return 0;
  exit(1);
}
