# Linux File Manager — 基于ARM Linux的模块化文件管理与智能网关系统

一个面向嵌入式Linux平台的C语言工程项目，以文件管理为核心载体，串联起Linux系统编程、POSIX多线程、线程池、网络通信、日志系统、配置管理等多个技术模块，并支持x86与ARM64双平台编译部署。

项目最初用于Linux系统编程学习与C语言工程化实践，目前已演进为具备智能网关能力的嵌入式应用框架，支持STM32+ESP8266设备通过TCP接入、二进制协议通信、GPIO控制、心跳保活等功能。

---

## 1. 系统架构

```
──────────────────────────────────────────────────────────────
                        应用层 (main.c)
                   配置初始化 → 日志初始化 → 菜单交互
──────────────────────────────────────────────────────────────
  文件管理模块     目录管理模块      路径管理模块    智能网关模块
  file_manager    directory      path_manager   ┌─ GPIO 模块
                                                │  (sysfs GPIO控制)
                                                ├─ Protocol 模块
                                                │  (二进制帧协议)
                                                └─ Gateway 模块
                                                   (TCP服务器+心跳+回调分发)
──────────────────────────────────────────────────────────────
  线程池模块 (thread_pool)   日志模块 (log)    配置模块 (config)
  动态扩容/缩容/任务调度       线程安全/文件锁    inih解析/运行时配置
──────────────────────────────────────────────────────────────
              Linux / POSIX 系统调用层 (open/read/write/...)
──────────────────────────────────────────────────────────────
  x86_64 (Ubuntu 开发环境)   │   ARM64 (Orange Pi Zero 3 部署)
──────────────────────────────────────────────────────────────
  STM32F103 + ESP8266 客户端  →  TCP 6789 →  网关服务器
──────────────────────────────────────────────────────────────
```

---

## 2. 技术栈

| 类别 | 技术 |
|------|------|
| 开发语言 | C (C99) |
| 系统接口 | Linux/POSIX 系统调用 (open, read, write, close, stat, opendir 等) |
| 多线程 | POSIX Threads (pthread)、互斥锁、条件变量 |
| 并发模型 | 线程池（动态扩容缩容 + 管理者线程 + 生产者-消费者模型） |
| 网络通信 | TCP Socket、多客户端并发接入、线程池任务分发 |
| 日志系统 | 文件描述符、write、dup2 标准输出重定向、flock 文件锁 |
| 配置管理 | inih 第三方库解析 INI 配置文件，运行时可配 |
| 构建工具 | GNU Make（支持 x86 与 ARM64 交叉编译双目标） |
| 版本控制 | Git |
| 目标平台 | x86_64 / ARM64 (AArch64) |
| 硬件平台 | Orange Pi Zero 3 (Allwinner H616) |
---

## 3. 目录结构

```
linux_file_manager/
├── app/                    # 运行时目录
│   └── config.ini          # 主配置文件（日志路径、线程池参数）
├── include/                # 头文件
│   ├── common.h            # 通用工具函数声明
│   ├── config.h            # 配置管理模块
│   ├── directory.h         # 目录操作模块
│   ├── file_manager.h      # 文件管理模块
│   ├── gateway.h           # 智能网关模块（TCP服务器+心跳+回调分发）
│   ├── gpio.h              # GPIO 控制模块（sysfs 接口）
│   ├── log.h               # 日志系统模块
│   ├── menu.h              # 命令行菜单交互
│   ├── path_manager.h      # 路径管理模块
│   ├── protocol.h          # 通信协议模块（二进制帧协议）
│   └── thread_pool.h       # 线程池模块
├── src/                    # 源码实现
│   ├── common.c
│   ├── config.c
│   ├── directory.c
│   ├── file_manager.c
│   ├── gateway.c           # 网关服务器实现（替代原 server.c）
│   ├── gpio.c              # GPIO 实现
│   ├── log.c
│   ├── menu.c
│   ├── path_manager.c
│   ├── protocol.c          # 协议帧构建与解析
│   └── thread_pool.c
├── third_reporty/          # 第三方依赖
│   └── inih/               # INI 配置文件解析库
│       ├── ini.c
│       └── ini.h
├── main.c                  # 程序入口
├── Makefile                # 构建脚本（x86 / ARM64 双目标）
├── .gitignore
└── README.md
```

