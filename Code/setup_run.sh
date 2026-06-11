#!/bin/bash

# ===== RUN ESP32 PROJECT =====
echo "===== RUN ESP32 PROJECT ====="

# Cấp quyền chạy cho chính file này (nếu chưa có)
chmod +x "$0"

# [1] Kiểm tra Python đã cài chưa
echo "[1] Check Python..."
python3 --version || exit 1   # nếu lỗi thì dừng script

# [2] Cài / cập nhật PlatformIO
echo "[2] Install / Update PlatformIO..."
python3 -m pip install -U platformio || exit 1

# [3] Build project (compile code)
echo "[3] Build..."
python3 -m platformio run || exit 1

# [4] Tự động tìm cổng USB của ESP32
echo "[4] Detect ESP32 port..."
PORT=$(ls /dev/cu.* 2>/dev/null | grep -E "usb|wch|serial|SLAB|UART" | head -n 1)

# Nếu không tìm thấy thì báo lỗi
if [ -z "$PORT" ]; then
  echo "❌ Không tìm thấy ESP32"
  exit 1
fi

echo "Port: $PORT"

# [5] Upload code vào ESP32
echo "[5] Upload..."
python3 -m platformio run --target upload --upload-port "$PORT" || exit 1

# [6] Mở Serial Monitor với baud 115200
echo "[6] Monitor baud 115200..."
python3 -m platformio device monitor --port "$PORT" --baud 115200