# HƯỚNG DẪN SỬ DỤNG HỆ THỐNG KÉT THÔNG MINH

## 1. Đăng nhập App
- Mở ứng dụng.
- Nhập tài khoản và mật khẩu.
- Nhập OTP nếu hệ thống yêu cầu.
- Sau khi đăng nhập thành công, app hiển thị trạng thái két.

## 2. Mở két trực tiếp
Có 3 cách:
- Vân tay
- RFID
- Mật khẩu keypad

Nếu xác thực đúng:
- Servo mở khóa.
- LCD hiển thị thành công.
- App cập nhật lịch sử.

Nếu xác thực sai:
- Buzzer báo lỗi.
- Hệ thống ghi log.
- Sai nhiều lần sẽ bật cảnh báo.

## 3. Mở két từ App
- Đăng nhập App.
- Chọn nút “Mở két”.
- Xác nhận thao tác.
- API tạo lệnh mở két.
- ESP32 nhận lệnh và mở servo.
- App hiển thị kết quả.

## 4. Khi có cảnh báo
Các trường hợp cảnh báo:
- Rung mạnh.
- Cửa mở trái phép.
- Nhập sai nhiều lần.
- Mất WiFi.

Hệ thống sẽ:
- Bật buzzer.
- Hiển thị trên LCD.
- Gửi SMS.
- Gửi cảnh báo lên App.
- Lưu sự kiện vào database.

## 5. Xem lịch sử
Trên App có thể xem:
- Lịch sử mở két.
- Lịch sử cảnh báo.
- Lịch sử SMS.
- Thời gian và phương thức xác thực.

## 6. Quản lý người dùng
Admin có thể:
- Thêm người dùng.
- Khóa/mở khóa tài khoản.
- Quản lý RFID, vân tay, mật khẩu.
- Cấu hình số điện thoại nhận SMS.

## 7. Kết nối Internet
Bình thường:
ESP32 dùng WiFi để gửi dữ liệu.

Khi WiFi lỗi:
ESP32 chuyển sang SIM7600 4G Data.

## 8. Lưu ý an toàn
- Không chia sẻ mật khẩu.
- Không để lộ thẻ RFID.
- Luôn giữ SIM có data.
- Kiểm tra pin dự phòng định kỳ.