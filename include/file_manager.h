#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <sys/stat.h>

int fm_create(char *path);


int fm_delete(char *path);


int fm_write(
    char *path,
    char *data
);


int fm_read(
    char *path,
    char *buffer,
    int size
);


int fm_copy(
    char *src,
    char *dst
);


int fm_info(
char *path,
struct stat *st
);


#endif
