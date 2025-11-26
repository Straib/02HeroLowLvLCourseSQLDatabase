#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "parse.h"

int list_employees(struct dbheader_t *dbhdr, struct employee_t *employees)
{
    if (dbhdr == NULL || employees == NULL) {
        return STATUS_ERROR;
    }

    for (int i = 0; i < dbhdr->count; i++) {
        printf("Employee %d\n", i);
        printf("\tName: %s\n", employees[i].name);
        printf("\tAdress: %s\n", employees[i].address);
        printf("\tHours: %d\n", employees[i].hours);
    }

    return STATUS_SUCCESS;
}


// int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring)
// {

//     if (NULL == dbhdr)
//         return STATUS_ERROR;
//     if (NULL == employees)
//         return STATUS_ERROR;
//     if (NULL == *employees)
//         return STATUS_ERROR;
//     if (NULL == addstring)
//         return STATUS_ERROR;

//     char *name = strtok(addstring, ",");
//     if (NULL == name)
//         return STATUS_ERROR;

//     char *addr = strtok(NULL, ",");
//     if (NULL == addr)
//         return STATUS_ERROR;

//     char *hours = strtok(NULL, ",");
//     if (NULL == hours)
//         return STATUS_ERROR;

//     struct employee_t *e = *employees;


//     e = realloc(e, sizeof(struct employee_t) * (dbhdr->count + 1));
//     if (!e)
//     {
//         return STATUS_ERROR;
//     }

//     *employees = e;

//     strncpy(e[dbhdr->count - 1].name, name, sizeof(e[dbhdr->count - 1].name) - 1);
//     strncpy(e[dbhdr->count - 1].address, addr, sizeof(e[dbhdr->count - 1].address) - 1);

//     e[dbhdr->count - 1].hours = atoi(hours);


//     dbhdr->count++;


//     return STATUS_SUCCESS;
// }

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring)
{
    if (!dbhdr || !employees || !addstring)
        return STATUS_ERROR;

    char *name = strtok(addstring, ",");
    char *addr = strtok(NULL, ",");
    char *hours = strtok(NULL, ",");
    if (!name || !addr || !hours)
        return STATUS_ERROR;

    size_t new_count = (size_t)dbhdr->count + 1;
    struct employee_t *newbuf = realloc(*employees, sizeof(struct employee_t) * new_count);
    if (!newbuf)
        return STATUS_ERROR;

    /* set pointer back */
    *employees = newbuf;

    /* initialize the new slot to zero then fill */
    struct employee_t *slot = &newbuf[new_count - 1];
    memset(slot, 0, sizeof(*slot));

    /* copy and ensure NUL termination */
    strncpy(slot->name, name, sizeof(slot->name) - 1);
    slot->name[sizeof(slot->name) - 1] = '\0';

    strncpy(slot->address, addr, sizeof(slot->address) - 1);
    slot->address[sizeof(slot->address) - 1] = '\0';

    slot->hours = atoi(hours);

    dbhdr->count = (uint16_t)new_count; /* keep dbhdr->count in host-endian */

    return STATUS_SUCCESS;
}


int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut)
{
    if (fd < 0)
    {
        printf("Got a bad FD from the user\n");
        return STATUS_ERROR;
    }

    int count = dbhdr->count;

    struct employee_t *employees = calloc(count, sizeof(struct employee_t));
    if (employees == NULL)
    {
        printf("Malloc failed \n");
        return STATUS_ERROR;
    }

    ssize_t got = read(fd, employees, (size_t)count * sizeof(struct employee_t));
    if (got != (ssize_t)((size_t)count * sizeof(struct employee_t))) {
        free(employees);
        return STATUS_ERROR;
    }

    int i = 0;
    for (; i < count; i++)
    {
        employees[i].hours = ntohl(employees[i].hours);
    }

    *employeesOut = employees;
    return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *dbhdr, struct employee_t *employees)
{
    if (fd < 0 || !dbhdr)
        return STATUS_ERROR;

    int realcount = dbhdr->count; /* host-endian */

    /* Make a temporary copy of header and convert fields for disk */
    struct dbheader_t hdr_copy = *dbhdr;
    hdr_copy.magic = htonl(hdr_copy.magic);
    hdr_copy.filesize = htonl((uint32_t)(sizeof(struct dbheader_t) + (sizeof(struct employee_t) * realcount)));
    hdr_copy.count = htons((uint16_t)realcount);
    hdr_copy.version = htons((uint16_t)hdr_copy.version);

    if (lseek(fd, 0, SEEK_SET) < 0)
        return STATUS_ERROR;

    if (write(fd, &hdr_copy, sizeof(hdr_copy)) != sizeof(hdr_copy))
        return STATUS_ERROR;

    for (int i = 0; i < realcount; ++i) {
        struct employee_t tmp = employees[i];
        tmp.hours = htonl(tmp.hours);
        if (write(fd, &tmp, sizeof(tmp)) != sizeof(tmp))
            return STATUS_ERROR;
    }

    return STATUS_SUCCESS;
}


int validate_db_header(int fd, struct dbheader_t **headerOut)
{
    if (fd < 0)
    {
        printf("Got a bad FD from the user\n");
        return STATUS_ERROR;
    }

    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL)
    {
        printf("Mallorc failed to create a db header \n");
        return STATUS_ERROR;
    }

    if (read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t))
    {
        perror("read");
        free(header);
        return STATUS_ERROR;
    }

    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->magic = ntohl(header->magic);
    header->filesize = ntohl(header->filesize);

    if (header->magic != HEADER_MAGIC)
    {
        printf("Impromper header magic\n");
        free(header);
        return -1;
    }

    if (header->version != 1)
    {
        printf("Impropmer header version\n");
        free(header);
        return -1;
    }

    struct stat dbstat = {0};
    fstat(fd, &dbstat);
    if (header->filesize != dbstat.st_size)
    {
        printf("Corrupted database\n");
        free(header);
        return -1;
    }

    *headerOut = header;
    return STATUS_SUCCESS;
}

int create_db_header(struct dbheader_t **headerOut)
{
    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL)
    {
        printf("Malloc failed to create db header \n");
        return STATUS_ERROR;
    }

    header->version = 0x1;
    header->count = 0;
    header->magic = HEADER_MAGIC;
    header->filesize = sizeof(struct dbheader_t);

    *headerOut = header;

    return STATUS_SUCCESS;
}
