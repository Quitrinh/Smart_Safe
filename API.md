# API DOCUMENTATION - SMART SAFE SYSTEM

> Tài liệu mô tả danh sách API dùng cho hệ thống két thông minh IoT.

---

## 1. Base URL

```text
https://smart-safe-api.azurewebsites.net](http://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net
```

---

## 2. Kiến trúc giao tiếp API

```text
ESP32 / Mobile App
        │
        ▼
Azure App Service - REST API
        │
        ▼
Azure Database for MySQL
```

---

## 3. Quy ước chung

### Header

```http
Content-Type: application/json
Authorization: Bearer <access_token>
```

### Response thành công

```json
{
  "success": true,
  "message": "Success",
  "data": {}
}
```

### Response thất bại

```json
{
  "success": false,
  "message": "Error message",
  "error": "ERROR_CODE"
}
```

---

# 4. Authentication API

## 4.1 Đăng nhập

```http
POST /api/auth/login
```

### Chức năng

Người dùng đăng nhập vào App.

### Request

```json
{
  "username": "admin",
  "password": "123456"
}
```

### Response

```json
{
  "success": true,
  "message": "Login success",
  "data": {
    "access_token": "jwt_token",
    "user_id": 1,
    "role": "ADMIN"
  }
}
```

### Database liên quan

```text
users
auth_logs
```

---

## 4.2 Đăng xuất

```http
POST /api/auth/logout
```

### Chức năng

Đăng xuất tài khoản khỏi App.

### Response

```json
{
  "success": true,
  "message": "Logout success"
}
```

---

## 4.3 Gửi OTP

```http
POST /api/auth/send-otp
```

### Chức năng

Gửi OTP để xác thực hoặc quên mật khẩu.

### Request

```json
{
  "phone": "0900000000"
}
```

### Response

```json
{
  "success": true,
  "message": "OTP sent"
}
```

### Database liên quan

```text
otp_codes
password_reset_otps
```

---

## 4.4 Xác thực OTP

```http
POST /api/auth/verify-otp
```

### Request

```json
{
  "phone": "0900000000",
  "otp": "123456"
}
```

### Response

```json
{
  "success": true,
  "message": "OTP verified"
}
```

---

# 5. Safe Status API

## 5.1 Lấy trạng thái két

```http
GET /api/safe/status
```

### Chức năng

App lấy trạng thái hiện tại của két.

### Response

```json
{
  "success": true,
  "data": {
    "safe_id": "SAFE001",
    "lock_state": "LOCKED",
    "door_state": "CLOSED",
    "alarm_state": "OFF",
    "network": "WIFI",
    "updated_at": "2026-06-03 20:30:00"
  }
}
```

### Database liên quan

```text
safe_status
```

---

## 5.2 Cập nhật trạng thái két

```http
POST /api/safe/status
```

### Chức năng

ESP32 gửi trạng thái mới lên server.

### Request

```json
{
  "safe_id": "SAFE001",
  "lock_state": "LOCKED",
  "door_state": "CLOSED",
  "alarm_state": "OFF",
  "network": "WIFI"
}
```

### Response

```json
{
  "success": true,
  "message": "Safe status updated"
}
```

---

# 6. Event API

## 6.1 Gửi sự kiện cảnh báo

```http
POST /api/events
```

### Chức năng

ESP32 gửi sự kiện cảnh báo lên API.

### Request

```json
{
  "safe_id": "SAFE001",
  "event_type": "VIBRATION",
  "message": "Phát hiện rung động mạnh",
  "latitude": 10.8231,
  "longitude": 106.6297
}
```

### Response

```json
{
  "success": true,
  "message": "Event saved"
}
```

### Database liên quan

```text
events
notifications
safe_status
```

---

## 6.2 Lấy danh sách sự kiện

```http
GET /api/events
```

### Chức năng

App lấy lịch sử cảnh báo.

### Response

```json
{
  "success": true,
  "data": [
    {
      "id": 1,
      "safe_id": "SAFE001",
      "event_type": "VIBRATION",
      "message": "Phát hiện rung động mạnh",
      "created_at": "2026-06-03 20:30:00"
    }
  ]
}
```

---

# 7. Authentication Log API

## 7.1 Gửi log xác thực

```http
POST /api/auth-log
```

### Chức năng

ESP32 gửi lịch sử xác thực lên server.

### Request

```json
{
  "safe_id": "SAFE001",
  "user_id": 1,
  "method": "FINGERPRINT",
  "status": "SUCCESS",
  "message": "Mở két bằng vân tay thành công"
}
```

### Response

```json
{
  "success": true,
  "message": "Auth log saved"
}
```

### Database liên quan

```text
auth_logs
auth_methods
users
```

---

## 7.2 Lấy lịch sử xác thực

```http
GET /api/auth-log
```

### Chức năng

App lấy lịch sử mở két.

---

# 8. Command API

## 8.1 App gửi lệnh mở két

```http
POST /api/commands/unlock
```

### Chức năng

App gửi yêu cầu mở két từ xa.

### Request

