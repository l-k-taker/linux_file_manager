#ifndef PATH_MANAGER_H
#define PATH_MANAGER_H


#define PATH_MAX_LEN 512


/*
    获取程序所在目录

    例如:
    /mnt/nfs/app/file_manager_arm

    返回:
    /mnt/nfs/app
*/
int get_app_root(char *root_path, int size);



/*
    拼接路径

    root + relative_path

*/
int path_join(
    char *dst,
    int size,
    const char *root,
    const char *relative
);


int path_mkdir_recursive(
    const char *path
);

int path_get_dir(
    const char *filepath,
    char *dir,
    int size
);

#endif