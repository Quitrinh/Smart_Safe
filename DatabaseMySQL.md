# MYSQL COMMANDS - SMART SAFE DATABASE

> Database sử dụng: `smart_safe_db`

---

## 1. Kiểm tra database và bảng

```sql
SHOW DATABASES;

USE smart_safe_db;

SHOW TABLES;
```

---

## 2. Xem cấu trúc tất cả bảng

```sql
DESCRIBE users;
DESCRIBE auth_methods;
DESCRIBE auth_logs;
DESCRIBE events;
DESCRIBE notifications;
DESCRIBE device_tokens;
DESCRIBE otp_codes;
DESCRIBE password_reset_otps;
DESCRIBE safe_commands;
DESCRIBE safe_config;
DESCRIBE safe_location_config;
DESCRIBE safe_status;
DESCRIBE sms_outbox;
DESCRIBE sms_receivers;
DESCRIBE sms_recipients;
DESCRIBE system_config;
```

---

## 3. Xem dữ liệu tất cả bảng

```sql
SELECT * FROM users;
SELECT * FROM auth_methods;
SELECT * FROM auth_logs;
SELECT * FROM events;
SELECT * FROM notifications;
SELECT * FROM device_tokens;
SELECT * FROM otp_codes;
SELECT * FROM password_reset_otps;
SELECT * FROM safe_commands;
SELECT * FROM safe_config;
SELECT * FROM safe_location_config;
SELECT * FROM safe_status;
SELECT * FROM sms_outbox;
SELECT * FROM sms_receivers;
SELECT * FROM sms_recipients;
SELECT * FROM system_config;
```

---

## 4. Đếm số dòng dữ liệu

```sql
SELECT COUNT(*) AS total_users FROM users;
SELECT COUNT(*) AS total_auth_logs FROM auth_logs;
SELECT COUNT(*) AS total_events FROM events;
SELECT COUNT(*) AS total_notifications FROM notifications;
SELECT COUNT(*) AS total_commands FROM safe_commands;
SELECT COUNT(*) AS total_status FROM safe_status;
SELECT COUNT(*) AS total_sms FROM sms_outbox;
```

---

## 5. Truy vấn dữ liệu quan trọng

### Lịch sử cảnh báo mới nhất

```sql
SELECT *
FROM events
ORDER BY created_at DESC
LIMIT 20;
```

### Lịch sử xác thực mới nhất

```sql
SELECT *
FROM auth_logs
ORDER BY created_at DESC
LIMIT 20;
```

### Trạng thái két hiện tại

```sql
SELECT *
FROM safe_status
ORDER BY updated_at DESC
LIMIT 1;
```

### Lệnh mở két chưa xử lý

```sql
SELECT *
FROM safe_commands
WHERE status = 'PENDING'
ORDER BY created_at ASC;
```

---

## 6. Thêm dữ liệu test

### Thêm cảnh báo rung

```sql
INSERT INTO events (safe_id, event_type, message, created_at)
VALUES ('SAFE001', 'VIBRATION', 'Phát hiện rung động mạnh', NOW());
```

### Thêm cảnh báo cửa mở trái phép

```sql
INSERT INTO events (safe_id, event_type, message, created_at)
VALUES ('SAFE001', 'DOOR_OPEN', 'Phát hiện cửa mở trái phép', NOW());
```

### Thêm log xác thực thành công

```sql
INSERT INTO auth_logs (user_id, method, status, message, created_at)
VALUES (1, 'FINGERPRINT', 'SUCCESS', 'Mở két bằng vân tay thành công', NOW());
```

### Thêm log xác thực thất bại

```sql
INSERT INTO auth_logs (user_id, method, status, message, created_at)
VALUES (1, 'PASSWORD', 'FAILED', 'Nhập sai mật khẩu', NOW());
```

### Thêm lệnh mở két từ App

```sql
INSERT INTO safe_commands (safe_id, command, status, created_at)
VALUES ('SAFE001', 'UNLOCK', 'PENDING', NOW());
```

---

## 7. Cập nhật dữ liệu

### ESP32 cập nhật lệnh đã xử lý

```sql
UPDATE safe_commands
SET status = 'DONE',
    executed_at = NOW()
WHERE id = 1;
```

### Cập nhật két đang khóa

```sql
UPDATE safe_status
SET lock_state = 'LOCKED',
    alarm_state = 'OFF',
    updated_at = NOW()
WHERE safe_id = 'SAFE001';
```

### Cập nhật két đang mở

```sql
UPDATE safe_status
SET lock_state = 'UNLOCKED',
    updated_at = NOW()
WHERE safe_id = 'SAFE001';
```

### Bật cảnh báo

```sql
UPDATE safe_status
SET alarm_state = 'ON',
    updated_at = NOW()
WHERE safe_id = 'SAFE001';
```

### Tắt cảnh báo

```sql
UPDATE safe_status
SET alarm_state = 'OFF',
    updated_at = NOW()
WHERE safe_id = 'SAFE001';
```

---

## 8. SMS

### Xem danh sách SMS

```sql
SELECT *
FROM sms_outbox
ORDER BY created_at DESC
LIMIT 20;
```

### Thêm SMS cảnh báo

```sql
INSERT INTO sms_outbox (phone_number, message, status, created_at)
VALUES ('0900000000', 'Canh bao: Ket sat phat hien rung dong!', 'PENDING', NOW());
```

### Cập nhật SMS đã gửi

```sql
UPDATE sms_outbox
SET status = 'SENT',
    sent_at = NOW()
WHERE id = 1;
```

### Xem người nhận SMS

```sql
SELECT * FROM sms_recipients;
SELECT * FROM sms_receivers;
```

### Thêm người nhận SMS

```sql
INSERT INTO sms_recipients (name, phone_number, is_active, created_at)
VALUES ('Chu so huu', '0900000000', 1, NOW());
```

---

## 9. OTP và người dùng

### Xem OTP còn hiệu lực

```sql
SELECT *
FROM otp_codes
WHERE is_used = 0
  AND expired_at > NOW();
```

### Đánh dấu OTP đã dùng

```sql
UPDATE otp_codes
SET is_used = 1
WHERE id = 1;
```

### Xem danh sách user

```sql
SELECT id, username, email, role, created_at
FROM users;
```

### Khóa tài khoản

```sql
UPDATE users
SET status = 'LOCKED'
WHERE id = 1;
```

### Mở khóa tài khoản

```sql
UPDATE users
SET status = 'ACTIVE'
WHERE id = 1;
```

---

## 10. Xóa dữ liệu test

> Cẩn thận trước khi chạy lệnh xóa.

### Xóa dữ liệu test trong events

```sql
DELETE FROM events
WHERE safe_id = 'SAFE001'
  AND event_type = 'TEST';
```

### Xóa toàn bộ dữ liệu log/test

```sql
TRUNCATE TABLE events;
TRUNCATE TABLE auth_logs;
TRUNCATE TABLE notifications;
```

---

## 11. Ghi chú khi bảo vệ đồ án

- `users`: quản lý tài khoản người dùng.
- `auth_methods`: lưu phương thức xác thực như RFID, vân tay, mật khẩu.
- `auth_logs`: lưu lịch sử xác thực.
- `events`: lưu sự kiện cảnh báo.
- `notifications`: lưu thông báo gửi lên App.
- `safe_status`: lưu trạng thái hiện tại của két.
- `safe_commands`: lưu lệnh điều khiển từ App.
- `sms_outbox`: lưu lịch sử SMS.
- `otp_codes`: lưu mã OTP.
- `system_config`: lưu cấu hình hệ thống.