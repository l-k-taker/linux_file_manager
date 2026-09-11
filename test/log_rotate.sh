#!/bin/bash
# ============================================================
# 日志归档与清理脚本
#
# 功能:
#   1. 归档 7 天前的 events.json.log
#   2. 删除 30 天前的归档
#   3. 清理 MySQL 中 30 天前的数据
#
# 建议配合 Cron 每天凌晨 2 点执行
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

LOG_DIR="${PROJECT_ROOT}/app/logs"
ARCHIVE_DIR="${LOG_DIR}/archive"
LOG_FILE="${LOG_DIR}/events.json.log"

# 保留策略
ARCHIVE_DAYS=7      # 7 天前的日志归档
DELETE_DAYS=30      # 30 天前的归档删除
DB_KEEP_DAYS=30     # MySQL 保留 30 天数据

# MySQL 配置
DB_USER="gateway_user"
DB_PASS="@liukang200612"      # ← 改成你的密码
DB_NAME="gateway_logs"

# 日志输出
LOG_OUT="${SCRIPT_DIR}/log_rotate.log"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_OUT"
}

# ============================================================
# 1. 创建归档目录
# ============================================================
mkdir -p "$ARCHIVE_DIR"

# ============================================================
# 2. 归档 7 天前的 JSON 日志
# ============================================================
if [ -f "$LOG_FILE" ]; then
    # 判断文件是否超过 7 天
    if [ "$(find "$LOG_FILE" -mtime +$ARCHIVE_DAYS 2>/dev/null)" ]; then
        TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
        ARCHIVE_FILE="${ARCHIVE_DIR}/events.json.${TIMESTAMP}.log"

        mv "$LOG_FILE" "$ARCHIVE_FILE"
        touch "$LOG_FILE"

        log "📦 归档日志: $ARCHIVE_FILE"

        # 重置 ETL 断点文件，避免 offset 错位
        if [ -f "${SCRIPT_DIR}/.last_offset" ]; then
            rm -f "${SCRIPT_DIR}/.last_offset"
            log "🔄 重置 ETL offset"
        fi
    else
        log "ℹ️  JSON 日志未满 $ARCHIVE_DAYS 天，跳过归档"
    fi
else
    log "⚠️  日志文件不存在: $LOG_FILE"
fi

# ============================================================
# 3. 删除 30 天前的归档
# ============================================================
DELETED=$(find "$ARCHIVE_DIR" -name "events.json.*.log" -mtime +$DELETE_DAYS -delete -print | wc -l)
if [ "$DELETED" -gt 0 ]; then
    log "🗑️  删除 $DELETED 个过期归档"
else
    log "ℹ️  没有过期归档需要删除"
fi

# ============================================================
# 4. 清理 MySQL 30 天前的数据
# ============================================================
/snap/bin/mysql -u "$DB_USER" -p"$DB_PASS" "$DB_NAME" -e \
    "DELETE FROM gateway_events WHERE event_time < DATE_SUB(NOW(), INTERVAL $DB_KEEP_DAYS DAY);" \
    2>/dev/null

if [ $? -eq 0 ]; then
    log "🗄️  MySQL 清理完成（保留最近 $DB_KEEP_DAYS 天）"
else
    log "❌ MySQL 清理失败"
fi

log "✅ 日志归档任务完成"