#include "log_json.h"

#ifdef ENABLE_JSON_LOG

#include "path_manager.h"
#include "config.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <limits.h>

/* JSON 日志文件描述符 */
static int json_fd = -1;

/* 完整路径缓存 */
static char json_log_path[PATH_MAX];

/* 线程锁 */
static pthread_mutex_t json_mutex;

/* 初始化状态 */
static int json_initialized = 0;


/*
====================================

初始化 JSON 日志模块

路径来源:
config.json_path (config.ini 中 [log] json_path)

支持绝对路径与相对路径:
- 绝对路径: 直接使用
- 相对路径: 拼上程序根目录

====================================
*/




void log_json_init(void)
{
    char root_path[PATH_MAX];
    char log_dir[PATH_MAX];
    char full_dir[PATH_MAX];
    const char *log_path_src;
    int n;

    if(json_initialized)
        return;

    if(get_app_root(root_path, sizeof(root_path)) != 0)
        return;

    if(config.json_path[0] != '\0')
        log_path_src = config.json_path;
    else
        log_path_src = "logs/events.json.log";

    path_get_dir(
        (char *)log_path_src,
        log_dir,
        sizeof(log_dir)
    );

    /* 拼接绝对路径 */
    if(log_path_src[0] == '/')
    {
        n = snprintf(json_log_path, sizeof(json_log_path), "%s", log_path_src);
    }
    else
    {
        n = snprintf(json_log_path, sizeof(json_log_path), "%s/%s",
                     root_path, log_path_src);
    }

    if(n < 0 || n >= (int)sizeof(json_log_path))
    {
        LOG_ERROR("JSON log path too long");
        return;
    }

    /* 拼接并创建目录 */
    if(log_dir[0] == '/')
    {
        n = snprintf(full_dir, sizeof(full_dir), "%s", log_dir);
    }
    else
    {
        n = snprintf(full_dir, sizeof(full_dir), "%s/%s", root_path, log_dir);
    }

    if(n < 0 || n >= (int)sizeof(full_dir))
    {
        LOG_ERROR("JSON log dir path too long");
        return;
    }

    path_mkdir_recursive(full_dir);

    /* 打开文件 */
    json_fd = open(
        json_log_path,
        O_CREAT | O_WRONLY | O_APPEND,
        0666
    );

    if(json_fd < 0)
    {
        LOG_ERROR("JSON log open failed");
        return;
    }

    pthread_mutex_init(&json_mutex, NULL);
    json_initialized = 1;

    LOG_INFO("JSON log system init");
}


/*
====================================

记录 JSON 事件日志

写入格式（每行一条）:
{"ts":"2026-09-10T18:20:01","event":"xxx","ip":"x.x.x.x","fd":6,"detail":"xxx"}

并发保护:
- pthread_mutex_lock  线程级互斥
- flock              进程级互斥

====================================
*/
void log_event_json(const char *event,
                    const char *client_ip,
                    int fd,
                    const char *detail)
{
    char buffer[512];
    char time_str[32];
    time_t now;
    struct tm tm_info;
    int len;

    if(!json_initialized)
        return;

    /* 1. 获取 ISO8601 时间 */
    time(&now);
    localtime_r(&now, &tm_info);
    strftime(
        time_str,
        sizeof(time_str),
        "%Y-%m-%dT%H:%M:%S",
        &tm_info
    );

    /* 2. 格式化 JSON 字符串（一行一条） */
    len = snprintf(
        buffer,
        sizeof(buffer),
        "{\"ts\":\"%s\",\"event\":\"%s\",\"ip\":\"%s\",\"fd\":%d,\"detail\":\"%s\"}\n",
        time_str,
        event ? event : "",
        client_ip ? client_ip : "",
        fd,
        detail ? detail : ""
    );

    if(len <= 0 || len >= (int)sizeof(buffer))
        return;

    /* 3. 加锁 + 文件锁 + 写入 */
    pthread_mutex_lock(&json_mutex);

    flock(json_fd, LOCK_EX);

    write(json_fd, buffer, len);

    flock(json_fd, LOCK_UN);

    pthread_mutex_unlock(&json_mutex);
}


/*
====================================

关闭 JSON 日志模块

====================================
*/
void log_json_close(void)
{
    if(!json_initialized)
        return;

    LOG_INFO("JSON log system close");

    if(json_fd >= 0)
    {
        close(json_fd);
        json_fd = -1;
    }

    pthread_mutex_destroy(&json_mutex);

    json_initialized = 0;
}

#endif /* ENABLE_JSON_LOG */