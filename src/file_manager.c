#include "file_manager.h"
#include "log.h"
#include "thread_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/*
    异步复制任务结构
*/
typedef struct
{

    char src[256];

    char dst[256];


}CopyTask;



/*
================================
创建文件
================================
*/
int fm_create(char *path)
{

    int fd;


    fd=open(
        path,
        O_CREAT | O_WRONLY,
        0666
    );


if(fd < 0)
{

     LOG_ERROR(
        "creat file failed"
        );
    return -1;
}



    close(fd);



    LOG_INFO(
    "create file success"
    );

    return 0;

}



/*
================================
删除文件
================================
*/
int fm_delete(char *path)
{

    if(unlink(path)<0)
    {

        LOG_ERROR(
        "delete file failed"
        );


        return -1;

    }



    LOG_INFO(
    "delete file success"
    );


    return 0;

}



/*
================================
写文件
================================
*/
int fm_write(
        char *path,
        char *data
)
{

    int fd;


    fd=open(
        path,
        O_WRONLY | O_APPEND
    );


    if(fd<0)
    {

        LOG_ERROR(
        "open file failed"
        );


        return -1;

    }



    int ret;


    ret=write(
        fd,
        data,
        strlen(data)
    );



    close(fd);



    if(ret<0)
    {

        LOG_ERROR(
        "write file failed"
        );


        return -1;

    }



    LOG_INFO(
    "write file success"
    );


    return 0;

}




/*
================================
读文件

================================
*/
int fm_read(
        char *path,
        char *buffer,
        int size
)
{

    int fd;



    fd=open(
        path,
        O_RDONLY
    );



    if(fd<0)
    {

        LOG_ERROR(
        "read file open failed"
        );


        return -1;

    }



    int len;


    len=read(
        fd,
        buffer,
        size-1
    );


    if(len<0)
    {

        close(fd);


        LOG_ERROR(
        "read file failed"
        );


        return -1;

    }



    buffer[len]='\0';



    close(fd);



    LOG_INFO(
    "read file success"
    );


    return len;

}





/*
================================
文件复制

底层同步复制
================================
*/
int fm_copy(char *src, char *dst)
{
    int fd_src;
    int fd_dst;
    char buffer[4096];
    int len;

    // 检查源文件是否存在
    if(access(src, F_OK) != 0) {
        LOG_ERROR("Source file does not exist");
        errno = ENOENT;
        return -1;
    }

    // 检查源文件是否可读
    if(access(src, R_OK) != 0) {
        LOG_ERROR("Source file not readable");
        errno = EACCES;
        return -1;
    }

    fd_src = open(src, O_RDONLY);
    if(fd_src < 0) {
        LOG_ERROR("open source failed");
        return -1;
    }

    // 检查目标路径是否可写（如果是目录，检查权限）
    char *last_slash = strrchr(dst, '/');
    if(last_slash != NULL) {
        char dir_path[512];
        strncpy(dir_path, dst, last_slash - dst);
        dir_path[last_slash - dst] = '\0';
        
        if(strlen(dir_path) > 0 && access(dir_path, W_OK) != 0) {
            LOG_ERROR("Destination directory not writable");
            close(fd_src);
            errno = EACCES;
            return -1;
        }
    }

    fd_dst = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if(fd_dst < 0) {
        close(fd_src);
        LOG_ERROR("create destination failed");
        return -1;
    }

    while((len = read(fd_src, buffer, sizeof(buffer))) > 0) {
        if(write(fd_dst, buffer, len) != len) {
            close(fd_src);
            close(fd_dst);
            LOG_ERROR("write copy failed");
            return -1;
        }
    }

    close(fd_src);
    close(fd_dst);

    LOG_INFO("copy file success");
    return 0;
}



/*
================================
获取文件信息
================================
*/
int fm_info(
char *path,
struct stat *st
)
{

    if(stat(path,st)<0)
    {
        LOG_ERROR(
            "failed file information"
        );

        return -1;
    }


    LOG_INFO(
        "success file information"
    );


    return 0;
}



