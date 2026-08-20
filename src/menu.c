#include "menu.h"

#include "file_manager.h"
#include "directory.h"
#include "common.h"
#include "log.h"
#include "config.h"
#include "gateway.h"
#include "protocol.h"
#include "thread_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

extern Config config;
#include <time.h>

/* ==================== 网关服务器管理界面 ==================== */

/* 全局网关上下文 (供 handler 访问) */
static GatewayCtx *g_gateway_ctx = NULL;
static struct Threadpool *g_server_pool = NULL;

/* 消息处理回调 */
static void handle_sensor_data(const GatewayDevice *dev,
                               uint8_t cmd, uint16_t seq,
                               const uint8_t *payload, uint16_t payload_len,
                               void *user_data) {
    printf("\n[SENSOR] Data from %s:%d (dev_id=%s)\n", dev->ip, dev->port, dev->dev_id);
    if (payload_len > 0) {
        printf("  Payload (%d bytes): ", payload_len);
        for (int i = 0; i < payload_len && i < 16; i++) {
            printf("%02X ", payload[i]);
        }
        printf("\n");
    }
    uint8_t ack[] = {0x01};
    gateway_send_response(g_gateway_ctx, dev->fd, CMD_SENSOR_DATA, FLAG_RESPONSE,
                         seq, ack, sizeof(ack));
    printf("  [SENSOR] ACK sent\n");
}

static void handle_register(const GatewayDevice *dev,
                            uint8_t cmd, uint16_t seq,
                            const uint8_t *payload, uint16_t payload_len,
                            void *user_data) {
    printf("\n[REGISTER] Request from %s:%d\n", dev->ip, dev->port);
    if (payload_len > 0 && payload_len < sizeof(dev->dev_id)) {
        memcpy((char*)dev->dev_id, payload, payload_len);
        ((char*)dev->dev_id)[payload_len] = '\0';
        printf("  Device ID: %s\n", dev->dev_id);
    }
    GatewayDevice *mutable_dev = (GatewayDevice *)dev;
    if (payload_len >= 2) {
        if (payload[0] == 'S' && payload[1] == 'T') {
            mutable_dev->dev_type = DEV_TYPE_STM32;
            printf("  Type: STM32\n");
        } else if (payload[0] == 'E' && payload[1] == 'S') {
            mutable_dev->dev_type = DEV_TYPE_ESP8266;
            printf("  Type: ESP8266\n");
        }
    }
    uint8_t resp[] = {0x00, 0x01};
    gateway_send_response(g_gateway_ctx, dev->fd, CMD_STM32_REGISTER, FLAG_RESPONSE,
                         seq, resp, sizeof(resp));
    printf("  [REGISTER] Registration response sent\n");
}

static void handle_gpio_control(const GatewayDevice *dev,
                                uint8_t cmd, uint16_t seq,
                                const uint8_t *payload, uint16_t payload_len,
                                void *user_data) {
    printf("\n[GPIO] Control command from %s\n", dev->ip);
    if (payload_len >= 2) {
        printf("  Pin: %d, Value: %d\n", payload[0], payload[1]);
    }
    uint8_t ack[] = {0x01};
    gateway_send_response(g_gateway_ctx, dev->fd, CMD_GPIO_CONTROL, FLAG_RESPONSE,
                         seq, ack, sizeof(ack));
}

/* 网关服务器管理界面主函数 */
void gateway_server_menu(void) {
    print_line();
    printf("  Starting Gateway Server...\n");
    print_line();

    /* 从配置文件读取线程池参数 */
    int pool_min = config.thread_min > 0 ? config.thread_min : 2;
    int pool_max = config.thread_max > pool_min ? config.thread_max : pool_min + 1;
    int pool_queue = config.task_num > 0 ? config.task_num : 100;

    /* 初始化线程池 */
    g_server_pool = Thread_init(pool_min, pool_max, pool_queue);
    if (!g_server_pool) {
        print_error("Failed to create thread pool");
        wait_enter();
        return;
    }
    printf("[GW] Thread pool created (min=%d, max=%d, queue=%d)\n", pool_min, pool_max, pool_queue);

    /* 初始化网关 */
    GatewayCtx *gw = gateway_init(6789);
    if (!gw) {
        print_error("Failed to initialize gateway");
        if (g_server_pool) threadPoolDestory(g_server_pool);
        g_server_pool = NULL;
        wait_enter();
        return;
    }

    g_gateway_ctx = gw;

    /* 注册 handler */
    gateway_register_handler(gw, CMD_SENSOR_DATA, handle_sensor_data, gw);
    gateway_register_handler(gw, CMD_STM32_REGISTER, handle_register, gw);
    gateway_register_handler(gw, CMD_GPIO_CONTROL, handle_gpio_control, gw);

    /* 运行网关交互模式 (阻塞直到用户按 q) */
    gateway_run_interactive(gw, g_server_pool);

    /* 停止网关（关闭线程和 listen_fd） */
    gateway_stop(gw);

    /* 清理 */
    if (g_server_pool) {
        threadPoolDestory(g_server_pool);
        g_server_pool = NULL;
    }
    gateway_cleanup(gw);
    g_gateway_ctx = NULL;
}

