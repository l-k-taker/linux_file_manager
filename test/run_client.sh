#!/bin/bash
# ============================================================
# 并发客户端启动脚本（v2：修复监督进程退出问题）
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLIENT_BIN="${SCRIPT_DIR}/client"
PID_FILE="${SCRIPT_DIR}/.client_pids"
SUPERVISOR_PID="${SCRIPT_DIR}/.supervisor_pid"

DEFAULT_IP="127.0.0.1"
DEFAULT_PORT=6789
DEFAULT_COUNT=5

# ============================================================
# 停止模式
# ============================================================
if [ "$1" = "stop" ]; then
    # ---------- 先强杀监督进程（它忽略 SIGTERM，必须 SIGKILL） ----------
    if [ -f "$SUPERVISOR_PID" ]; then
        SUP=$(cat "$SUPERVISOR_PID" 2>/dev/null)
        if [ -n "$SUP" ] && kill -0 "$SUP" 2>/dev/null; then
            kill -9 "$SUP" 2>/dev/null
        fi
        rm -f "$SUPERVISOR_PID"
    fi

    # ---------- 停止客户端 ----------
    if [ ! -f "$PID_FILE" ]; then
        echo "⚠️  没有找到 PID 文件"
        REMAIN=$(pgrep -f "${CLIENT_BIN}" | wc -l)
        if [ "$REMAIN" -gt 0 ]; then
            echo "   发现 $REMAIN 个残留客户端，强制清理..."
            pkill -9 -f "${CLIENT_BIN}"
        fi
        exit 0
    fi

    echo "🛑 停止本脚本启动的客户端..."
    STOPPED=0

    while read -r pid; do
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null && STOPPED=$((STOPPED + 1))
        fi
    done < "$PID_FILE"

    sleep 1

    KILLED=0
    while read -r pid; do
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null && KILLED=$((KILLED + 1))
        fi
    done < "$PID_FILE"

    rm -f "$PID_FILE"

    if [ "$KILLED" -gt 0 ]; then
        echo "✅ 已停止 $STOPPED 个（其中 $KILLED 个被强杀）"
    else
        echo "✅ 已停止 $STOPPED 个客户端"
    fi
    exit 0
fi

# ============================================================
# 启动模式
# ============================================================
COUNT=${1:-$DEFAULT_COUNT}
SERVER_IP=${2:-$DEFAULT_IP}
SERVER_PORT=${3:-$DEFAULT_PORT}

if [ ! -x "$CLIENT_BIN" ]; then
    echo "❌ 找不到可执行文件: $CLIENT_BIN"
    echo "   请先执行: make client"
    exit 1
fi

# 启动前清理残留
REMAIN=$(pgrep -f "${CLIENT_BIN}" | wc -l)
if [ "$REMAIN" -gt 0 ]; then
    echo "⚠️  发现 $REMAIN 个残留客户端，先清理..."
    pkill -9 -f "${CLIENT_BIN}"
    sleep 1
fi

# 启动前检测服务端
if ! (echo > /dev/tcp/${SERVER_IP}/${SERVER_PORT}) 2>/dev/null; then
    echo "❌ 无法连接服务端 ${SERVER_IP}:${SERVER_PORT}"
    echo "   请确认服务端已启动（菜单选 11）"
    exit 1
fi

# 清理旧文件
> "$PID_FILE"
if [ -f "$SUPERVISOR_PID" ]; then
    OLD_SUP=$(cat "$SUPERVISOR_PID" 2>/dev/null)
    [ -n "$OLD_SUP" ] && kill -9 "$OLD_SUP" 2>/dev/null
    rm -f "$SUPERVISOR_PID"
fi

# 启动
echo "============================================"
echo "  启动 $COUNT 个客户端（自动模式）"
echo "  目标服务器: $SERVER_IP:$SERVER_PORT"
echo "============================================"
echo ""

for i in $(seq 1 $COUNT); do
    "$CLIENT_BIN" -a -i "$i" "$SERVER_IP" "$SERVER_PORT" >/dev/null 2>&1 &
    PID=$!
    echo "$PID" >> "$PID_FILE"
    printf "  [%2d/%2d] 客户端已启动 PID=%d (ID=CLIENT-%02d)\n" "$i" "$COUNT" "$PID" "$i"
done

echo ""
echo "✅ 已启动 $COUNT 个客户端"
echo ""

# ============================================================
# 监督进程（修复版）
#
# 修复点：
#   1. 每轮先判断 PID_FILE 是否存在，不存在就自杀
#   2. trap 加 SIGQUIT
#   3. 读 PID 文件时用 2>/dev/null 兜底，但先判断文件
# ============================================================
(
    trap '' SIGHUP SIGINT SIGQUIT

    while true; do
        sleep 3

        # 文件被删（stop 或异常）→ 自杀
        if [ ! -f "$PID_FILE" ]; then
            rm -f "$SUPERVISOR_PID"
            exit 0
        fi

        ALIVE=0
        while read -r pid; do
            if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
                ALIVE=$((ALIVE + 1))
            fi
        done < "$PID_FILE"

        # 所有客户端都退了 → 清理并自杀
        if [ "$ALIVE" -eq 0 ]; then
            rm -f "$PID_FILE"
            rm -f "$SUPERVISOR_PID"
            exit 0
        fi
    done
) &

SUP_PID=$!
echo "$SUP_PID" > "$SUPERVISOR_PID"

echo "📋 后续操作:"
echo "   - 查看网关设备列表: 在网关命令行按 s"
echo "   - 停止所有客户端:   ./test/run_client.sh stop"
echo "   - 查看客户端进程:   ps aux | grep 'test/client'"
echo ""
echo "💡 服务端关闭后，客户端会自动退出，监督进程会自动清理 PID 文件"
echo ""