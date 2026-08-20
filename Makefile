CC=gcc
CROSS_COMPILE=aarch64-none-linux-gnu-
TARGET=app/file_manager
CFLAGS=-Iinclude -pthread
SRC=$(wildcard src/*.c) \
    third_reporty/inih/ini.c \
	main.c

# x86编译（默认）
$(TARGET):
	@mkdir -p app
	@$(CC) $(CFLAGS) $(SRC) -o $(TARGET)
	@echo "x86 build succeed"

# 交叉编译目标
arm:
	@mkdir -p app
	@$(CROSS_COMPILE)gcc $(CFLAGS) $(SRC) -o $(TARGET)_arm
	@echo "ARM build succeed: $(TARGET)_arm"

# 同时编译两个版本
all: $(TARGET) arm
	@echo "All builds complete"

clean:
	@rm -f app/file_manager app/file_manager_arm
	@rm -f app/logs/system.log
	@echo "project is cleaned"
