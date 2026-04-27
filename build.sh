#!/bin/bash
# Harti 项目编译脚本

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 检查 ESP-IDF 环境
if [ -z "$IDF_PATH" ]; then
    echo "Error: ESP-IDF 环境未设置"
    echo "请先运行: . ~/esp-idf/export.sh"
    exit 1
fi

echo "=== 开始编译 Harti 项目 ==="
idf.py set-target esp32s3
idf.py build

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ 编译成功！"
else
    echo ""
    echo "❌ 编译失败，请检查错误"
    exit 1
fi
