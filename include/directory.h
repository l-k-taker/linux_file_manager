#ifndef DIRECTORY_H
#define DIRECTORY_H


int dir_list(
        char *path,
        char *buffer,
        int size
);



int dir_create(
        char *path
);



int dir_delete(
        char *path
);



#endif
