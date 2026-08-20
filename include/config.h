#ifndef CONFIG_H
#define CONFIG_H


typedef struct
{

    char log_path[128];

        /*
        thread pool
    */

    int task_num;

    int thread_max;

    int thread_min;

}Config;



extern Config config;

void config_init(void);

int config_load(
    char *filename
);

void config_print();

#endif