---

## 4. 核心模块详解

### 4.1 文件管理模块 (`file_manager.c`)

基于 Linux 系统调用封装文件操作接口，提供统一的文件管理能力：

- **文件创建/删除**：`open()` + `O_CREAT` 创建，`unlink()` 删除
- **文件读写**：`read()` / `write()` 基于文件描述符的二进制读写
- **文件复制**：源文件读取 → 缓冲区中转 → 目标文件写入，支持大文件分块复制
- **文件信息查看**：`stat()` 获取文件大小、权限、修改时间等元数据

### 4.2 目录管理模块 (`directory.c`)

- **目录列表**：`opendir()` / `readdir()` / `closedir()` 遍历目录条目
- **目录创建/删除**：`mkdir()` / `rmdir()`
- 与路径管理模块配合，支持递归目录创建

### 4.3 路径管理模块 (`path_manager.c`)

解决程序在不同工作目录下运行时的路径问题：

- **程序根目录获取**：通过 `/proc/self/exe` 符号链接读取程序所在目录
- **路径拼接**：`path_join()` 安全拼接目录与文件名，自动处理斜杠
- **目录提取**：`path_get_dir()` 从完整路径中提取目录部分
- **递归创建目录**：`path_mkdir_recursive()` 逐级创建不存在的目录

### 4.4 线程池模块 (`thread_pool.c`)

项目核心并发组件，实现了一个具备动态管理能力的线程池：

**数据结构：**
- 环形任务队列（数组实现，`queueFront` / `queueRear` 指针循环）
- 工作线程ID数组（动态分配，最大容量 `maxNUM`）
- 管理者线程（独立线程，定期检测负载并调整线程数）

**核心机制：**
- **生产者-消费者模型**：任务入队时 `pthread_cond_signal(&notEmpty)` 唤醒工作线程；队列满时生产者阻塞在 `notFull`
- **动态扩容**：管理者线程每3秒检测一次，当任务队列长度 > 空闲线程数且存活线程数 < 最大值时，每次新增2个工作线程
- **动态缩容**：当忙碌线程数 × 2 < 存活线程数且存活线程数 > 最小值时，设置 `exitNUM`，工作线程被唤醒后检测到该标志自行退出
- **双锁设计**：`mutexPool` 保护线程池整体状态，`mutexBusy` 保护忙碌线程计数，减少锁竞争

**对外接口：**
```c
struct Threadpool* Thread_init(int min, int max, int queuesize);
void threadPoolAdd(struct Threadpool* pool, void(*function)(void*), void* arg);
int threadPoolBusyNum(struct Threadpool* pool);
int threadPoolLiveNum(struct Threadpool* pool);
int threadPoolDestory(struct Threadpool* pool);
```

### 4.5 日志系统模块 (`log.c`)

线程安全的日志系统，支持多线程/多进程环境下的可靠写入：

- **日志格式**：`[时间][级别][PID:进程号][TID:线程号]日志内容`
- **线程安全**：`pthread_mutex` 互斥锁保证多线程串行写入
- **多进程安全**：`flock()` 文件锁防止多进程同时写同一日志文件
- **标准输出重定向**：`log_redirect()` 通过 `dup2(log_fd, STDOUT_FILENO)` 将标准输出重定向到日志文件
- **自动目录创建**：初始化时根据配置路径自动创建日志目录
- **日志查看**：`log_view()` 读取并打印日志文件内容

**日志级别：** `LOG_INFO` / `LOG_WARN` / `LOG_ERROR`（宏封装，自动传入级别字符串）

### 4.6 配置管理模块 (`config.c`)

