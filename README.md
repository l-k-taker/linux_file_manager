# Linux File Manager — 基于ARM Linux的模块化文件管理与智能网关系统

一个面向嵌入式Linux平台的C语言工程项目，以文件管理为核心载体，串联起Linux系统编程、POSIX多线程、线程池、网络通信、日志系统、配置管理等多个技术模块，并支持x86与ARM64双平台编译部署。

项目最初用于Linux系统编程学习与C语言工程化实践，目前已演进为具备智能网关能力的嵌入式应用框架，支持STM32+ESP8266设备通过TCP接入、二进制协议通信、GPIO控制、心跳保活，并配套完整的 **JSON 事件日志 → Python ETL → MySQL → SQL 分析** 数据管道。

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

  JSON日志模块 (log_json)   Python ETL        MySQL 分析
  结构化事件输出              JSON → MySQL       SQL 聚合查询
──────────────────────────────────────────────────────────────
              Linux / POSIX 系统调用层 (open/read/write/...)
──────────────────────────────────────────────────────────────
  x86_64 (Ubuntu 开发环境)   │   ARM64 (Orange Pi Zero 3 部署)
──────────────────────────────────────────────────────────────
  STM32F103 + ESP8266 客户端  →  TCP 6789 →  网关服务器
  Linux 测试客户端 (test/)     →  TCP 6789 →  网关服务器
──────────────────────────────────────────────────────────────
```

---

## 2. 技术栈

| 类别 | 技术 |
|------|------|
| 开发语言 | C (C99)、Python 3（ETL 脚本） |
| 系统接口 | Linux/POSIX 系统调用 (open, read, write, close, stat, opendir 等) |
| 多线程 | POSIX Threads (pthread)、互斥锁、条件变量 |
| 并发模型 | 线程池（动态扩容缩容 + 管理者线程 + 生产者-消费者模型） |
| 网络通信 | TCP Socket、多客户端并发接入、线程池任务分发 |
| 通信协议 | 自定义二进制帧协议（魔数+命令字+序列号+载荷+异或校验） |
| 硬件控制 | Linux sysfs GPIO 接口（Orange Pi Zero 3 26pin 排针） |
| 日志系统 | 文件描述符、write、dup2 标准输出重定向、flock 文件锁 |
| 结构化日志 | JSON Lines 格式事件日志、独立文件描述符 + flock |
| 数据处理 | Python ETL、JSON 解析、批量插入、断点续传 |
| 数据库 | MySQL 8.x（事件存储、聚合查询、索引优化） |
| 配置管理 | inih 第三方库解析 INI 配置文件，运行时可配 |
| 自动化 | Cron 定时任务（ETL + 日志归档 + 数据清理） |
| 构建工具 | GNU Make（支持 x86 与 ARM64 交叉编译双目标） |
| 版本控制 | Git |
| 目标平台 | x86_64 / ARM64 (AArch64) |
| 硬件平台 | Orange Pi Zero 3 (Allwinner H616/H618) |

---

## 3. 目录结构

```
linux_file_manager/
├── app/                        # 运行时目录
│   ├── config.ini              # 主配置文件（日志路径、JSON路径、线程池参数）
│   └── logs/                   # 日志输出目录
│       ├── system.log          # 文本日志
│       ├── events.json.log     # JSON 事件日志（供 ETL 消费）
│       └── archive/            # 归档目录（log_rotate 自动创建）
├── include/                    # 头文件
│   ├── common.h                # 通用工具函数声明
│   ├── config.h                # 配置管理模块
│   ├── directory.h             # 目录操作模块
│   ├── file_manager.h          # 文件管理模块
│   ├── gateway.h               # 智能网关模块（TCP服务器+心跳+回调分发）
│   ├── gpio.h                  # GPIO 控制模块（sysfs 接口）
│   ├── log.h                   # 日志系统模块
│   ├── log_json.h              # JSON 事件日志模块
│   ├── menu.h                  # 命令行菜单交互
│   ├── path_manager.h          # 路径管理模块
│   ├── protocol.h              # 通信协议模块（二进制帧协议）
│   └── thread_pool.h           # 线程池模块
├── src/                        # 源码实现
│   ├── common.c
│   ├── config.c
│   ├── directory.c
│   ├── file_manager.c
│   ├── gateway.c               # 网关服务器实现
│   ├── gpio.c
│   ├── log.c
│   ├── log_json.c              # JSON 事件日志实现（条件编译）
│   ├── menu.c
│   ├── path_manager.c
│   ├── protocol.c
│   └── thread_pool.c
├── test/                       # 测试与客户端
│   ├── client.c                # Linux 测试客户端（交互 + 自动模式）
│   ├── run_client.sh           # 并发启动脚本（PID 管理 + 监督进程）
│   ├── log_etl.py              # Python ETL：JSON → MySQL
│   ├── log_rotate.sh           # 日志归档 + MySQL 清理
│   └── .gitignore
├── third_reporty/              # 第三方依赖
│   └── inih/                   # INI 配置文件解析库
│       ├── ini.c
│       └── ini.h
├── main.c                      # 程序入口
├── Makefile                    # 构建脚本（x86 / ARM64 / JSON / client）
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
path=logs/system.log              ; 文本日志文件路径（相对程序根目录）
json_path=logs/events.json.log    ; JSON 事件日志路径（相对程序根目录）

