#!/bin/bash
set -e

echo "===== RUN ESP32 PROJECT ====="

echo "[1] Check Python..."
python3 --version

echo "[2] Create virtual environment..."
if [ ! -d ".venv" ]; then
  python3 -m venv .venv
fi

echo "[3] Activate virtual environment..."
source .venv/bin/activate

echo "[4] Install / Update pip..."
python -m pip install --upgrade pip

echo "[5] Install / Update PlatformIO..."
python -m pip install --upgrade platformio

echo "[6] PlatformIO version..."
python -m platformio --version

echo "[7] Build project..."
python -m platformio run

echo "[8] Upload project..."
python -m platformio run -t upload

echo "[9] Open serial monitor..."
python -m platformio device monitor -b 115200