基于 inih 库实现 INI 配置文件解析，支持运行时参数配置：

**当前配置项：**
```ini
[log]
path=logs/system.log          ; 日志文件路径（相对程序根目录）

[thread_pool]
task_num=100                  ; 任务队列容量
thread_max=2                  ; 最大工作线程数
thread_min=1                  ; 最小工作线程数
```

**实现要点：**
- `config_handler()` 回调函数按 section + name 匹配配置项，写入全局 `Config` 结构体
- 其他模块通过 `extern Config config;` 直接访问配置
- 配置加载失败时打印错误并返回 -1

### 4.7 GPIO 控制模块 (`gpio.c`)

基于 Linux sysfs 接口 (`/sys/class/gpio/`) 封装 GPIO 操作，用于 Orange Pi Zero 3 (Allwinner H616)：

**引脚映射：**
| 宏定义 | 引脚 | sysfs 编号 | 用途 |
|--------|------|-----------|------|
| `GPIO_PC9` | PC9 | 73 | 启动指示灯 (默认) |
| `GPIO_PC4` | PC4 | 68 | 预留 |
| `GPIO_PC5` | PC5 | 69 | 预留 |

**核心功能：**
- `gpio_init(pin, direction, initial_value)` — 导出引脚、设置方向、初始值
- `gpio_set(pin, value)` / `gpio_get(pin)` — 设置/读取电平
- `gpio_toggle(pin)` — 翻转电平
- `gpio_boot_indicator_init()` — 启动时点亮 PC9 指示灯
- `gpio_cleanup()` — 逆序释放所有已初始化引脚

### 4.8 通信协议模块 (`protocol.c`)

实现简单二进制帧协议，用于网关与 STM32+ESP8266 设备之间的通信：

**帧格式：**
```
[MAGIC(2B)] [CMD(1B)] [FLAGS(1B)] [SEQ(2B)] [PAYLOAD_LEN(2B)] [PAYLOAD(NB)] [CHECKSUM(1B)]
  0xAA 0x55    命令字     标志位      序列号       载荷长度        载荷数据       异或校验
```

**命令字定义：**
| 命令字 | 名称 | 说明 |
|--------|------|------|
| `0x01` | PING/PONG | 心跳请求/响应 (FLAGS=0x02) |
| `0x10` | STM32_REGISTER | 设备注册请求 |
| `0x11` | SENSOR_DATA | 传感器数据上报 |
| `0x20` | GPIO_CONTROL | GPIO 控制命令 |
| `0x21` | GPIO_QUERY | GPIO 状态查询 |
| `0x30` | FW_INFO | 固件信息查询 |
| `0xF0` | ERROR | 错误响应 |

**标志位：**
| 值 | 含义 |
|----|------|
| `0x00` | 请求 |
| `0x01` | 响应 |
| `0x02` | 心跳 |

**核心函数：**
- `proto_build_frame()` — 构建完整协议帧（魔数+载荷+校验和）
- `proto_parse_frame()` — 从字节流中解析帧（支持粘包/半包处理）
- `proto_checksum()` — 异或校验计算

### 4.9 智能网关模块 (`gateway.c`)

替代原 `server.c` 的粗糙回显逻辑，实现完整的 TCP 网关服务器：

**架构设计（三线程模型）：**
- **Accept 线程**：阻塞在 `accept()`，接收新连接 → 添加设备记录 → 投递到线程池
- **Client Worker**：从线程池获取 → select 轮询 → 协议帧解析 → 回调分发
- **Heartbeat 线程**：每 10 秒检测设备活跃度 → 空闲超 10 秒发 PING → 超 30 秒且 3 次无响应则踢除

**核心功能：**
- 端口 6789 监听（`SO_REUSEADDR` 端口复用）
- 最多 32 个并发客户端连接管理
- 回调驱动的消息分发机制（替代硬编码 echo）
- 自动心跳保活 + 超时断开
- 设备注册、类型识别（STM32/ESP8266）、状态追踪
- 运行时命令交互界面：`q` 退出、`s` 状态、`h` 帮助、`l` 设备列表、`r` 重启LED命令