[thread_pool]
task_num=100                      ; 任务队列容量
thread_max=32                     ; 最大工作线程数
thread_min=1                      ; 最小工作线程数
```

**实现要点：**
- `config_handler()` 回调函数按 section + name 匹配配置项，写入全局 `Config` 结构体
- 其他模块通过 `extern Config config;` 直接访问配置
- 配置加载失败时打印错误并返回 -1

### 4.7 GPIO 控制模块 (`gpio.c`)

基于 Linux sysfs 接口 (`/sys/class/gpio/`) 封装 GPIO 操作，用于 Orange Pi Zero 3 (Allwinner H616/H618)：

**引脚映射（26pin 排针）：**

| 宏定义 | 引脚 | sysfs 编号 | 物理引脚 |
|--------|------|-----------|---------|
| `GPIO_PC6` | PC6 | 70 | Pin 11 |
| `GPIO_PC7` | PC7 | 71 | Pin 22 |
| `GPIO_PC8` | PC8 | 72 | Pin 15 |
| `GPIO_PC9` | PC9 | 73 | Pin 7（启动指示灯，默认） |
| `GPIO_PC10` | PC10 | 74 | Pin 26 |
| `GPIO_PC11` | PC11 | 75 | Pin 12 |
| `GPIO_PC12` | PC12 | 76 | Pin 16 |
| `GPIO_PC13` | PC13 | 77 | Pin 18 |
| `GPIO_PC14` | PC14 | 78 | Pin 18 |
| `GPIO_PC15` | PC15 | 79 | Pin 16 |

**sysfs 编号规则：** `PCx = 64 + x`（Port C 从 64 开始编号）

**核心功能：**
- `gpio_init(pin, direction, initial_value)` — 导出引脚、设置方向、初始值（含双保险写入机制，防止内核异步重置导致电平不稳定）
- `gpio_set(pin, value)` / `gpio_get(pin)` — 设置/读取电平
- `gpio_toggle(pin)` — 翻转电平
- `gpio_uninit(pin)` — 拉低电平并取消导出
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
- `proto_verify_magic()` — 验证帧头魔数

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

选项 11 进入网关服务器管理界面，提供完整的 q/s/h/l/r 命令交互能力。

### 4.11 JSON 事件日志模块 (`log_json.c`)

在原有文本日志基础上，新增独立的结构化事件日志模块，用于网关关键事件的记录，为 Python ETL 提供数据源。

**设计特点：**
- **可插拔设计**：通过编译宏 `ENABLE_JSON_LOG` 控制，未启用时 `LOG_EVENT` 宏退化为空操作，对原项目零侵入
- **独立文件描述符**：与 `log.c` 的文本日志并行，互不干扰，各自读取 `config.json_path`
- **线程安全 + 进程安全**：沿用 `log.c` 的 `pthread_mutex` + `flock` 双重保护机制
- **JSON Lines 格式**：每行一条独立 JSON，便于流式解析
- **路径自适应**：支持绝对路径和相对路径，相对路径自动拼接程序根目录

**输出格式：**
```json
{"ts":"2026-09-10T16:22:31","event":"device_register","ip":"10.91.58.14","fd":6,"detail":"register request"}
{"ts":"2026-09-10T16:22:34","event":"gpio_control","ip":"10.91.58.14","fd":6,"detail":"gpio control command"}
{"ts":"2026-09-10T16:22:40","event":"sensor_data","ip":"10.91.58.14","fd":6,"detail":"sensor data received"}
```

**事件埋点位置（menu.c 的 handler 回调中）：**

| 事件类型 | 触发时机 |
|---------|---------|
| `device_register` | 收到设备注册请求 |
| `sensor_data` | 收到传感器数据 |
| `gpio_control` | 收到 GPIO 控制命令 |

**统一接口宏：**
```c
#ifdef ENABLE_JSON_LOG
    #define LOG_EVENT(event, ip, fd, detail) log_event_json(event, ip, fd, detail)
    #define LOG_JSON_INIT()  log_json_init()
    #define LOG_JSON_CLOSE() log_json_close()