/* ==================== 原有函数 ==================== */

void print_file_info(
struct stat *st,
char *path
)
{

    printf(
    "\n========== File Information ==========\n"
    );


    printf(
    "File path      : %s\n",
    path
    );


    printf(
    "File size      : %ld bytes\n",
    st->st_size
    );


    printf(
    "File mode      : %o\n",
    st->st_mode & 0777
    );


    printf(
    "Hard links     : %ld\n",
    st->st_nlink
    );


    printf(
    "Owner UID      : %d\n",
    st->st_uid
    );


    printf(
    "Owner GID      : %d\n",
    st->st_gid
    );


    printf(
    "Last access    : %s",
    ctime(
        &st->st_atime
    )
    );


    printf(
    "Last modify    : %s",
    ctime(
        &st->st_mtime
    )
    );


    printf(
    "Last change    : %s",
    ctime(
        &st->st_ctime
    )
    );


    /*
        文件类型判断
    */
    printf(
    "File type      : "
    );


    if(S_ISREG(st->st_mode))
    {
        printf(
        "Regular file\n"
        );
    }
    else if(S_ISDIR(st->st_mode))
    {
        printf(
        "Directory\n"
        );
    }
    else if(S_ISLNK(st->st_mode))
    {
        printf(
        "Symbolic link\n"
        );
    }
    else
    {
        printf(
        "Other\n"
        );
    }


    print_line();

}

static void show_menu()
{


    print_line();


    printf(
    "        Linux File Manager\n"
    );


    print_line();



    printf(
    "1. Create File\n"
    );

    printf(
    "2. Delete File\n"
    );

    printf(
    "3. Write File\n"
    );

    printf(
    "4. Read File\n"
    );

    printf(
    "5. Copy File\n"
    );

    printf(
    "6. File Information\n"
    );


    printf(
    "7. List Directory\n"
    );

    printf(
    "8. Create Directory\n"
    );

    printf(
    "9. Delete Directory\n"
    );

    printf("10. View Log\n");
    
    printf("11. Start Server\n");
    
    printf(
    "0. Exit\n"
    );


    print_line();


    printf(
    "Input:"
    );


}




