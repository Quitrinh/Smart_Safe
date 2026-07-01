# DEPLOYMENT GUIDE - SMART SAFE API

## 1. Mục tiêu

Triển khai API của hệ thống két thông minh lên Microsoft Azure để ESP32, SIM7600 và Mobile App có thể truy cập qua Internet.

---

## 2. Kiến trúc deploy

```text
Source Code
↓
GitHub / Local Project
↓
Azure App Service
↓
Azure Database for MySQL
↓
Mobile App / ESP32
```

---

## 3. Tài nguyên Azure sử dụng

```text
smart-safe-api       → App Service chạy API
smart-safe-plan      → App Service Plan
smart-safe-db        → Azure Database for MySQL
Application Insights → Theo dõi log và lỗi API
```

---

## 4. Chuẩn bị source code API

Cấu trúc thư mục đề xuất:

```text
smart-safe-api/
├── main.py
├── requirements.txt
├── .env
├── README.md
├── API.md
└── DEPLOYMENT.md
```

---

## 5. File requirements.txt

```txt
fastapi
uvicorn
pymysql
python-dotenv
bcrypt
pyjwt
```

---

## 6. Cấu hình biến môi trường trên Azure

Vào:

```text
Azure Portal
↓
App Service
↓
smart-safe-api
↓
Settings
↓
Environment variables
```

Thêm:

```text
DB_HOST=smart-safe-db.mysql.database.azure.com
DB_PORT=3306
DB_NAME=smart_safe_db
DB_USER=your_user
DB_PASSWORD=your_password
JWT_SECRET=your_secret_key
```

Không nên ghi mật khẩu database trực tiếp trong code.

---

## 7. Kết nối Azure MySQL

Thông tin kết nối:

```text
Host: smart-safe-db.mysql.database.azure.com
Port: 3306
Database: smart_safe_db
SSL: Require
```

Kiểm tra database:

```sql
USE smart_safe_db;
SHOW TABLES;
```

---

## 8. Deploy bằng Azure Portal

Vào:

```text
App Service
↓
Deployment Center
↓
Source
↓
GitHub hoặc Local Git
↓
Save
```

Azure sẽ tự build và chạy API.

---

## 9. Deploy bằng Azure CLI

Đăng nhập:

```bash
az login
```

Deploy:

```bash
az webapp up \
  --name smart-safe-api \
  --resource-group smart-safe-rg \
  --runtime "PYTHON:3.11"
```

---

## 10. Startup command

Trong App Service:

```text
Settings
↓
Configuration
↓
General settings
↓
Startup Command
```

Nhập:

```bash
uvicorn main:app --host 0.0.0.0 --port 8000
```

---

## 11. Kiểm tra API sau deploy

Mở trình duyệt:

```text
https://smart-safe-api.azurewebsites.net
```

Swagger UI:

```text
https://smart-safe-api.azurewebsites.net/docs
```

Test endpoint:

```http
GET /api/safe/status
```

---

## 12. Kiểm tra log khi lỗi

Vào:

```text
App Service
↓
Monitoring
↓
Log stream
```

Hoặc:

```text
Application Insights
↓
Failures
↓
Transaction search
```

---

## 13. Luồng sau khi deploy thành công

```text
ESP32 / SIM7600
↓
POST https://smart-safe-api.azurewebsites.net/api/events
↓
Azure App Service
↓
Azure MySQL
↓
Mobile App
```

---

## 14. Lưu ý bảo mật

- Không public mật khẩu database.
- Dùng HTTPS.
- Dùng JWT cho App.
- Chỉ cho phép API truy cập database.
- Không cho ESP32 kết nối trực tiếp MySQL.
- Cấu hình Firewall MySQL phù hợp.
- Backup database định kỳ.

---

## 15. Kết luận

Sau khi deploy, API chạy trên Azure App Service và có thể truy cập từ Internet.  
ESP32, SIM7600 và Mobile App đều giao tiếp với hệ thống thông qua REST API.