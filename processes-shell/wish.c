/*
1. run `./echo_test.sh`
to check the test.
2. 20.desc based on the before basic functions.
*/
#include "wish.h"
#include <ctype.h>  // isspace
#include <regex.h>  // regcomp, regexec, regfree
#include <stdio.h>  // fopen, fclose, fileno, getline, feof
#include <stdlib.h> // exit
#include <sys/types.h>
#include <sys/wait.h> // waitpid

FILE *in = NULL;
char *paths[BUFF_SIZE] = {"/bin", NULL};
char *line = NULL;

void clean(void) {
  free(line);
  fclose(in);
}

char *trim(char *s) {
  // trim leading spaces
  while (isspace(*s))
    s++;

  if (*s == '\0')
    return s; // empty string

  // trim trailing spaces
  char *end = s + strlen(s) - 1;
  while (end > s && isspace(*end))
    end--;

  end[1] = '\0';
  return s;
}

void *parseInput(void *arg) {
  /*
  here args -> char**, which points to array of "char *"s.
  */
  char *args[BUFF_SIZE];
  int args_num = 0;
  FILE *output = stdout;
  struct function_args *fun_args = (struct function_args *)arg;
  char *commandLine = fun_args->command;

  /*
  1. After this, there should not be any ">" inside commandLine
  this is checked by `strstr(commandLine, ">") != NULL`

  2. by `man strsep`, `commandLine` should not contain ">" 
    > and *stringp is updated to point past the token.
  3. `getline(&line, &linecap, in))` make `commandLine` only one line.
  */
  char *command = strsep(&commandLine, ">");
  /*
  *command == '\0' -> 12.desc,16.desc(where only '&' -> '\0' by `strsep`).
  */
  if (command == NULL || *command == '\0') {
    if (*command == '\0') {
      printf("only >\n");
      /*
      control the output order
      */
      fflush(stdout);
    }
    printError();
    return NULL;
  }
  /*
  15.desc, 18.desc
  */
  command = trim(command);

  /*
  Notice this can be only checked whether '>' exists, but not whether '>' ends
  from https://stackoverflow.com/a/6866123/21294350
  > and *stringp is made NULL.
  */
  if (commandLine != NULL) {
    // printf("%p\n%p\n",commandLine,NULL);
    // contain white space in the middle or ">"
    regex_t reg;
    /*
    https://stackoverflow.com/a/42022766/21294350
    > In C string literals the \ has a special meaning, it's for representing characters such as line endings \n.
    */
    if (regcomp(&reg, "\\S\\s+\\S", REG_CFLAGS) != 0) {
      printError();
      regfree(&reg);
      return NULL;
    }
    /*
    9.desc, 10.desc, 
    */
    if (regexec(&reg, commandLine, 0, NULL, 0) == 0 ||
        strstr(commandLine, ">") != NULL) {
      printError();
      regfree(&reg);
      return NULL;
    }

    regfree(&reg);

    /*
    8.desc
    */
    if ((output = fopen(trim(commandLine), "w")) == NULL) {
      printf("can't open the file.\n");
      printError();
      return NULL;
    }
  }

  char **ap = args;
  /*
  1. " " or "\t" as the delimeter by `man 3 strsep`.
  This also check '\t' based on `trim`.
  2. Notice: here `*ap` -> char *; and `**ap` -> char.
  */
  while ((*ap = strsep(&command, " \t")) != NULL)
    /*
    not the starting before '\t'
    */
    if (**ap != '\0') {
      *ap = trim(*ap);
      /*
      track the next cmd str.
      */
      ap++;
      if (++args_num >= BUFF_SIZE)
        break;
    }

  if (args_num > 0)
    executeCommands(args, args_num, output);

  return NULL;
}

int searchPath(char path[], char *firstArg) {
  // search executable file in path
  int i = 0;
  while (paths[i] != NULL) {
    snprintf(path, BUFF_SIZE, "%s/%s", paths[i], firstArg);
    if (access(path, X_OK) == 0)
      return 0;
    i++;
  }
  return -1;
}

