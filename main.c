#include "log.h"
#include "menu.h"
#include "config.h"
#include "gpio.h"
#include <unistd.h>

int main()
{


    config_init();

    gpio_boot_indicator_init();
    sleep(1);                     // 亮1秒
    gpio_set(GPIO_PC9, GPIO_LOW);  // 熄灭
    sleep(1);                     // 灭1秒
    gpio_set(GPIO_PC9, GPIO_HIGH); // 再点亮保持


    log_init();


    LOG_INFO(
    "File Manager start"
    );



    menu_start();


    log_close();

    gpio_cleanup();

    return 0;

}

