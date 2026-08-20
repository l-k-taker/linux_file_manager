#include "directory.h"
#include "log.h"
#include "path_manager.h"

#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


/*
====================================

获取目录列表

类似:
ls

返回:
0 成功
-1失败

====================================
*/
int dir_list(
        char *path,
        char *buffer,
        int size
)
{


    DIR *dir;


    struct dirent *entry;



    dir=opendir(path);



    if(dir==NULL)
    {

        LOG_ERROR(
        "open directory failed"
        );


        return -1;

    }



    memset(
        buffer,
        0,
        size
    );



    while(
        (entry=readdir(dir))
        !=NULL
    )
    {


        /*
            去除:
            .
            ..
        */

        if(entry->d_name[0]=='.')
            continue;



        /*
            防止越界

        */

        if(strlen(buffer)
            +
            strlen(entry->d_name)
            +
            2
            >= size)
        {
            break;
        }




        strcat(
            buffer,
            entry->d_name
        );


        strcat(
            buffer,
            "\n"
        );


    }



    closedir(dir);



    LOG_INFO(
    "list directory success"
    );



    return 0;

}






/*
====================================

创建目录

类似:

mkdir test

====================================
*/
int dir_create(
char *path
)
{

    int ret;


    ret = path_mkdir_recursive(
        path
    );


    if(ret < 0)
    {

        LOG_ERROR(
        "create directory failed"
        );


        return -1;

    }



    LOG_INFO(
    "create directory success"
    );


    return 0;
}





/*
====================================

删除目录

类似:

rmdir test

====================================
*/
int dir_delete(
char *path
)
{

    if(rmdir(path)<0)
    {

        if(errno == ENOTEMPTY)
        {

            LOG_ERROR(
            "delete directory failed: directory not empty"
            );

        }
        else if(errno == ENOENT)
        {

            LOG_ERROR(
            "delete directory failed: no such directory"
            );

        }
        else if(errno == EACCES)
        {

            LOG_ERROR(
            "delete directory failed: permission denied"
            );

        }
        else
        {

            LOG_ERROR(
            "delete directory failed: unknown error"
            );

        }


        return -1;
    }



    LOG_INFO(
    "delete directory success"
    );


    return 0;
}