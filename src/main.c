#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[])
{
    printf("Usage: %s -n -f <database file>\n", argv[0]);
    printf("\t -n - create new database file\n");
    printf("\t -f - (required) path to database file\n");
    return;
}

int main(int argc, char *argv[])
{
    int c;
    int dbfd;
    bool newfile = false;
    char *filepath = NULL;
    struct dbheader_t *dbhdr = NULL;
    struct employee_t *emply = NULL;


    while ((c = getopt(argc, argv, "nf:")) != -1)
    {
        switch (c)
        {
        case 'n':
            newfile = true;
            break;
        case 'f':
            filepath = optarg;
            break;
        case '?':
            printf("Unkown option -%c\n", c);
        default:
            return -1;
        }
    }

    if (filepath == NULL)
    {
        printf("Filepath is a required agrument\n");
        print_usage(argv);
    }

    if (newfile)
    {
        dbfd = create_db_file(filepath);
        if (dbfd == STATUS_ERROR)
        {
            printf("Unable to create database file\n");
            return -1;
        }
        if(create_db_header(&dbhdr) == STATUS_ERROR) {
            printf("failed to create database header\n");
        }
    }
    else
    {
        dbfd = open_db_file(filepath);
        if (dbfd == STATUS_ERROR)
        {
            printf("Unable to open database file\n");
            return -1;
        }
        if(validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            printf("Failed to validagte database header \n");
            return -1;
        }
    }

    output_file(dbfd, dbhdr, emply);

    return 0;
}