#else
    #define LOG_EVENT(event, ip, fd, detail) do { } while(0)
    #define LOG_JSON_INIT()  do { } while(0)
    #define LOG_JSON_CLOSE() do { } while(0)
#endif
```

### 4.12 Python ETL 模块 (`test/log_etl.py`)

将 `events.json.log` 中的 JSON 事件数据解析、清洗、批量写入 MySQL，形成完整的数据管道。

**核心功能：**
- **JSON 解析**：逐行解析 JSON Lines，字段提取与长度截断
- **时间转换**：ISO8601 → MySQL DATETIME
- **批量插入**：每 1000 条一批 `executemany`，减少网络往返
- **断点续传**：记录上次读取的文件字节偏移量（`.last_offset`），避免重复导入
- **异常处理**：单行解析失败跳过，事务提交失败回滚

**关键实现：**
```python
# 断点续传：从上次位置继续读取
f.seek(last_offset)
for line in f:
    current_offset += len(line.encode('utf-8'))
    record = parse_line(line)
    if record:
        batch.append(record)
    if len(batch) >= BATCH_SIZE:
        cursor.executemany(insert_sql, batch)
        conn.commit()
save_last_offset(current_offset)
```

### 4.13 日志归档模块 (`test/log_rotate.sh`)

定期归档 JSON 日志、清理过期数据，避免日志无限增长。

**功能：**
- 归档超过 7 天的 `events.json.log` 至 `app/logs/archive/`
- 删除超过 30 天的历史归档
- 清理 MySQL 中超过 30 天的事件记录
- 归档时同步重置 ETL 断点文件，避免 offset 错位

### 4.14 Linux 测试客户端 (`test/client.c`)

用于模拟 STM32 设备与网关通信的 Linux 客户端，支持交互和自动两种模式。

**两种运行模式：**

| 模式 | 启动参数 | 行为 |
|------|---------|------|
| 交互模式 | `./client <ip> [port]` | 显示 `client>` 提示符，键盘触发发送 |
| 自动模式 | `./client -a -i <id> <ip> [port]` | 每 3 秒轮流发送 sensor/gpio/ping，静默运行 |

**交互命令：**

| 命令 | 功能 |
|------|------|
| `h` | 帮助 |
| `s` | 连接状态 |
| `q` | 退出 |
| `r` | 发送注册请求 (CMD=0x10) |
| `d` | 发送传感器数据 (CMD=0x11) |
| `g` | 发送 GPIO 控制 (CMD=0x20) |
| `p` | 发送 PING (CMD=0x01) |
| `v` | 切换心跳日志显示 |

**关键技术：**
- `select()` 同时监听 stdin 和 socket，实现收发并发
- 收到服务器 PING 自动回 PONG，维持长连接
- 自动模式下把发送逻辑放在循环开头，不依赖 select 超时

### 4.15 并发测试脚本 (`test/run_client.sh`)

批量启动多个客户端进行并发测试。

**用法：**
```bash
./test/run_client.sh                        # 默认启动 5 个
./test/run_client.sh 10                     # 启动 10 个
./test/run_client.sh 10 192.168.1.100      # 指定 IP
./test/run_client.sh stop                   # 只停止本脚本启动的客户端
```

**设计要点：**
- **PID 文件管理**：记录脚本启动的客户端 PID，stop 时精确停止，绝不误杀服务端
- **监督进程**：后台子进程，每 3 秒检测客户端存活状态，全部退出后自动清理 PID 文件
- **服务端可达检测**：启动前先探测端口，服务端未启动直接报错
- **SIGTERM + SIGKILL 兜底**：先优雅停止，1 秒后强杀残留

---

## 5. 配置文件说明

配置文件位于 `app/config.ini`，程序启动时自动加载：

| Section | Key | 默认值 | 说明 |
|---------|-----|--------|------|
| `[log]` | `path` | `logs/system.log` | 文本日志文件相对路径 |
| `[log]` | `json_path` | `logs/events.json.log` | JSON 事件日志相对路径 |
| `[thread_pool]` | `task_num` | `100` | 任务队列最大容量 |
| `[thread_pool]` | `thread_max` | `32` | 线程池最大线程数 |
| `[thread_pool]` | `thread_min` | `1` | 线程池最小线程数 |

---

## 6. 编译与运行

### 6.1 环境要求

- Linux 操作系统（推荐 Ubuntu 20.04 / 22.04）
- GCC 编译器
- GNU Make
- POSIX Threads 支持（pthread）
- Python 3 + `mysql-connector-python`（ETL 需要）
- MySQL 8.x（数据存储需要）
- ARM64 交叉编译工具链（`aarch64-none-linux-gnu-gcc`，仅交叉编译时需要）

### 6.2 x86 本地编译

```bash
make            # 默认编译 x86 版本（不含 JSON 模块），输出到 app/file_manager
./app/file_manager
```

### 6.3 编译 JSON 版本

```bash
make json       # 编译含 JSON 事件日志的版本，输出到 app/file_manager_json
./app/file_manager_json
```

### 6.4 编译测试客户端

```bash
make client     # 编译客户端，输出到 test/client
```

### 6.5 ARM64 交叉编译与 NFS 部署

```bash
make arm        # 交叉编译 ARM64 版本，输出到 app/file_manager_arm
```

本项目采用 **NFS 网络文件系统** 进行开发板部署，Ubuntu 开发机作为 NFS 服务端共享目录，Orange Pi 作为客户端挂载，实现编译后直接在板上运行，无需反复拷贝文件。

**开发机（NFS 服务端）**：将编译产物放入 NFS 共享目录：

```bash
cp app/file_manager_arm ~/linux/nfs/
cp app/config.ini ~/linux/nfs/app/
```

**Orange Pi（NFS 客户端）**：挂载开发机共享目录后直接运行：

```bash
# 创建挂载点
sudo mkdir -p /mnt/nfs
# 挂载 NFS 共享目录（将 <ubuntu-ip> 替换为开发机 IP）
sudo mount -t nfs <ubuntu-ip>:/home/user/linux/nfs /mnt/nfs
# 进入挂载目录运行程序
cd /mnt/nfs
chmod +x file_manager_arm
sudo ./file_manager_arm
```

> GPIO 操作需要 root 权限，Orange Pi 上运行时需加 `sudo`。
>
> NFS 挂载方式下，开发机修改代码并重新编译后，开发板端无需重新拷贝，直接运行即可，适合嵌入式开发的频繁调试场景。

### 6.6 同时编译双平台

```bash
make all        # 同时编译 x86 和 ARM64 版本
```

### 6.7 清理

```bash
make clean      # 删除所有编译产物和日志文件
```

---

## 7. 数据管道：JSON 日志 → MySQL

项目支持从网关事件日志到数据库分析的完整数据管道。

### 7.1 初始化数据库

```bash
mysql -u gateway_user -p
```

```sql
CREATE DATABASE IF NOT EXISTS gateway_logs;
USE gateway_logs;

