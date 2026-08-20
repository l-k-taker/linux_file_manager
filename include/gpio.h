#ifndef GPIO_H
#define GPIO_H

/* ==================== 引脚方向 ==================== */
#define GPIO_DIR_IN    0
#define GPIO_DIR_OUT   1

/* ==================== 电平值 ==================== */
#define GPIO_LOW       0
#define GPIO_HIGH      1

/* ==================== 香橙派 Zero 3 (H616/H618) 引脚编号 ====================
 * sysfs 编号规则:
 *   PAx = x          (0~28)
 *   PBx = 32 + x     (32~44)
 *   PCx = 64 + x     (64~79)
 *   PDx = 96 + x     (96~119)
 *   PEx = 128 + x    (128~145)
 *   PFx = 160 + x    (160~166)
 *   PGx = 192 + x    (192~205)
 *   PHx = 224 + x    (224~237)
 *
 * 26pin 排针常用引脚:
 *   物理 Pin 7  = PC9  = 73
 *   物理 Pin 11 = PC6  = 70
 *   物理 Pin 12 = PC11 = 75
 *   物理 Pin 13 = PC10 = 74
 *   物理 Pin 15 = PC12 = 76
 *   物理 Pin 16 = PC13 = 77
 *   物理 Pin 18 = PC14 = 78
 *   物理 Pin 22 = PC15 = 79
 */
#define GPIO_PA0    0
#define GPIO_PA1    1
#define GPIO_PA2    2
#define GPIO_PA3    3
#define GPIO_PA4    4
#define GPIO_PA5    5
#define GPIO_PA6    6
#define GPIO_PA7    7
#define GPIO_PC6    70
#define GPIO_PC7    71
#define GPIO_PC8    72
#define GPIO_PC9    73
#define GPIO_PC10   74
#define GPIO_PC11   75
#define GPIO_PC12   76
#define GPIO_PC13   77
#define GPIO_PC14   78
#define GPIO_PC15   79

/* 启动指示灯默认引脚 (物理 Pin 7 = PC9) */
#define GPIO_BOOT_LED_PIN   GPIO_PC9

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化 GPIO 引脚
 * @param pin           sysfs 引脚编号 (如 GPIO_PC9=73)
 * @param direction     GPIO_DIR_IN 或 GPIO_DIR_OUT
 * @param initial_value 初始电平 (GPIO_HIGH 或 GPIO_LOW, 仅输出模式有效)
 * @return 0 成功, -1 失败
 */
int gpio_init(int pin, int direction, int initial_value);

/**
 * @brief 设置输出电平
 * @return 0 成功, -1 失败
 */
int gpio_set(int pin, int value);

/**
 * @brief 读取输入电平
 * @return GPIO_HIGH / GPIO_LOW, -1 失败
 */
int gpio_get(int pin);

/**
 * @brief 翻转输出电平
 * @return 0 成功, -1 失败
 */
int gpio_toggle(int pin);

/**
 * @brief 取消初始化引脚 (拉低并 unexport)
 * @return 0 成功, -1 失败
 */
int gpio_uninit(int pin);

/**
 * @brief 初始化启动指示灯 (PC9, 输出高电平)
 * @return 0 成功, -1 失败
 */
int gpio_boot_indicator_init(void);

/**
 * @brief 清理所有已初始化的引脚
 * @return 0 成功
 */
int gpio_cleanup(void);

#endif /* GPIO_H */
