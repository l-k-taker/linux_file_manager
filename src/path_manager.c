#include "path_manager.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int get_app_root(char *root_path, int size)
{

    char exe_path[PATH_MAX_LEN];


    /*
        获取当前程序完整路径

        例如:
        /mnt/nfs/app/file_manager_arm
    */
    int len = readlink(
        "/proc/self/exe",
        exe_path,
        sizeof(exe_path)-1
    );


    if(len < 0)
    {
        return -1;
    }


    exe_path[len]='\0';



    /*
        去掉最后的文件名

        /mnt/nfs/app/file_manager_arm

        变成:

        /mnt/nfs/app
    */


    char *last_slash = strrchr(
        exe_path,
        '/'
    );


    if(last_slash == NULL)
    {
        return -1;
    }


    *last_slash='\0';



    strncpy(
        root_path,
        exe_path,
        size-1
    );


    root_path[size-1]='\0';



    return 0;
}





int path_join(
    char *dst,
    int size,
    const char *root,
    const char *relative
)
{

    snprintf(
        dst,
        size,
        "%s/%s",
        root,
        relative
    );


    return 0;
}

int path_get_dir(
    const char *filepath,
    char *dir,
    int size
)
{
    if(filepath == NULL || dir == NULL)
    {
        return -1;
    }


    strncpy(
        dir,
        filepath,
        size-1
    );

    dir[size-1]='\0';


    char *p = strrchr(
        dir,
        '/'
    );


    if(p == NULL)
    {
        strcpy(dir,".");
        return 0;
    }


    /*
        去掉最后文件名

        /app/logs/system.log

        变成:

        /app/logs
    */
    *p='\0';


    return 0;
}

int path_mkdir_recursive(
    const char *path
)
{

    char temp[512];


    strncpy(
        temp,
        path,
        sizeof(temp)-1
    );

    temp[sizeof(temp)-1]='\0';



    int len=strlen(temp);


    if(len==0)
    {
        return -1;
    }



    /*
     * 从第一个/后面开始扫描
     */
    for(int i=1;i<len;i++)
    {

        if(temp[i]=='/')
        {

            temp[i]='\0';


            /*
             * 创建当前目录
             */
            if(access(temp,F_OK)!=0)
            {
                if(mkdir(temp,0755)<0)
                {
                    if(errno!=EEXIST)
                    {
                        perror("mkdir");
                        return -1;
                    }
                }
            }


            temp[i]='/';

        }

    }


    /*
     * 创建最后一级目录
     */
    if(access(temp,F_OK)!=0)
    {
        if(mkdir(temp,0755)<0)
        {
            if(errno!=EEXIST)
            {
                perror("mkdir");
                return -1;
            }
        }
    }


    return 0;
}