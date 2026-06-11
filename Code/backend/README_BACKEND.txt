# SMART SAFE BACKEND

## Tổng quan

Backend cho hệ thống két thông minh.

Chức năng:

* Đăng nhập
* OTP mở két
* Điều khiển mở két từ xa
* Quản lý người dùng
* Quản lý số nhận SMS
* Lưu lịch sử sự kiện
* Đồng bộ ESP32 ↔ MySQL ↔ Flutter

---

# 1. Yêu cầu

Cài đặt:

* Node.js LTS
* npm
* MySQL Server
* MySQL Workbench
* VS Code
* Thunder Client

Kiểm tra:

```bash
node -v
npm -v
mysql --version
```

---

# 2. Tạo project

```bash
mkdir smart-safe-backend
cd smart-safe-backend

npm init -y
```

Cài thư viện:

```bash
npm install express mysql2 cors dotenv bcryptjs jsonwebtoken
npm install --save-dev nodemon
```

---

# 3. Cấu trúc thư mục

```text
smart-safe-backend
│
├── server.js
├── db.js
├── database.sql
├── .env
├── package.json
└── README_BACKEND.md
```

---

# 4. File .env

```env
PORT=3000

DB_HOST=127.0.0.1
DB_PORT=3306
DB_USER=root
DB_PASSWORD=12345678
DB_NAME=smart_safe_db

JWT_SECRET=smart_safe_secret_key
```

---

# 5. Database

Chạy file:

```bash
mysql -u root -p < database.sql
```

Hoặc mở MySQL Workbench và chạy nội dung file database.sql.

---

# 6. db.js

Dán nội dung file db.js tại đây.

---

# 7. server.js

Dán nội dung file server.js tại đây.

---

# 8. Chạy backend

Chạy thường:

```bash
npm start
```

Chạy chế độ phát triển:

```bash
npm run dev
```

Kết quả:

```text
MYSQL CONNECTED
SMART SAFE BACKEND RUNNING ON PORT 3000
```

---

# 9. Test bằng trình duyệt

```text
http://localhost:3000
```

Kết quả:

```json
{
  "message": "SMART SAFE API OK"
}
```

---

# 10. Test Thunder Client

## Login

POST

```text
http://localhost:3000/api/login
```

Body:

```json
{
  "username":"admin",
  "password":"123456"
}
```

---

## Tạo OTP

POST

```text
http://localhost:3000/api/request-open-otp
```

Body:

```json
{
  "user_id":1
}
```

---

## Xác nhận OTP

POST

```text
http://localhost:3000/api/verify-otp-open
```

Body:

```json
{
  "user_id":1,
  "otp":"123456"
}
```

---

## ESP32 lấy lệnh

GET

```text
http://localhost:3000/api/esp32/commands
```

---

## ESP32 báo hoàn thành

POST

```text
http://localhost:3000/api/esp32/command-done
```

Body:

```json
{
  "command_id":1,
  "status":"done"
}
```

---

## Gửi sự kiện

POST

```text
http://localhost:3000/api/events
```

Body:

```json
{
  "event_type":"VIBRATION",
  "message":"Phat hien rung ket",
  "gps_lat":10.123,
  "gps_lng":106.123,
  "network_type":"WIFI"
}
```

---

# 11. ESP32 kết nối backend

Lấy IP máy tính:

```bash
ipconfig getifaddr en0
```

Ví dụ:

```text
192.168.1.6
```

ESP32 gọi:

```text
http://192.168.1.6:3000/api/esp32/commands
```

Không dùng:

```text
http://localhost:3000
```

---

# 12. Luồng hoạt động

```text
Flutter
    ↓
Login
    ↓
OTP
    ↓
Backend
    ↓
safe_commands
    ↓
ESP32
    ↓
Servo Open
    ↓
Command Done
    ↓
MySQL
```

---

# 13. Chức năng tương lai

* Quản lý RFID
* Quản lý vân tay
* Quản lý mật khẩu két
* Quản lý số nhận SMS
* GPS Tracking
* WiFi → SIM 4G Failover
* Điều khiển máy bơm chữa cháy
* Thông báo đẩy Flutter
* Dashboard quản trị
* Phân quyền Admin/User
* Nhật ký hoạt động
* Biểu đồ thống kê

```
```
