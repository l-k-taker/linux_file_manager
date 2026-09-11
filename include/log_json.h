#ifndef LOG_JSON_H
#define LOG_JSON_H

/*
====================================

JSON 事件日志模块（可插拔）

配合编译宏 ENABLE_JSON_LOG 使用:

- 已定义: LOG_EVENT 转发到 log_event_json
- 未定义: LOG_EVENT 退化为空操作

路径来源:
config.ini 中 [log] json_path 项

====================================
*/

#ifdef ENABLE_JSON_LOG

/* 初始化 / 关闭 */
void log_json_init(void);
void log_json_close(void);

/* 核心写入接口 */
void log_event_json(const char *event,
                    const char *client_ip,
                    int fd,
                    const char *detail);

/* 统一调用宏 */
#define LOG_EVENT(event, ip, fd, detail) \
    log_event_json(event, ip, fd, detail)

#define LOG_JSON_INIT()  log_json_init()
#define LOG_JSON_CLOSE() log_json_close()

#else

/* 未启用: 空操作，避免未使用变量警告 */
#define LOG_EVENT(event, ip, fd, detail) \
    do { \
        (void)(event); \
        (void)(ip); \
        (void)(fd); \
        (void)(detail); \
    } while(0)

#define LOG_JSON_INIT()  do { } while(0)
#define LOG_JSON_CLOSE() do { } while(0)

#endif /* ENABLE_JSON_LOG */

#endif /* LOG_JSON_H */