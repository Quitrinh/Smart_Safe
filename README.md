# Smart_Safe
# HỆ THỐNG KÉT THÔNG MINH IoT

> Tài liệu mô tả kiến trúc hệ thống, luồng hoạt động, cấu trúc mã nguồn và quy trình xử lý nhằm phục vụ bảo vệ đồ án.

---

# 1. Tổng quan hệ thống

Hệ thống két thông minh được xây dựng nhằm nâng cao tính an toàn thông qua nhiều phương thức xác thực, giám sát trạng thái két theo thời gian thực và hỗ trợ điều khiển từ xa.

Hệ thống gồm ba phần chính:

- Phần cứng (ESP32 + cảm biến + khóa)
- Backend API (Azure App Service)
- Database (Azure Database for MySQL)
- Mobile App

Toàn bộ dữ liệu được lưu trên Cloud để người dùng có thể giám sát ở bất kỳ đâu.

---

# 2. Kiến trúc tổng thể

```text
                    Người dùng
                         │
                  Mobile Application
                         │
                  HTTP / HTTPS API
                         │
                Azure App Service
                         │
            Azure Database for MySQL
                         ▲
                         │
              WiFi hoặc SIM7600 (4G)
                         │
                      ESP32
                         │
 ┌────────────────────────────────────────────────────┐
 │                                                    │
 │ AS608 Fingerprint                                 │
 │ MFRC522 RFID                                      │
 │ TTP229 Keypad                                     │
 │ MC38 Door Sensor                                  │
 │ SW420 Vibration Sensor                            │
 │ Servo SG92R                                       │
 │ LCD1602                                           │
 │ Buzzer                                            │
 │ GPS GY-NEO                                        │
 └────────────────────────────────────────────────────┘
```

---

# 3. Phần cứng sử dụng

## ESP32

Vai trò:

- Bộ xử lý trung tâm
- Điều khiển toàn bộ thiết bị
- Kết nối Internet
- Gửi dữ liệu lên API
- Nhận lệnh từ App

---

## AS608

Chức năng

- Đăng ký vân tay
- Xác thực người dùng
- Mở khóa

---

## MFRC522

Chức năng

- Đọc UID thẻ RFID
- Kiểm tra quyền truy cập

---

## TTP229

Chức năng

- Nhập mật khẩu
- Đổi mật khẩu

---

## SW420

Chức năng

- Phát hiện rung động
- Phát hiện phá két

---

## MC38

Chức năng

- Phát hiện cửa mở
- Phát hiện cửa bị cạy

---

## Servo SG92R

Chức năng

- Điều khiển chốt khóa

---

## LCD1602

Chức năng

Hiển thị

- Trạng thái
- Mở két
- Đóng két
- Cảnh báo

---

## Buzzer

Chức năng

- Báo thành công
- Báo lỗi
- Báo động

---

## SIM7600

Chức năng

- Gửi SMS
- Gửi dữ liệu Internet qua 4G

---

## GPS GY-NEO

Chức năng

- Lấy vị trí GPS khi có cảnh báo

---

# 4. Kiến trúc phần mềm

```text
ESP32 Firmware
        │
        ▼
REST API (FastAPI)
        │
        ▼
Azure MySQL
        │
        ▼
Flutter Mobile App
```

---

# 5. Luồng khởi động hệ thống

```text
ESP32 khởi động

↓

Khởi tạo GPIO

↓

Khởi tạo LCD

↓

Khởi tạo Servo

↓

Khởi tạo RFID

↓

Khởi tạo Fingerprint

↓

Khởi tạo Keypad

↓

Khởi tạo SW420

↓

Khởi tạo MC38

↓

Khởi tạo SIM7600

↓

Kết nối WiFi

↓

Nếu WiFi lỗi

↓

Chuyển sang SIM7600 Data

↓

Kết nối API

↓

Sẵn sàng hoạt động
```

---

# 6. Các phương thức mở két

Hệ thống hỗ trợ 2 nhóm phương thức mở két:

1. Mở két trực tiếp tại thiết bị:
   - RFID
   - Vân tay
   - Keypad / mật khẩu

