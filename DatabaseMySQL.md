-- 1. Xem tất cả database
SHOW DATABASES;

-- 2. Chọn database đồ án
USE smart_safe_db;

-- 3. Xem tất cả bảng
SHOW TABLES;

-- 4. Xem cấu trúc từng bảng
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

-- 5. Xem dữ liệu từng bảng
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

-- 6. Đếm số dòng trong từng bảng
SELECT COUNT(*) FROM users;
SELECT COUNT(*) FROM auth_logs;
SELECT COUNT(*) FROM events;
SELECT COUNT(*) FROM notifications;
SELECT COUNT(*) FROM safe_commands;
SELECT COUNT(*) FROM safe_status;
SELECT COUNT(*) FROM sms_outbox;

-- 7. Xem lịch sử cảnh báo mới nhất
SELECT *
FROM events
ORDER BY created_at DESC
LIMIT 20;

-- 8. Xem lịch sử xác thực mới nhất
SELECT *
FROM auth_logs
ORDER BY created_at DESC
LIMIT 20;

-- 9. Xem trạng thái két hiện tại
SELECT *
FROM safe_status
ORDER BY updated_at DESC
LIMIT 1;

-- 10. Xem lệnh mở két chưa xử lý
SELECT *
FROM safe_commands
WHERE status = 'PENDING'
ORDER BY created_at ASC;

-- 11. Thêm sự kiện cảnh báo rung
INSERT INTO events (safe_id, event_type, message, created_at)
VALUES ('SAFE001', 'VIBRATION', 'Phát hiện rung động mạnh', NOW());

-- 12. Thêm sự kiện cửa mở trái phép
INSERT INTO events (safe_id, event_type, message, created_at)
VALUES ('SAFE001', 'DOOR_OPEN', 'Phát hiện cửa mở trái phép', NOW());

-- 13. Thêm log xác thực thành công
INSERT INTO auth_logs (user_id, method, status, message, created_at)
VALUES (1, 'FINGERPRINT', 'SUCCESS', 'Mở két bằng vân tay thành công', NOW());

-- 14. Thêm log xác thực thất bại
INSERT INTO auth_logs (user_id, method, status, message, created_at)
VALUES (1, 'PASSWORD', 'FAILED', 'Nhập sai mật khẩu', NOW());

-- 15. Thêm lệnh mở két từ app
INSERT INTO safe_commands (safe_id, command, status, created_at)
VALUES ('SAFE001', 'UNLOCK', 'PENDING', NOW());

-- 16. ESP32 cập nhật lệnh đã xử lý
UPDATE safe_commands
SET status = 'DONE', executed_at = NOW()
WHERE id = 1;

-- 17. Cập nhật trạng thái két đang khóa
UPDATE safe_status
SET lock_state = 'LOCKED',
    alarm_state = 'OFF',
    updated_at = NOW()
WHERE safe_id = 'SAFE001';

-- 18. Cập nhật trạng thái két đang mở
UPDATE safe_status
SET lock_state = 'UNLOCKED',
    updated_at = NOW()
WHERE safe_id = 'SAFE001';

-- 19. Cập nhật trạng thái cảnh báo
UPDATE safe_status
SET alarm_state = 'ON',
    updated_at = NOW()
WHERE safe_id = 'SAFE001';

-- 20. Tắt cảnh báo
UPDATE safe_status
SET alarm_state = 'OFF',
    updated_at = NOW()
WHERE safe_id = 'SAFE001';

-- 21. Xem danh sách SMS đã gửi/chờ gửi
SELECT *
FROM sms_outbox
ORDER BY created_at DESC
LIMIT 20;

-- 22. Thêm SMS cảnh báo vào hàng chờ
INSERT INTO sms_outbox (phone_number, message, status, created_at)
VALUES ('0900000000', 'Canh bao: Ket sat phat hien rung dong!', 'PENDING', NOW());

-- 23. Cập nhật SMS đã gửi
UPDATE sms_outbox
SET status = 'SENT', sent_at = NOW()
WHERE id = 1;

-- 24. Xem người nhận SMS
SELECT * FROM sms_recipients;
SELECT * FROM sms_receivers;

-- 25. Thêm người nhận SMS
INSERT INTO sms_recipients (name, phone_number, is_active, created_at)
VALUES ('Chu so huu', '0900000000', 1, NOW());

-- 26. Xem OTP còn hiệu lực
SELECT *
FROM otp_codes
WHERE is_used = 0
AND expired_at > NOW();

-- 27. Đánh dấu OTP đã dùng
UPDATE otp_codes
SET is_used = 1
WHERE id = 1;

-- 28. Xem user
SELECT id, username, email, role, created_at
FROM users;

-- 29. Khóa tài khoản user
UPDATE users
SET status = 'LOCKED'
WHERE id = 1;

-- 30. Mở khóa tài khoản user
UPDATE users
SET status = 'ACTIVE'
WHERE id = 1;

-- 31. Xóa dữ liệu test trong events
DELETE FROM events
WHERE safe_id = 'SAFE001'
AND event_type = 'TEST';

-- 32. Xóa toàn bộ dữ liệu trong bảng test/log
TRUNCATE TABLE events;
TRUNCATE TABLE auth_logs;
TRUNCATE TABLE notifications;