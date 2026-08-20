#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "gpio.h"

/* ==================== sysfs 路径 ==================== */
#define GPIO_SYSFS_PATH        "/sys/class/gpio"
#define GPIO_EXPORT_PATH       GPIO_SYSFS_PATH "/export"
#define GPIO_UNEXPORT_PATH     GPIO_SYSFS_PATH "/unexport"
#define GPIO_VALUE_FMT         "/sys/class/gpio/gpio%d/value"
#define GPIO_DIRECTION_FMT     "/sys/class/gpio/gpio%d/direction"
#define GPIO_BASE_FMT          "/sys/class/gpio/gpio%d"

/* ==================== 配置参数 ==================== */
#define GPIO_MAX_PINS          16       /* 最大同时管理的引脚数 */
#define GPIO_EXPORT_DELAY_US   200000   /* export 后等待节点创建 (200ms) */
#define GPIO_DIRECTION_DELAY_US 200000  /* 原来是100000，改成200000 */
#define GPIO_VALUE_SET_DELAY_US  50000   /* 写电平后等待稳定 (50ms) */


/*
 * GPIO 模块实现 - 香橙派 Zero 3 (Allwinner H616)
 *
 * 使用 Linux sysfs 接口:
 *   /sys/class/gpio/export       - 导出引脚
 *   /sys/class/gpio/unexport     - 取消导出
 *   /sys/class/gpio/gpioN/direction
 *   /sys/class/gpio/gpioN/value
 *
 * Allwinner H616 引脚映射:
 *   PAx = gpiochip0, offset x  =>  sysfs = x
 *   PCx = gpiochip0, offset (64+x)
 */

#define GPIO_SYSFS_PATH       "/sys/class/gpio"
#define GPIO_EXPORT_PATH      GPIO_SYSFS_PATH "/export"
#define GPIO_UNEXPORT_PATH    GPIO_SYSFS_PATH "/unexport"
#define GPIO_VALUE_PATH       "/sys/class/gpio/gpio%d/value"
#define GPIO_DIRECTION_PATH   "/sys/class/gpio/gpio%d/direction"

/* 最大已初始化引脚数 */
#define GPIO_MAX_PINS         16

/* 已初始化引脚跟踪 */
static int gpio_initialized[GPIO_MAX_PINS] = {0};
static int gpio_init_count = 0;

/**
 * @brief 将引脚编号转换为 sysfs 编号
 * Allwinner H616: PAx = x, PCx = 64+x
 */
static int gpio_to_sysfs(int pin) {
    return pin;  /* 简化映射，PA7=7, PC4=68, PC5=69 */
}

/**
 * @brief 写入 sysfs 文件
 */
static int sysfs_write(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("[GPIO] open failed");
        return -1;
    }
    ssize_t len = write(fd, value, strlen(value));
    close(fd);
    if (len < 0) {
        perror("[GPIO] write failed");
        return -1;
    }
    return 0;
}

/**
 * @brief 读取 sysfs 文件到缓冲区
 */
static int sysfs_read(const char *path, char *buf, size_t len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("[GPIO] read open failed");
        return -1;
    }
    ssize_t n = read(fd, buf, len - 1);
    close(fd);
    if (n < 0) {
        perror("[GPIO] read failed");
        return -1;
    }
    buf[n] = '\0';
    return 0;
}

