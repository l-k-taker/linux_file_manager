#include "config.h"
#include "../third_reporty/inih/ini.h"
#include "path_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
    全局配置变量

    其他模块通过extern访问
*/
Config config;


void config_init(void){

        char root[512];

get_app_root(
    root,
    sizeof(root)
);


char config_path[512];


path_join(
    config_path,
    sizeof(config_path),
    root,
    "config.ini"
);


config_load(config_path);

}


/*
    inih解析回调函数

    ini文件每读取一项，
    就会调用一次这个函数


*/
static int config_handler(
        void *user,
        const char *section,
        const char *name,
        const char *value
)
{

    /*
        user就是ini_parse传进来的
        &config

        转换成Config结构体指针
    */
    Config *cfg =(Config *)user;


    /*
        判断section

        对应:

        [log]
    */
    if(strcmp(section,"log")==0)
    {

        /*
            判断key

            对应:

            path=xxx
        */
        if(strcmp(name,"path")==0)
        {

            strncpy(
                cfg->log_path,
                value,
                sizeof(cfg->log_path)-1
            );

        }

    }
    else if(strcmp(section,"thread_pool")==0)
{

    if(strcmp(name,"task_num")==0)
    {

        config.task_num =
        atoi(value);

    }


    else if(strcmp(name,"thread_max")==0)
    {

        config.thread_max=atoi(value);

    }

    else if(strcmp(name,"thread_min")==0){

        config.thread_min=atoi(value);
    }

}




    /*
        返回1表示解析成功

        返回0表示忽略这一项
    */
    return 1;

}


void config_print()
{

    printf("\n");
    printf("========== Current Configuration ==========\n");


    printf(
        "Log path        : %s\n",
        config.log_path
    );


    printf(
        "Task number     : %d\n",
        config.task_num
    );


    printf(
        "Thread max      : %d\n",
        config.thread_max
    );

    printf(
        "Thread min      : %d\n",
        config.thread_min
    );

    printf(
        "===========================================\n"
    );


}

/*
    加载配置文件

*/
int config_load(char *filename)
{


    /*
        初始化结构体

        防止里面有垃圾数据
    */
    memset(
        &config,
        0,
        sizeof(Config)
    );

    /*
        调用第三方库inih


        参数1:
        配置文件路径


        参数2:
        回调函数


        参数3:
        用户数据
    */
    if(
        ini_parse(
            filename,
            config_handler,
            &config
        ) < 0
    )
    {

        printf(
            "cannot load config file\n"
        );


        return -1;

    }


    config_print();

    
    return 0;

}