#ifndef LOG_H
#define LOG_H


#define LOG_INFO(msg) \
    log_write("INFO",msg)


#define LOG_ERROR(msg) \
    log_write("ERROR",msg)

#define LOG_WARN(msg) \
    log_write("WARN",msg) \


void log_init();


void log_redirect();


void log_write(char *level,char *msg);


void log_close();

void log_view();


#endif
