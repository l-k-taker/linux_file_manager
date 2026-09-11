#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
网关 JSON 日志 ETL 脚本
读取 events.json.log → 解析 → 批量写入 MySQL
支持断点续传：记录上次读取的 offset，避免重复导入
"""

import json
import os
import sys
from datetime import datetime

import mysql.connector


# ==================== 配置 ====================
DB_CONFIG = {
    'host': 'localhost',
    'user': 'gateway_user',
    'password': '@liukang200612',        
    'database': 'gateway_logs',
    'charset': 'utf8mb4'
}

# JSON 日志路径
LOG_FILE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    '..', 'app', 'logs', 'events.json.log'
)

# 断点位置记录文件
OFFSET_FILE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    '.last_offset'
)

BATCH_SIZE = 1000


# ==================== 工具函数 ====================
def read_last_offset():
    if not os.path.exists(OFFSET_FILE):
        return 0
    try:
        with open(OFFSET_FILE, 'r') as f:
            return int(f.read().strip())
    except (ValueError, IOError):
        return 0


def save_last_offset(offset):
    with open(OFFSET_FILE, 'w') as f:
        f.write(str(offset))


def parse_line(line):
    line = line.strip()
    if not line:
        return None

    try:
        data = json.loads(line)

        ts_str = data.get('ts', '')
        if ts_str:
            event_time = datetime.fromisoformat(ts_str)
        else:
            event_time = datetime.now()

        return (
            event_time,
            data.get('event', 'unknown')[:50],
            data.get('ip', '')[:45],
            data.get('fd', 0),
            data.get('detail', '')[:255]
        )
    except (json.JSONDecodeError, ValueError) as e:
        print(f"[WARN] 跳过无效行: {e}", file=sys.stderr)
        return None


# ==================== 主流程 ====================
def main():
    if not os.path.exists(LOG_FILE):
        print(f"[ERROR] 日志文件不存在: {LOG_FILE}")
        return 1

    last_offset = read_last_offset()
    file_size = os.path.getsize(LOG_FILE)

    if last_offset >= file_size:
        print(f"[INFO] 没有新日志（offset={last_offset}, size={file_size}）")
        return 0

    print(f"[INFO] 从 offset={last_offset} 开始读取，文件大小={file_size}")

    try:
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
    except mysql.connector.Error as e:
        print(f"[ERROR] 数据库连接失败: {e}")
        return 1

    insert_sql = """
        INSERT INTO gateway_events
            (event_time, event_type, client_ip, fd, detail)
        VALUES (%s, %s, %s, %s, %s)
    """

    batch = []
    total_inserted = 0
    current_offset = last_offset

    try:
        with open(LOG_FILE, 'r', encoding='utf-8') as f:
            f.seek(last_offset)

            for line in f:
                current_offset += len(line.encode('utf-8'))

                record = parse_line(line)
                if record is None:
                    continue

                batch.append(record)

                if len(batch) >= BATCH_SIZE:
                    cursor.executemany(insert_sql, batch)
                    conn.commit()
                    total_inserted += len(batch)
                    batch.clear()

            if batch:
                cursor.executemany(insert_sql, batch)
                conn.commit()
                total_inserted += len(batch)

        save_last_offset(current_offset)

        print(f"[OK] 本次导入 {total_inserted} 条记录，offset 更新至 {current_offset}")
        return 0

    except mysql.connector.Error as e:
        conn.rollback()
        print(f"[ERROR] 数据库操作失败: {e}")
        return 1
    except Exception as e:
        print(f"[ERROR] 未知错误: {e}")
        return 1
    finally:
        cursor.close()
        conn.close()


if __name__ == '__main__':
    sys.exit(main())