void menu_start()
{

    int choice;


    char path[256];

    char path2[256];


    char buffer[4096];



    while(1)
    {


        show_menu();



        scanf(
            "%d",
            &choice
        );


        clear_input_buffer();



        memset(
            buffer,
            0,
            sizeof(buffer)
        );



        switch(choice)
        {


        /*
            创建文件
        */
        case 1:


            printf(
            "File path:"
            );


            get_string(
                path,
                sizeof(path)
            );



            if(
            fm_create(path)==0
            )
            {

                
                print_success(
                "Create file success"
                );

            }
            else
            {


                if(errno == ENOENT)
    {
        printf(
        "[ERROR] Directory does not exist, please create directory first\n"
        );
    }

    else if(errno == EACCES)
    {
        printf(
        "[ERROR] Permission denied\n"
        );
    }

    else
    {
        perror("create file");
    }    

                print_error(
                "Create file failed"
                );

            }


            wait_enter();


            break;





        /*
            删除文件
        */
        case 2:


            printf(
            "File path:"
            );


            get_string(
                path,
                sizeof(path)
            );



            if(
            fm_delete(path)==0
            )
            {

                print_success(
                "Delete file success"
                );

            }
            else
            {

                print_error(
                "Delete file failed"
                );

            }



            wait_enter();


            break;





        /*
            写文件
        */
        case 3:



            printf(
            "File path:"
            );


            get_string(
                path,
                sizeof(path)
            );


            printf(
            "Content:"
            );


            get_string(
                buffer,
                sizeof(buffer)
            );



            if(
            fm_write(
                path,
                buffer
            )==0
            )
            {

                print_success(
                "Write file success"
                );

            }
            else
            {


                    if(errno == ENOENT)
    {
        print_error(
        "File does not exist"
        );
    }
    else if(errno == EACCES)
    {
        print_error(
        "Permission denied"
        );
    }
    else if(errno == EISDIR)
    {
        print_error(
        "Target is a directory"
        );
    }
    else
    {
        print_error(
        "Write file failed"
        );
    }
                print_error(
                "Write file failed"
                );

            }


            wait_enter();


            break;





        /*
            读文件
        */
        case 4:


            printf(
            "File path:"
            );


            get_string(
                path,
                sizeof(path)
            );



            memset(
            buffer,
            0,
            sizeof(buffer)
            );



            if(
            fm_read(
                path,
                buffer,
                sizeof(buffer)
            )>0
            )
            {

                printf(
                "\n%s\n",
                buffer
                );

            }
            else
            {


                if(errno == ENOENT)
{
    print_error(
        "File does not exist"
    );
}
else if(errno == EACCES)
{
    print_error(
        "Permission denied"
    );
}
else if(errno == EISDIR)
{
    print_error(
        "Target is a directory"
    );
}
else
{
    print_error(
        "Read file failed"
    );
}

                print_error(
                "Read file failed"
                );

            }


            wait_enter();


            break;






/*
            文件复制
        */
            case 5:
                printf("Source: ");
                get_string(path, sizeof(path));

                printf("Destination: ");
                get_string(path2, sizeof(path2));

    // ✅ 正确的判断逻辑：返回 0 表示成功
                if(fm_copy(path, path2) == 0) {
                    print_success("Copy completed successfully");
                } else {
        // 根据 errno 给出具体错误信息
                 if(errno == ENOENT) {
                        print_error("Source file does not exist");
                    } else if(errno == EACCES) {
                        print_error("Permission denied");
                    } else {
                        print_error("Copy failed");
                    }
                }

                wait_enter();
                break;




        /*
            文件信息
        */
        case 6:

    printf(
    "File path:"
    );


    get_string(
        path,
        sizeof(path)
    );


    struct stat st;


    if(fm_info(path,&st)==0)
    {

        print_file_info(
            &st,
            path
        );


        print_success(
        "Get information success"
        );

    }
    else
    {

        if(errno == ENOENT)
        {
            print_error(
            "File does not exist"
            );
        }
        else if(errno == EACCES)
        {
            print_error(
            "Permission denied"
            );
        }
        else
        {
            print_error(
            "Get information failed"
            );
        }

    }


    wait_enter();

    break;





        /*
            查看目录
        */
        case 7:



            printf(
            "Directory:"
            );


            get_string(
                path,
                sizeof(path)
            );



            memset(
            buffer,
            0,
            sizeof(buffer)
            );



           if(
dir_list(
    path,
    buffer,
    sizeof(buffer)
)==0
)
{

    printf(
    "\n%s",
    buffer
    );

}
else
{

    if(errno == ENOENT)
    {

        print_error(
        "Directory does not exist"
        );

    }
    else if(errno == EACCES)
    {

        print_error(
        "Permission denied"
        );

    }
    else if(errno == ENOTDIR)
    {

        print_error(
        "Path is not a directory"
        );

    }
    else
    {

        print_error(
        "List directory failed"
        );

    }

}

            wait_enter();


            break;





        /*
            创建目录
        */
        case 8:


            printf(
            "Directory:"
            );


            get_string(
                path,
                sizeof(path)
            );


if(dir_create(path)==0)
{
    print_success(
    "Create directory success"
    );
}
else
{

    if(errno == EEXIST)
    {
        print_error(
        "Directory already exists"
        );
    }
    else if(errno == EACCES)
    {
        print_error(
        "Permission denied"
        );
    }
    else
    {
        print_error(
        "Create directory failed"
        );
    }

}


            wait_enter();


            break;






        /*
            删除目录
        */
        case 9:



            printf(
            "Directory:"
            );


            get_string(
                path,
                sizeof(path)
            );



if(
dir_delete(path)==0
)
{

    print_success(
    "Delete directory success"
    );

}
else
{

    if(errno == ENOTEMPTY)
    {

        print_error(
        "Directory is not empty"
        );

    }
    else if(errno == ENOENT)
    {

        print_error(
        "Directory does not exist"
        );

    }
    else if(errno == EACCES)
    {

        print_error(
        "Permission denied"
        );

    }
    else
    {

        print_error(
        "Delete directory failed"
        );

    }

}


            wait_enter();


            break;

	case 10:

    		log_view();

    		wait_enter();

    		break;

        case 11:  /* Start Gateway Server */
            gateway_server_menu();
            break;

        case 0:


            LOG_INFO(
            "User exit system"
            );


            return;




        default:


            print_error(
            "Invalid choice"
            );


            wait_enter();


            break;


        }


    }


}