int gpio_init(int pin, int direction, int initial_value) {
    int sysfs_pin = gpio_to_sysfs(pin);
    char path[128];
    char num_str[8];
    char buf[24];
    /* 检查是否已初始化 */
    for (int i = 0; i < gpio_init_count; i++) {
        if (gpio_initialized[i] == pin) {
            fprintf(stderr, "[GPIO] Pin %d already initialized\n", pin);
            return -1;
        }
    }

    /* 导出引脚 */
    snprintf(num_str, sizeof(num_str), "%d", sysfs_pin);
    if (sysfs_write(GPIO_EXPORT_PATH, num_str) < 0) {
        /* 可能已经导出过，继续尝试 */
        if (errno != EBUSY) {
            fprintf(stderr, "[GPIO] Failed to export pin %d\n", pin);
            return -1;
        }
    }

    /* 短暂延迟等待 sysfs 节点创建 */
    usleep(100000);

    /* 设置方向 */
    snprintf(path, sizeof(path), GPIO_DIRECTION_PATH, sysfs_pin);
    if (sysfs_write(path, direction == GPIO_DIR_OUT ? "out\n" : "in\n") < 0) {
        fprintf(stderr, "[GPIO] Failed to set direction for pin %d\n", pin);
        return -1;
    }

    /* 3. 设置初始值 (仅输出模式) */
    if (direction == GPIO_DIR_OUT) {
        snprintf(path, sizeof(path), GPIO_VALUE_FMT, sysfs_pin);
        printf("[GPIO DEBUG] writing to: %s\n", path);  
        int target = (initial_value == GPIO_HIGH) ? 1 : 0;

        /* 第一轮：先写0，等内核direction重置完成 */
        snprintf(buf, sizeof(buf), "0\n");
        sysfs_write(path, buf);
        usleep(200000);  /* 200ms，给内核足够时间完成异步重置 */

        /* 第二轮：写目标值 */
        snprintf(buf, sizeof(buf), "%d\n", target);
        sysfs_write(path, buf);
        usleep(100000);  /* 100ms */

        /* 第一次验证 */
        char verify[8];
        sysfs_read(path, verify, sizeof(verify));
        int actual = (verify[0] == '1') ? 1 : 0;
        printf("[GPIO] 1st verify: pin=%d, expected=%d, got=%d\n", pin, target, actual);

        /* 双保险：如果第一次验证不对，再写一次 */
        if (actual != target) {
            printf("[GPIO] 1st verify mismatch, rewriting...\n");
            snprintf(buf, sizeof(buf), "%d\n", target);
            sysfs_write(path, buf);
            usleep(100000);
            sysfs_read(path, verify, sizeof(verify));
            actual = (verify[0] == '1') ? 1 : 0;
            printf("[GPIO] 2nd verify: pin=%d, expected=%d, got=%d\n", pin, target, actual);
        }

        if (actual != target) {
            fprintf(stderr, "[GPIO] Value verification FAILED: pin=%d, expected=%d, got=%d\n",
                    pin, target, actual);
            return -1;
        }

        printf("[GPIO] Pin %d initialized: direction=out, value=%d (stable)\n", pin, actual);
    }


    /* 记录已初始化 */
    if (gpio_init_count < GPIO_MAX_PINS) {
        gpio_initialized[gpio_init_count++] = pin;
    }

    printf("[GPIO] Pin %d (sysfs=%d) initialized: direction=%s, value=%d\n",
           pin, sysfs_pin, direction == GPIO_DIR_OUT ? "out" : "in", initial_value);
    return 0;
}

int gpio_set(int pin, int value) {
    int sysfs_pin = gpio_to_sysfs(pin);
    char path[128];
    char buf[8];

    snprintf(path, sizeof(path), GPIO_VALUE_FMT, sysfs_pin);
    snprintf(buf, sizeof(buf), "%d\n", value == GPIO_HIGH ? 1 : 0);
    if (sysfs_write(path, buf) < 0) {
        return -1;
    }

    /* 延迟后验证 */
    usleep(20000);  /* 20ms */
    char verify[8];
    if (sysfs_read(path, verify, sizeof(verify)) == 0) {
        int actual = (verify[0] == '1') ? 1 : 0;
        if (actual != (value == GPIO_HIGH ? 1 : 0)) {
            fprintf(stderr, "[GPIO] Set verification FAILED pin %d: expected %d, got %d\n",
                    pin, value == GPIO_HIGH ? 1 : 0, actual);
            return -1;
        }
    }
    return 0;
}


int gpio_get(int pin) {
    int sysfs_pin = gpio_to_sysfs(pin);
    char path[128];
    char buf[8];

    snprintf(path, sizeof(path), GPIO_VALUE_PATH, sysfs_pin);
    if (sysfs_read(path, buf, sizeof(buf)) < 0) {
        return -1;
    }
    return (buf[0] == '1') ? GPIO_HIGH : GPIO_LOW;
}

int gpio_toggle(int pin) {
    int current = gpio_get(pin);
    if (current < 0) {
        return -1;
    }
    return gpio_set(pin, !current);
}

int gpio_uninit(int pin) {
    int sysfs_pin = gpio_to_sysfs(pin);
    char num_str[8];
    char path[128];

    snprintf(num_str, sizeof(num_str), "%d", sysfs_pin);

    /* 先设置为低电平，确保 LED 熄灭 */
    snprintf(path, sizeof(path), GPIO_VALUE_PATH, sysfs_pin);
    sysfs_write(path, "0");

    /* 短暂延迟确保电平稳定 */
    usleep(10000);

    /* 从初始化列表中移除 */
    for (int i = 0; i < gpio_init_count; i++) {
        if (gpio_initialized[i] == pin) {
            /* 紧凑数组 */
            for (int j = i; j < gpio_init_count - 1; j++) {
                gpio_initialized[j] = gpio_initialized[j + 1];
            }
            gpio_init_count--;
            break;
        }
    }

    return sysfs_write(GPIO_UNEXPORT_PATH, num_str);
}

int gpio_boot_indicator_init(void) {
    /* 启动时点亮 PC9 指示灯 (物理 Pin 7) */
    int ret = gpio_init(GPIO_PC9, GPIO_DIR_OUT, GPIO_HIGH);
    if (ret == 0) {
        printf("[GPIO] Boot indicator LED ON (PC9)\n");
    }
    return ret;
}

int gpio_cleanup(void) {
    /* 逆序释放所有已初始化引脚 */
    for (int i = gpio_init_count - 1; i >= 0; i--) {
        gpio_uninit(gpio_initialized[i]);
    }
    gpio_init_count = 0;
    memset(gpio_initialized, 0, sizeof(gpio_initialized));
    printf("[GPIO] All pins cleaned up\n");
    return 0;
}
