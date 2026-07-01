# ARCHITECTURE.md

## 1. Kiến trúc tổng thể
ESP32 → WiFi/SIM7600 → API → MySQL → App

## 2. Thành phần hệ thống
ESP32, RFID, vân tay, keypad, cảm biến rung, cảm biến cửa, SIM7600, GPS, API, Database, App.

## 3. Luồng mở két trực tiếp
RFID / vân tay / keypad → ESP32 → Servo → API → Database → App.

## 4. Luồng mở két từ App
App → API → safe_commands → ESP32 → Servo → safe_status.

## 5. Luồng cảnh báo
SW420 / MC38 → ESP32 → Buzzer → SMS → API → Database → App.

## 6. Luồng WiFi và SIM7600
Bình thường dùng WiFi.
Mất WiFi hoặc cảnh báo khẩn cấp thì dùng SIM7600.

## 7. Luồng GPS
Có cảnh báo → ESP32 lấy GPS → gửi SMS/API → App hiển thị vị trí.

## 8. Luồng Database
API ghi vào users, auth_logs, events, notifications, safe_status, safe_commands.

## 9. Bảo mật
JWT, hash password, OTP, giới hạn nhập sai, phân quyền mở két.