**网关服务器交互界面：**
```
gateway> h
===========================================
        Gateway Server Commands
===========================================
  q  - Quit gateway server and return to menu
  s  - Show current server status
  h  - Show this help message
  l  - List all connected devices
  r  - Send restart LED command to all devices
===========================================
```

**状态查询输出：**
```
╔═══════════════════════════════════════════════════════╗
║              GATEWAY DEVICE STATUS                   ║
╠═══════════════════════════════════════════════════════╣
║  Port: 6789                                      ║
║  Devices: 1/32                                  ║
╠═══════════════════════════════════════════════════════╣
║  [1] 192.168.1.100:54321 STM32   ID=STM32_NODE_01 ║
╚═══════════════════════════════════════════════════════╝
```

**与原 server.c 的对比：**
| 原 server.c | 新 gateway 模块 |
|-------------|----------------|
| 简单 recv/send 回显 | 协议帧解析 + 回调分发 |
| 无心跳保活 | 自动心跳检测 + 超时踢除 |
| 无设备管理 | 设备注册 + 状态追踪 |
| 硬编码通信逻辑 | 回调驱动，业务与通信解耦 |
| 无交互界面 | q/s/h/l/r 完整管理界面 |
| 独立线程 | 集成项目线程池 |

### 4.10 菜单交互模块 (`menu.c`)

命令行交互式菜单，提供文件管理、目录管理、日志查看、网关服务器管理等功能入口。

选项 11 进入网关服务器管理界面，提供完整的 q/s/h 命令交互能力。

---

## 5. 配置文件说明

配置文件位于 `app/config.ini`，程序启动时自动加载：

| Section | Key | 默认值 | 说明 |
|---------|-----|--------|------|
| `[log]` | `path` | `logs/system.log` | 日志文件相对路径 |
| `[thread_pool]` | `task_num` | `100` | 任务队列最大容量 |
| `[thread_pool]` | `thread_max` | `2` | 线程池最大线程数 |
| `[thread_pool]` | `thread_min` | `1` | 线程池最小线程数 |

---

## 6. 编译与运行

### 6.1 环境要求

- Linux 操作系统（推荐 Ubuntu 20.04 / 22.04）
- GCC 编译器
- GNU Make
- POSIX Threads 支持（pthread）

### 6.2 x86 本地编译

```bash
make            # 默认编译 x86 版本，输出到 app/file_manager
./app/file_manager
```

### 6.3 ARM64 交叉编译与 NFS 部署

需要提前安装 ARM64 交叉编译工具链（`aarch64-none-linux-gnu-gcc`）：

```bash
make arm        # 交叉编译 ARM64 版本，输出到 app/file_manager_arm
```

本项目采用 **NFS 网络文件系统** 进行开发板部署，Ubuntu 开发机作为 NFS 服务端共享目录，Orange Pi 作为客户端挂载，实现编译后直接在板上运行，无需反复拷贝文件。

**开发机（NFS 服务端）**：将编译产物放入 NFS 共享目录（例如 `~/linux/nfs`）：

```bash
cp app/file_manager_arm ~/linux/nfs/
cp app/config.ini ~/linux/nfs/app/
```

**Orange Pi（NFS 客户端）**：挂载开发机共享目录后直接运行：

```bash
# 创建挂载点
sudo mkdir -p /mnt/nfs
# 挂载 NFS 共享目录（将 <ubuntu-ip> 替换为开发机 IP，共享路径按实际情况修改）
sudo mount -t nfs <ubuntu-ip>:/home/user/linux/nfs /mnt/nfs
# 进入挂载目录运行程序
cd /mnt/nfs
chmod +x file_manager_arm
./file_manager_arm
```

> NFS 挂载方式下，开发机修改代码并重新编译后，开发板端无需重新拷贝，直接运行即可，适合嵌入式开发的频繁调试场景。