```json
{
  "safe_id": "SAFE001",
  "user_id": 1
}
```

### Response

```json
{
  "success": true,
  "message": "Unlock command created"
}
```

### Database liên quan

```text
safe_commands
auth_logs
safe_status
```

---

## 8.2 App gửi lệnh tắt cảnh báo

```http
POST /api/commands/alarm-off
```

### Request

```json
{
  "safe_id": "SAFE001",
  "user_id": 1
}
```

### Response

```json
{
  "success": true,
  "message": "Alarm off command created"
}
```

---

## 8.3 ESP32 lấy lệnh mới

```http
GET /api/commands?safe_id=SAFE001
```

### Chức năng

ESP32 kiểm tra server có lệnh mới hay không.

### Response

```json
{
  "success": true,
  "data": {
    "command_id": 10,
    "command": "UNLOCK",
    "status": "PENDING"
  }
}
```

---

## 8.4 ESP32 cập nhật lệnh đã xử lý

```http
POST /api/commands/done
```

### Request

```json
{
  "command_id": 10,
  "status": "DONE"
}
```

### Response

```json
{
  "success": true,
  "message": "Command updated"
}
```

---

# 9. Notification API

## 9.1 Lấy danh sách thông báo

```http
GET /api/notifications
```

### Chức năng

App lấy danh sách thông báo.

### Database liên quan

```text
notifications
device_tokens
```

---

## 9.2 Đánh dấu đã đọc

```http
POST /api/notifications/read
```

### Request

```json
{
  "notification_id": 1
}
```

---

# 10. SMS API

## 10.1 Lưu lịch sử SMS

```http
POST /api/sms
```

### Chức năng

ESP32/API lưu lịch sử gửi SMS.

### Request

```json
{
  "safe_id": "SAFE001",
  "phone_number": "0900000000",
  "message": "Canh bao: Ket sat phat hien rung dong!",
  "status": "SENT"
}
```

### Database liên quan

```text
sms_outbox
sms_receivers
sms_recipients
```

---

## 10.2 Lấy lịch sử SMS

```http
GET /api/sms
```

---

# 11. Location API

## 11.1 Cập nhật vị trí GPS

```http
POST /api/location
```

### Chức năng

ESP32 gửi vị trí GPS khi có cảnh báo.

### Request

```json
{
  "safe_id": "SAFE001",
  "latitude": 10.8231,
  "longitude": 106.6297
}
```

### Database liên quan

```text
safe_location_config
events
notifications
```

---

## 11.2 Lấy vị trí mới nhất

```http
GET /api/location?safe_id=SAFE001
```

---

# 12. Config API

## 12.1 Lấy cấu hình hệ thống

```http
GET /api/config
```

### Database liên quan

```text
safe_config
system_config
```

---

## 12.2 Cập nhật cấu hình

```http
POST /api/config
```

### Request

```json
{
  "safe_id": "SAFE001",
  "max_failed_attempts": 3,
  "alarm_duration": 60,
  "wifi_priority": true,
  "sim7600_backup": true
}
```

---

# 13. Luồng API chính

## 13.1 Luồng cảnh báo

```text
ESP32
↓
POST /api/events
↓
API
↓
events + notifications
↓
App
```

## 13.2 Luồng mở két từ App

```text
App
↓
POST /api/commands/unlock
↓
safe_commands
↓
ESP32 GET /api/commands
↓
Servo mở
↓
POST /api/commands/done
```

## 13.3 Luồng cập nhật trạng thái

```text
ESP32
↓
POST /api/safe/status
↓
safe_status
↓
App GET /api/safe/status
```

---

# 14. Mã trạng thái thường dùng

```text
200 OK                  Thành công
201 Created             Tạo dữ liệu thành công
400 Bad Request         Dữ liệu gửi lên sai
401 Unauthorized        Chưa đăng nhập hoặc token sai
403 Forbidden           Không có quyền
404 Not Found           Không tìm thấy dữ liệu
500 Internal Error      Lỗi server
```

---

# 15. Ghi chú bảo mật

- API dùng HTTPS.
- App đăng nhập bằng JWT Token.
- Password lưu dạng hash, không lưu plain text.
- OTP có thời gian hết hạn.
- API kiểm tra quyền trước khi tạo lệnh mở két.
- ESP32 chỉ xử lý lệnh có trạng thái `PENDING`.
- Sau khi xử lý, ESP32 cập nhật lệnh sang `DONE`.
- Toàn bộ sự kiện quan trọng được lưu vào database.

---

# 16. Kết luận

File `API.md` mô tả toàn bộ API chính của hệ thống két thông minh.  
Các API này giúp kết nối:

```text
ESP32 → API → Database → Mobile App
```

Hệ thống hỗ trợ:

- Đăng nhập và OTP.
- Xem trạng thái két.
- Gửi cảnh báo.
- Lưu lịch sử xác thực.
- Mở két từ xa.
- Tắt cảnh báo.
- Lưu SMS.
- Gửi GPS.
- Quản lý cấu hình hệ thống.