2. Mở két từ xa bằng App Mobile

---

# 7. Luồng mở két trực tiếp tại thiết bị

```text
Người dùng xác thực bằng RFID / Vân tay / Keypad

↓

ESP32 nhận dữ liệu xác thực

↓

ESP32 kiểm tra thông tin hợp lệ

↓

Nếu hợp lệ

↓

Servo mở chốt khóa

↓

LCD hiển thị mở két thành công

↓

Buzzer báo thành công

↓

ESP32 gửi auth_logs lên API

↓

API lưu vào Database

↓

App cập nhật lịch sử mở két

Nếu không hợp lệ

↓

Buzzer báo lỗi

↓

Tăng số lần xác thực sai

↓

Lưu auth_logs thất bại

↓

Nếu sai quá số lần cho phép

↓

Chuyển sang trạng thái cảnh báo

↓

Gửi SMS / gửi cảnh báo lên App


```

---

# 8. Luồng mở két gián tiếp qua App

```text
┌───────────────────────┐
│ Người dùng đăng nhập  │
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐
│ App gửi yêu cầu mở két│
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐
│ API xác thực JWT      │
│ và kiểm tra quyền     │
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐
│ Tạo lệnh UNLOCK       │
│ trong safe_commands   │
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐
│ ESP32 nhận lệnh       │
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐
│ Servo mở khóa         │
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐
│ Cập nhật Database     │
│ safe_status           │
│ auth_logs             │
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐
│ App hiển thị thành công│
└───────────────────────┘
```

---

---

# 9. Luồng chống trộm

```text
SW420 phát hiện rung

↓

ESP32

↓

Bật cảnh báo

↓

Buzzer kêu liên tục

↓

LCD hiển thị

↓

Lấy GPS

↓

SIM7600 gửi SMS

↓

HTTP POST

↓

API

↓

events

↓

notifications

↓

App nhận cảnh báo
```

---

# 10. Luồng cửa mở trái phép

```text
MC38 phát hiện cửa

↓

ESP32

↓

Nếu chưa xác thực

↓

Cảnh báo

↓

SMS

↓

API

↓

Database

↓

App
```

---

# 11. Luồng gửi SMS

```text
ESP32

↓

Sinh nội dung

↓

SIM7600

↓

AT Command

↓

SMS

↓

Điện thoại
```

---

# 12. Luồng gửi dữ liệu lên API

```text
ESP32

↓

Tạo JSON

↓

HTTP POST

↓

Azure App Service

↓

Kiểm tra dữ liệu

↓

MySQL

↓

Response

↓

ESP32
```

Ví dụ JSON

```json
{
  "safe_id":"SAFE001",
  "event":"VIBRATION",
  "message":"Phát hiện rung mạnh",
  "latitude":10.8231,
  "longitude":106.6297
}
```

---

# 13. Luồng App

```text
App mở

↓

Đăng nhập

↓

JWT

↓

API

↓

Database

↓

Hiển thị

- Trạng thái két

- Cảnh báo

- Lịch sử

- GPS
```

---

# 14. Luồng mở két từ App

```text
Người dùng

↓

App

↓

POST /unlock

↓

API

↓

Kiểm tra Token

↓

Kiểm tra quyền

↓

safe_commands

↓

ESP32 GET Command

↓

Có lệnh

↓

Servo mở

↓

Gửi trạng thái

↓

safe_status

↓

App cập nhật
```

---

# 15. Luồng WiFi và SIM7600

## Bình thường

```text
ESP32

↓

WiFi

↓

Azure API

↓

Database

↓

App
```

---

## Mất WiFi

```text
ESP32

↓

SIM7600 Data

↓

Azure API

↓

Database

↓

App
```

---

## Khi bị trộm

```text
ESP32

↓

SW420

↓

GPS

↓

SIM7600

↓

SMS

↓

API

↓

Database

↓

Notification

↓

App
```

---

# 16. Cấu trúc Database

## users

Thông tin người dùng

---

## auth_methods

Phương thức xác thực

- RFID
- Fingerprint
- Password

---

## auth_logs

Lịch sử đăng nhập

---

## events

Lưu tất cả sự kiện

