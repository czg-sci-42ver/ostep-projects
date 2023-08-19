#include <stdio.h>
#include <stdlib.h>    // exit, free
#include <string.h>    // strstr

int
main(int argc, char *argv[])
{
    FILE *fp = NULL;
    char *line = NULL;
    size_t linecap = 0;

    if (argc <= 1) { // Detail 1
        printf("wgrep: searchterm [file ...]\n"); // Detail 4
        exit(EXIT_FAILURE);
    }

    if (argc >= 3 && (fp = fopen(argv[2], "r")) == NULL) {
        printf("wgrep: cannot open file\n"); // Detail 5
        exit(EXIT_FAILURE);
    }

    if (argc == 2)
        fp = stdin; // Detail 7

    // printf("argv[1]: %s\n",argv[1]);

    while (getline(&line, &linecap, fp) > 0) // Detail 3
        if (strstr(line, argv[1])) // Detail 2
            printf("%s", line);

    free(line);
    fclose(fp);

    return 0; // Detail 6
}