CREATE TABLE IF NOT EXISTS gateway_events (
    id INT AUTO_INCREMENT PRIMARY KEY,
    event_time DATETIME NOT NULL,
    event_type VARCHAR(50) NOT NULL,
    client_ip VARCHAR(45),
    fd INT,
    detail VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_event_time (event_time),
    INDEX idx_event_type (event_type),
    INDEX idx_client_ip (client_ip)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 7.2 手动执行 ETL

```bash
cd test/
python3 log_etl.py
```

**输出示例：**
```
[INFO] 从 offset=0 开始读取，文件大小=13212
[OK] 本次导入 120 条记录，offset 更新至 13212
```

**再次执行（无新数据时）：**
```
[INFO] 没有新日志（offset=13212, size=13212）
```

### 7.3 SQL 分析查询

**按事件类型聚合：**
```sql
SELECT event_type, COUNT(*) AS cnt
FROM gateway_events
GROUP BY event_type
ORDER BY cnt DESC;
```

**按客户端聚合：**
```sql
SELECT fd,
       COUNT(*) AS total_events,
       SUM(CASE WHEN event_type = 'sensor_data' THEN 1 ELSE 0 END) AS sensor_cnt,
       SUM(CASE WHEN event_type = 'gpio_control' THEN 1 ELSE 0 END) AS gpio_cnt
FROM gateway_events
GROUP BY fd
ORDER BY total_events DESC;
```

**按小时统计趋势：**
```sql
SELECT DATE_FORMAT(event_time, '%Y-%m-%d %H:00') AS hour,
       event_type,
       COUNT(*) AS cnt
FROM gateway_events
GROUP BY hour, event_type
ORDER BY hour DESC, cnt DESC;
```

### 7.4 自动化：Cron 定时任务

编辑 `crontab -e`，添加：

```cron
# 环境变量（cron 环境比 shell 简单，需显式指定）
PYTHONPATH=/home/liukang/.local/lib/python3.10/site-packages
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin

# 每天 02:00 执行 ETL（JSON 日志 → MySQL）
0 2 * * * cd /home/liukang/linux/project/linux_file_manager/test && /usr/bin/python3 log_etl.py >> log_etl.log 2>&1

# 每天 02:30 执行日志归档与数据清理
30 2 * * * /home/liukang/linux/project/linux_file_manager/test/log_rotate.sh
```

**验证：**
```bash
crontab -l
cat test/log_etl.log
cat test/log_rotate.log
```

### 7.5 手动执行日志归档

```bash
./test/log_rotate.sh
```

**功能：**
- 归档 7 天前的 JSON 日志至 `app/logs/archive/`
- 删除 30 天前的归档
- 清理 MySQL 中 30 天前的事件记录
- 归档时重置 ETL 断点文件

---

## 8. 开发与部署环境

| 项目 | 配置 |
|------|------|
| 开发系统 | Ubuntu 22.04 LTS (VMware Workstation 虚拟机) |
| 本地编译器 | GCC (x86_64) |
| 交叉编译器 | aarch64-none-linux-gnu-gcc (ARM64) |
| 构建工具 | GNU Make |
| 线程库 | POSIX Threads (pthread) |
| 配置解析 | inih |
| 数据库 | MySQL 8.x (Snap 安装) |
| ETL 环境 | Python 3.10 + mysql-connector-python |
| 版本控制 | Git |
| 目标平台 | x86_64 / ARM64 (AArch64) |
| ARM 开发板 | Orange Pi Zero 3 |
| SoC | Allwinner H616 / H618 |
| 开发方式 | Ubuntu 开发 + ARM64 交叉编译 + NFS 挂载部署测试 |

---

## 9. 项目亮点

1. **完整的工程化结构**：头文件/源码/第三方库/运行时目录分离，模块化设计，各模块职责单一、接口清晰
2. **自研动态线程池**：管理者线程动态扩容缩容、双锁减少竞争、环形任务队列，具备生产级线程池的核心特征
3. **线程安全日志系统**：互斥锁 + flock 文件锁双重保护，支持多线程/多进程环境，dup2 标准输出重定向，日志含 PID/TID 便于调试
4. **智能网关三线程架构**：Accept线程 + 线程池Worker + 心跳检测线程，实现协议帧解析、回调分发、心跳保活、设备管理等完整网关能力
5. **自定义二进制通信协议**：魔数+命令字+序列号+载荷+异或校验的帧结构，支持粘包/半包解析，请求-响应匹配
6. **sysfs GPIO 硬件控制**：基于 Linux 原生 sysfs 接口封装 GPIO 操作，零第三方依赖，含双保险写入机制应对内核异步重置
7. **运行时可配置**：通过 INI 配置文件管理日志路径、JSON 路径、线程池参数，无需重新编译即可调整系统行为
8. **双平台编译部署**：一套 Makefile 支持 x86 开发调试与 ARM64 交叉编译部署，配合 NFS 挂载实现编译即运行
9. **路径无关设计**：通过 `/proc/self/exe` 获取程序根目录，配置文件与日志文件使用相对路径
10. **可插拔的结构化日志**：`ENABLE_JSON_LOG` 宏控制，未启用时对原项目零侵入，启用后输出 JSON Lines 格式事件日志
11. **完整的可观测性数据管道**：C 网关 → JSON 日志 → Python ETL → MySQL → SQL 分析，实现了从数据产生到分析的全链路打通
12. **ETL 断点续传**：记录文件字节偏移量，避免重复导入，保证数据一致性
13. **Cron 自动化运维**：定时 ETL、日志归档、数据库清理，形成无人值守的日志生命周期管理

---

## 10. 后续规划

- [x] **服务器通信协议扩展**：自定义应用层协议（设备ID + 命令字 + 数据 + 校验）
- [x] **GPIO 控制模块**：基于 Linux sysfs 接口封装 GPIO 操作
- [x] **智能网关模块**：TCP 网关 + 协议帧解析 + 回调分发 + 心跳保活
- [x] **网关服务器交互界面**：q/s/h/l/r 命令管理界面
- [x] **JSON 事件日志模块**：结构化事件日志，可插拔设计
- [x] **Python ETL 管道**：JSON → MySQL 批量导入 + 断点续传
- [x] **日志归档与自动化**：log_rotate.sh + Cron 定时任务
- [x] **并发测试客户端**：Linux 客户端 + 自动模式 + PID 管理脚本
- [ ] **ESP8266 WiFi 节点对接**：STM32F103 + ESP-01 通过 WiFi TCP 接入
- [ ] **服务器端口配置化**：将监听端口从硬编码移入 config.ini
- [ ] **数据可视化**：Grafana / Web 看板展示 MySQL 中的事件趋势
- [ ] **性能测试与优化**：多客户端并发压力测试，线程池参数调优
- [ ] **固件 OTA 更新通道**：网关向设备下发固件更新数据
- [ ] **epoll 改造**：将长连接架构从"线程池 + 阻塞 IO"升级为 epoll 多路复用

---

## 11. 版本历史

| 版本 | 日期 | 主要内容 |
|------|------|---------|
| V0.1 | - | 基础文件管理、目录管理、日志系统、线程池、INI 配置管理、路径管理 |
| V0.2 | 2026.08 | 新增网络服务器模块（TCP Socket + 多客户端 + 线程池集成 + 客户端链表管理），支持运行时状态查询与优雅关闭 |
| V0.3 | 2026.08 | 三模块架构替代 server.c：GPIO 模块（sysfs）、Protocol 模块（二进制帧协议）、Gateway 模块（TCP服务器+心跳+回调分发）；网关服务器管理界面（q/s/h/l/r）；集成项目线程池；NFS 挂载部署流程 |
| V0.4 | 2026.09 | 新增 JSON 事件日志模块（可插拔设计 + 条件编译）；Python ETL 管道（JSON → MySQL + 断点续传）；日志归档与 Cron 自动化；Linux 测试客户端（交互 + 自动模式）与并发启动脚本；MySQL 数据分析查询 |