Ví dụ

- rung
- mở cửa
- mở két
- đăng nhập

---

## notifications

Thông báo App

---

## device_tokens

Firebase Token

---

## otp_codes

OTP

---

## password_reset_otps

OTP quên mật khẩu

---

## safe_status

Trạng thái hiện tại

Ví dụ

- locked
- unlocked
- alarm

---

## safe_commands

Lệnh từ App

Ví dụ

- unlock
- alarm_off

---

## safe_config

Cấu hình két

---

## safe_location_config

Cấu hình GPS

---

## sms_outbox

Lịch sử SMS

---

## sms_receivers

Danh sách nhận SMS

---

## sms_recipients

Danh sách người nhận

---

## system_config

Cấu hình hệ thống

---

# 17. Azure

## App Service

Chạy REST API

---

## Azure Database for MySQL

Lưu dữ liệu

---

## App Service Plan

Máy chủ chạy API

---

## Application Insights

Theo dõi

- lỗi
- request
- response
- hiệu năng

---

# 18. API chính

```text
POST    /api/login

POST    /api/logout

POST    /api/otp

POST    /api/reset-password

POST    /api/events

POST    /api/auth-log

POST    /api/notification

POST    /api/unlock

POST    /api/alarm-off

GET     /api/status

GET     /api/events

GET     /api/history

GET     /api/location

GET     /api/config
```

---

# 19. Bảo mật

- Password Hash (bcrypt)
- JWT Authentication
- HTTPS
- OTP hết hạn
- Giới hạn số lần đăng nhập
- Lưu auth_logs
- Lưu events
- Lưu notification
- Kiểm tra quyền trước khi mở két

---

# 20. Trình tự hoạt động tổng thể

```text
Người dùng

↓

Fingerprint / RFID / Password

↓

ESP32

↓

Nếu hợp lệ

↓

Servo mở

↓

API

↓

Database

↓

App

=========================

Nếu phát hiện rung

↓

ESP32

↓

GPS

↓

SIM7600

↓

SMS

↓

API

↓

Database

↓

Push Notification

↓

App

=========================

Nếu App yêu cầu mở két

↓

API

↓

safe_commands

↓

ESP32

↓

Servo

↓

safe_status

↓

Database

↓

App
```

---

# 21. Câu hỏi hội đồng thường hỏi

### Tại sao dùng Azure?

- Không phụ thuộc máy tính local.
- Truy cập mọi nơi.
- Dễ mở rộng.
- Có Application Insights giám sát hệ thống.

---

### Tại sao dùng MySQL Cloud?

- Lưu lịch sử.
- Đồng bộ App.
- Không mất dữ liệu khi ESP32 khởi động lại.

---

### Tại sao dùng API?

- ESP32 không truy cập trực tiếp Database.
- API kiểm tra dữ liệu.
- API xác thực người dùng.
- API quản lý quyền truy cập.

---

### Tại sao dùng SIM7600?

- Gửi SMS khẩn cấp.
- Truyền dữ liệu Internet khi WiFi mất.
- Gửi GPS khi có cảnh báo.

---

### Tại sao lưu auth_logs?

Để biết:

- Ai mở két.
- Mở lúc nào.
- Mở bằng phương thức nào.
- Có thất bại hay không.

---

### Tại sao lưu events?

Để lưu toàn bộ lịch sử hệ thống:

- Rung
- Mở cửa
- Đăng nhập
- Cảnh báo
- SMS
- Điều khiển từ App

---

# 22. Kết luận

Hệ thống được thiết kế theo mô hình IoT hiện đại:

ESP32 → API → Azure → Database → Mobile App.

Các chức năng chính gồm:

- Xác thực đa phương thức (vân tay, RFID, mật khẩu).
- Điều khiển khóa bằng servo.
- Giám sát rung và cửa mở trái phép.
- Gửi SMS và vị trí GPS khi có cảnh báo.
- Đồng bộ dữ liệu với Azure Database for MySQL.
- Điều khiển và giám sát từ xa qua ứng dụng di động.
- Lưu lịch sử hoạt động, sự kiện và xác thực phục vụ quản lý và truy vết.