### 6.4 同时编译双平台

```bash
make all        # 同时编译 x86 和 ARM64 版本
```

### 6.5 清理

```bash
make clean      # 删除编译产物和日志文件
```

---

## 7. 开发与部署环境

| 项目 | 配置 |
|------|------|
| 开发系统 | Ubuntu 22.04 LTS (VMware Workstation 虚拟机) |
| 本地编译器 | GCC (x86_64) |
| 交叉编译器 | aarch64-none-linux-gnu-gcc (ARM64) |
| 构建工具 | GNU Make |
| 线程库 | POSIX Threads (pthread) |
| 配置解析 | inih |
| 版本控制 | Git |
| 目标平台 | x86_64 / ARM64 (AArch64) |
| ARM 开发板 | Orange Pi Zero 3 |
| SoC | Allwinner H616 |
| 开发方式 | Ubuntu 开发 + ARM64 交叉编译 + 开发板部署测试 |

---

## 8. 项目亮点

1. **完整的工程化结构**：头文件/源码/第三方库/运行时目录分离，模块化设计，各模块职责单一、接口清晰
2. **自研动态线程池**：非网上最简版，实现了管理者线程动态扩容缩容、双锁减少竞争、环形任务队列，具备生产级线程池的核心特征
3. **线程安全日志系统**：互斥锁 + flock 文件锁双重保护，支持多线程/多进程环境，dup2 标准输出重定向，日志含 PID/TID 便于调试
4. **网络服务与线程池深度集成**：Accept 线程 + 线程池任务分发的经典架构，客户端链表统一管理，支持运行时状态查询与优雅关闭
5. **运行时可配置**：通过 INI 配置文件管理日志路径、线程池参数，无需重新编译即可调整系统行为
6. **双平台编译部署**：一套 Makefile 同时支持 x86 开发调试与 ARM64 交叉编译部署，完整覆盖从开发到板上运行的嵌入式开发流程
7. **路径无关设计**：通过 `/proc/self/exe` 获取程序根目录，配置文件和日志文件使用相对路径，程序可在任意工作目录下运行

---

## 9. 后续规划

- [x] **服务器通信协议扩展**：从回显服务器升级为自定义应用层协议（设备ID + 命令字 + 数据 + 校验），支持命令解析与数据转发
- [x] **GPIO 控制模块**：基于 Linux sysfs 接口封装 GPIO 操作，实现服务器运行状态 LED 指示
- [x] **智能网关模块**：替代原 server.c，实现 TCP 网关 + 协议帧解析 + 回调分发 + 心跳保活
- [x] **网关服务器交互界面**：q/s/h/l/r 命令管理界面，集成项目线程池
- [ ] **ESP8266 WiFi 节点对接**：STM32F103 + ESP-01 通过 WiFi TCP 接入服务器，实现传感器数据上传与远程控制
- [ ] **服务器端口配置化**：将监听端口从硬编码移入 config.ini
- [ ] **数据持久化**：传感器数据写入文件管理模块，支持历史查询
- [ ] **性能测试与优化**：多客户端并发压力测试，线程池参数调优
- [ ] **固件 OTA 更新通道**：网关向设备下发固件更新数据

---

## 10. 版本历史

| 版本 | 日期 | 主要内容 |
|------|------|---------|
| V0.1 | - | 基础文件管理、目录管理、日志系统、线程池、INI 配置管理、路径管理 |
| V0.2 | 2026.08 | 新增网络服务器模块（TCP Socket + 多客户端 + 线程池集成 + 客户端链表管理），支持运行时状态查询与优雅关闭 |
| V0.3 | 2026.08 | 三模块架构替代 server.c：GPIO 模块（sysfs）、Protocol 模块（二进制帧协议）、Gateway 模块（TCP服务器+心跳+回调分发）；网关服务器管理界面（q/s/h/l/r）；集成项目线程池 |