void redirect(FILE *out) {
  int outFileno;
  if ((outFileno = fileno(out)) == -1) {
    printError();
    return;
  }

  // From `grep -r ">" tests/*in`, there is no something like `2>` redirect which redirects only the stderr.
  if (outFileno != STDOUT_FILENO) {
    // redirect output
    if (dup2(outFileno, STDOUT_FILENO) == -1) {
      printError();
      return;
    }
    if (dup2(outFileno, STDERR_FILENO) == -1) {
      printError();
      return;
    }
    fclose(out);
  }
}

void executeCommands(char *args[], int args_num, FILE *out) {
  // check built-in commands first
  if (strcmp(args[0], "exit") == 0) {
    /*
    5.desc
    */
    if (args_num > 1)
      printError();
    else {
      atexit(clean);
      exit(EXIT_SUCCESS);
    }
  } else if (strcmp(args[0], "cd") == 0) {
    /*
    1.desc 2.desc
    */
    if (args_num == 1 || args_num > 2)
      printError();
    else if (chdir(args[1]) == -1)
      /* 1.out; See desc file for more information */
      printError();
  } else if (strcmp(args[0], "path") == 0) {
    size_t i = 0;
    /*
    1. ignore this line:This "paths[0]" can be also set to `args[...]`.
    This is to empty the path.
    2. 7.desc -> this will output "An error has occurred" twice.
    */
    paths[0] = NULL;
    for (; i < args_num - 1; i++)
      paths[i] = strdup(args[i + 1]);

    paths[i + 1] = NULL;
  } else {
    // not built-in commands
    char path[BUFF_SIZE];
    if (searchPath(path, args[0]) == 0) {
      pid_t pid = fork();
      if (pid == -1)
        printError();
      else if (pid == 0) {
        // child process
        redirect(out);
        /*
        1. ignore this line: Here maybe imply using the system PATH to search arg
        by `man 3 execv`
        > If the specified filename includes a slash character, then PATH is ignored, and the file at the specified pathname is executed.
        So path is directly executed ignoring args[0]

        2. 3.desc
        */
        if (execv(path, args) == -1)
          printError();
      } else
        waitpid(pid, NULL, 0); // parent process waits child
    } else
      /*
      6.desc
      */ 
      printError(); // not fond in path
  }
}

int main(int argc, char *argv[]) {
  int mode = INTERACTIVE_MODE;
  in = stdin;
  size_t linecap = 0;
  /*
  long int
  */
  ssize_t nread;

  if (argc > 1) {
    mode = BATCH_MODE;
    /*
    13.desc, 14.desc
    */
    if (argc > 2 || (in = fopen(argv[1], "r")) == NULL) {
      printError();
      exit(EXIT_FAILURE);
    }
  }

  while (1) {
    if (mode == INTERACTIVE_MODE)
      printf("wish> ");

    if ((nread = getline(&line, &linecap, in)) > 0) {
      printf("get one line\n");
      char *command;
      int commands_num = 0;
      struct function_args args[BUFF_SIZE];

      /*
      21.desc
      */
      // remove newline character
      if (line[nread - 1] == '\n'){
        printf("no contents got\n");
        line[nread - 1] = '\0';
      }

      char *temp = line;
      
      /*
      1. by `man 2 strsep`:
      when `temp` has no '&', &temp -> NULL and command -> '\0' which is transformed from '&'
      2. 18.desc,19.desc
      */
      while ((command = strsep(&temp, "&")) != NULL)
        if (command[0] != '\0') {
          printf("command:'%s'\n",command);
          /*
          1. why use strdup -> https://stackoverflow.com/a/4079901/21294350
          because of avoiding using const parameter as mutable str.
          2. The `strdup` implies the command ends with '\0'
          */
          args[commands_num++].command = strdup(command);
          /*
          limited command str size.
          */
          if (commands_num >= BUFF_SIZE)
            break;
        }
      /*
      process may be no use with 22.desc
      */
      for (size_t i = 0; i < commands_num; i++)
        if (pthread_create(&args[i].thread, NULL, &parseInput, &args[i]) != 0)
          printError();

      for (size_t i = 0; i < commands_num; i++) {
        if (pthread_join(args[i].thread, NULL) != 0)
          printError();
        if (args[i].command != NULL)
          free(args[i].command);
      }
    } 
    /*
    1. read the EOF of `fopen`.
    2. 13.desc
    */
    else if (feof(in) != 0) {
      printf("reach EOF\n");
      atexit(clean);
      exit(EXIT_SUCCESS); // EOF
    }
  }

  return 0;
}
