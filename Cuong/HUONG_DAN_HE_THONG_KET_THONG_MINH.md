# Hướng dẫn xây dựng hệ thống két thông minh đa phương thức xác thực

## 1. Kiến trúc tổng thể

- **Thiết bị két thông minh (ESP32/Arduino):**
  - Xác thực: vân tay, thẻ RFID, mật mã số.
  - Giám sát: rung, khói, trạng thái cửa.
  - Cảnh báo: còi, LCD, gửi cảnh báo từ xa.
  - Giao tiếp: SIM 4G/WiFi với server/app.

- **Backend (MySQL + REST API):**
  - Lưu trữ tài khoản, lịch sử xác thực, cảnh báo, cấu hình.
  - Xử lý xác thực từ xa, gửi cảnh báo, quản lý người dùng.

- **App di động (Flutter):**
  - Đăng nhập, điều khiển két, nhận cảnh báo, xem lịch sử, quản lý người dùng.

---

## 2. Chức năng chính

### A. Thiết bị két
- Xác thực độc lập & xác thực kép (2 bước liên tiếp).
- Giới hạn số lần xác thực sai, chuyển cảnh báo khi vượt ngưỡng.
- Lưu lịch sử xác thực, gửi log lên server.
- Phát hiện bất thường (rung, khói, mở cửa trái phép), chuyển cảnh báo.
- Phản hồi âm thanh (buzzer) & hiển thị LCD.
- Nhận lệnh mở két từ xa qua app (sau xác thực).

### B. Backend (MySQL + API)
- Bảng users, auth_methods, auth_logs, alerts, settings.
- API: đăng nhập, xác thực, lấy/gửi trạng thái két, lịch sử, cảnh báo, cấu hình.
- Gửi thông báo push (FCM/MQTT) khi có cảnh báo.

### C. App Flutter
- Đăng nhập, bảo mật, tự động đăng xuất khi hết phiên.
- Màn hình chính: trạng thái két, cảm biến, cảnh báo, nút mở két từ xa.
- Nhận thông báo cảnh báo, xem chi tiết, tắt cảnh báo sau xác thực.
- Xem lịch sử hoạt động, lọc theo loại/thời gian.
- Quản lý người dùng, cấu hình hệ thống.

---

## 3. Quy trình triển khai

1. Xây dựng từng module nhỏ, kiểm thử độc lập (firmware, backend, app).
2. Định nghĩa rõ API giữa các thành phần.
3. Tích hợp dần từng phần, kiểm tra thực tế.
4. Viết tài liệu hướng dẫn sử dụng, cấu hình, bảo trì.

---

## 4. Công nghệ đề xuất

- **Firmware:** PlatformIO, Arduino IDE, ESP-IDF, Adafruit_Fingerprint, MFRC522, FreeRTOS.
- **Backend:** MySQL Workbench, Node.js/Express, Flask, Laravel, REST API, JWT.
- **App:** Flutter, Firebase Cloud Messaging (FCM), http package.

---

## 5. Gợi ý mở rộng

- Hỗ trợ thêm xác thực khuôn mặt, OTP qua app.
- Tích hợp AI phát hiện xâm nhập bất thường.
- Hỗ trợ nhiều két, nhiều người dùng, phân quyền linh hoạt.

---

Nếu cần ví dụ code cụ thể cho từng phần, hãy chỉ rõ phần nào bạn muốn ưu tiên trước!
