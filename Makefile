CC=gcc
CROSS_COMPILE=aarch64-none-linux-gnu-
TARGET=app/file_manager
CFLAGS=-Iinclude -pthread
SRC=$(wildcard src/*.c) \
    third_reporty/inih/ini.c \
	main.c

# JSON 版本
TARGET_JSON=app/file_manager_json
CFLAGS_JSON=-DENABLE_JSON_LOG

# 客户端
CLIENT_SRC=test/client.c
CLIENT_BIN=test/client

# x86编译（默认，不含 JSON 模块）
$(TARGET):
	@mkdir -p app
	@$(CC) $(CFLAGS) $(SRC) -o $(TARGET)
	@echo "x86 build succeed"

# JSON 版本编译
json:
	@mkdir -p app
	@$(CC) $(CFLAGS) $(CFLAGS_JSON) $(SRC) -o $(TARGET_JSON)
	@echo "x86 JSON build succeed: $(TARGET_JSON)"

# 交叉编译目标
arm:
	@mkdir -p app
	@$(CROSS_COMPILE)gcc $(CFLAGS) $(SRC) -o $(TARGET)_arm
	@echo "ARM build succeed: $(TARGET)_arm"

# 客户端编译
client:
	@$(CC) $(CLIENT_SRC) -o $(CLIENT_BIN)
	@echo "client build succeed: $(CLIENT_BIN)"

# 同时编译两个版本
all: $(TARGET) arm
	@echo "All builds complete"

clean:
	@rm -f app/file_manager app/file_manager_arm app/file_manager_json
	@rm -f app/logs/*.log
	@rm -f $(CLIENT_BIN)
	@rm -f test/*.log
	@echo "project is cleaned"

.PHONY: json arm client all clean