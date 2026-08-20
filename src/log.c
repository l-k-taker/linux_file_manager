#include "log.h"
#include "config.h"
#include "path_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>

static pthread_mutex_t log_mutex;

static char real_log_path[512];
/*
    Linux文件描述符
    -1表示未打开
*/
static int log_fd = -1;

/*
    初始化日志系统
*/
void log_init()
{
    char root_path[512];
    
    char log_dir[512];


    /*
        获取程序目录
    */
    if(get_app_root(
        root_path,
        sizeof(root_path)
    ) != 0)
    {
        exit(1);
    }



    /*
        拼接真实日志路径
    */
    path_join(
        real_log_path,
        sizeof(real_log_path),
        root_path,
        config.log_path  //配置文件的使用
    );



    /*
        获取日志目录
    */
    path_get_dir(
        real_log_path,
        log_dir,
        sizeof(log_dir)
    );



    /*
        创建目录
    */
    path_mkdir_recursive(
        log_dir
    );



    /*
        打开日志文件
    */
    log_fd=open(
        real_log_path,
        O_CREAT|O_WRONLY|O_APPEND,
        0666
    );

    if(log_fd<0)
    {
        perror("open log");
        exit(1);
    }


    pthread_mutex_init(
        &log_mutex,
        NULL
    );


    LOG_INFO(
        "Log system init"
    );
}
/*
    stdout重定向
    fd 1 ---> log_fd
*/
void log_redirect()
{
    if (log_fd < 0)
    {
        return;
    }

    dup2(
        log_fd,
        STDOUT_FILENO
    );
}

/*
    写日志
*/
void log_write(char *level, char *msg)
{

    char buffer[1024];


    /*
        获取时间
    */

    time_t now;

    time(&now);


    struct tm tm_info;


    localtime_r(
        &now,
        &tm_info
    );


    char time_str[64];


    strftime(
        time_str,
        sizeof(time_str),
        "%Y-%m-%d %H:%M:%S",
        &tm_info
    );



    /*
        获取PID
    */

    pid_t pid = getpid();



    /*
        获取线程ID

        pthread_self返回pthread_t类型
    */

    pthread_t tid = pthread_self();



    snprintf(
        buffer,
        sizeof(buffer),

        "[%s][%s][PID:%d][TID:%lu]%s\n",

        time_str,

        level,

        pid,

        (unsigned long)tid,

        msg
    );



    pthread_mutex_lock(
        &log_mutex
    );



    /*
        文件锁

        防止多进程同时写
    */

    flock(
        log_fd,
        LOCK_EX
    );


    write(
        log_fd,
        buffer,
        strlen(buffer)
    );


    flock(
        log_fd,
        LOCK_UN
    );



    pthread_mutex_unlock(
        &log_mutex
    );

}

/*
    关闭日志
*/
void log_close()
{
    if (log_fd >= 0)
    {
        LOG_INFO("Log system close");

        close(log_fd);

        pthread_mutex_destroy(&log_mutex);

        log_fd = -1;
    }
}

void log_view()
{
    pthread_mutex_lock(&log_mutex);

    int fd;
    char buffer[512];
    ssize_t bytes_read;

    fd = open(
        real_log_path,
        O_RDONLY
    );

    if (fd < 0)
    {
        perror("open log");
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    // 读取并打印日志内容
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0';  // 添加字符串结束符
        printf("%s", buffer);
    }

    close(fd);

    pthread_mutex_unlock(&log_mutex);
}
