#!/bin/bash

echo "===== RUN ESP32 PROJECT ====="

# Cấp quyền cho chính file này (trường hợp chưa chmod)
chmod +x "$0"

# Kiểm tra Python
echo "[1] Check Python..."
python3 --version || exit 1

# Cài PlatformIO nếu chưa có
echo "[2] Install PlatformIO..."
python3 -m pip install -U platformio

# Build project
echo "[3] Build..."
python3 -m platformio run

# Tìm port ESP32
echo "[4] Detect ESP32 port..."
PORT=$(ls /dev/cu.* | grep -E "usb|wch|serial|SLAB" | head -n 1)

echo "Port: $PORT"

if [ -z "$PORT" ]; then
  echo "❌ Không tìm thấy ESP32"
  exit 1
fi

# Upload
echo "[5] Upload..."
python3 -m platformio run --target upload --upload-port "$PORT"

# Monitor
echo "[6] Monitor..."
python3 -m platformio device monitor --port "$PORT"