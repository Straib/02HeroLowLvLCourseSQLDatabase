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

int find_employee_index(struct dbheader_t *dbhdr, struct employee_t *employees, char *removestring)
{
    if (!dbhdr || !employees || !removestring)
    {
        return STATUS_ERROR;
    }

    int index = 0;

    for (; index < dbhdr->count; index++)
    {
        if (strcmp(employees[index].name, removestring) == 0)
        {
            return index;
        }
    }

    return -1;
}

int remove_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *removestring)
{
    if (!dbhdr || !employees || !removestring)
    {
        return STATUS_ERROR;
    }

    int userIndex = find_employee_index(dbhdr, *employees, removestring);
    if(userIndex == -1) {
        perror("no such employee\n");
        return STATUS_ERROR;
    }

    memmove(&(*employees)[userIndex], &(*employees)[userIndex + 1], (dbhdr->count - userIndex - 1) * sizeof(struct employee_t));

    size_t new_count = (size_t)dbhdr->count - 1;

    if (new_count == 0) {
        free(*employees);
        *employees = NULL;
        dbhdr->count = 0;
        return STATUS_SUCCESS;
    }

    struct employee_t *newbuf = realloc(*employees, sizeof(struct employee_t) * new_count);

    if (!newbuf)
        return STATUS_ERROR;

    *employees = newbuf;
    dbhdr->count = (uint16_t)new_count;

    return STATUS_SUCCESS;
}

int list_employees(struct dbheader_t *dbhdr, struct employee_t *employees)
{
    if (dbhdr == NULL || employees == NULL)
    {
        return STATUS_ERROR;
    }

    for (int i = 0; i < dbhdr->count; i++)
    {
        printf("Employee %d\n", i);
        printf("\tName: %s\n", employees[i].name);
        printf("\tAdress: %s\n", employees[i].address);
        printf("\tHours: %d\n", employees[i].hours);
    }

    return STATUS_SUCCESS;
}

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring)
{
    if (!dbhdr || !employees || !addstring)
    {
        return STATUS_ERROR;
    }

    char *name = strtok(addstring, ",");
    char *addr = strtok(NULL, ",");
    char *hours = strtok(NULL, ",");
    if (!name || !addr || !hours)
        return STATUS_ERROR;

    size_t new_count = (size_t)dbhdr->count + 1;
    struct employee_t *newbuf = realloc(*employees, sizeof(struct employee_t) * new_count);
    if (!newbuf)
        return STATUS_ERROR;

    *employees = newbuf;

    struct employee_t *slot = &newbuf[new_count - 1];
    memset(slot, 0, sizeof(*slot));

    strncpy(slot->name, name, sizeof(slot->name) - 1);
    slot->name[sizeof(slot->name) - 1] = '\0';

    strncpy(slot->address, addr, sizeof(slot->address) - 1);
    slot->address[sizeof(slot->address) - 1] = '\0';

    slot->hours = atoi(hours);

    dbhdr->count = (uint16_t)new_count;

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
    if (got != (ssize_t)((size_t)count * sizeof(struct employee_t)))
    {
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

    int realcount = dbhdr->count;

    off_t expected_filesize = sizeof(struct dbheader_t) + (sizeof(struct employee_t) * realcount);

    if(ftruncate(fd, expected_filesize) == -1) {
        perror("ftruncate failed");
        return STATUS_ERROR;
    }

    struct dbheader_t hdr_copy = *dbhdr;
    hdr_copy.magic = htonl(hdr_copy.magic);
    hdr_copy.filesize = htonl((uint32_t)(sizeof(struct dbheader_t) + (sizeof(struct employee_t) * realcount)));
    hdr_copy.count = htons((uint16_t)realcount);
    hdr_copy.version = htons((uint16_t)hdr_copy.version);

    if (lseek(fd, 0, SEEK_SET) < 0)
        return STATUS_ERROR;

    if (write(fd, &hdr_copy, sizeof(hdr_copy)) != sizeof(hdr_copy))
        return STATUS_ERROR;

    for (int i = 0; i < realcount; ++i)
    {
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
