#include <stdio.h>
#include <errno.h>
#include <stdlib.h>    // exit
#include <string.h>    // strerror

#define BUFFER_SIZE 1024

int 
main(int argc, char *argv[])
{
    FILE *fp;
    char buff[BUFFER_SIZE];

    for (size_t i = 1; i < argc; i++) // Detail 4
    {
        if ((fp = fopen(argv[i], "r")) == NULL) // Detail 1
        {
            printf("wcat: cannot open file\n"); // Detail 4
            exit(EXIT_FAILURE);
        }

        while (fgets(buff, BUFFER_SIZE, fp)){
            // printf("line: %s", buff);
            printf("%s", buff);
            /*
            Detail 4
            */
            if (errno!=0) {
                printf("wcat: failed to read file\n");
                exit(EXIT_FAILURE); 
            }
        }

        errno = 0;
        if (fclose(fp) != 0) {
            strerror(errno);
            exit(EXIT_FAILURE); // Detail 2
        }
    }

    // fclose(fp);

    return 0; // Detail 3
}
