const express = require("express");
const cors = require("cors");
const bcrypt = require("bcryptjs");
const jwt = require("jsonwebtoken");
require("dotenv").config();

const db = require("./db");
const firebaseAdmin = require("./firebase-admin");
const app = express();

app.use(cors());
app.use(express.json());

const PORT = process.env.PORT || 3000;
const JWT_SECRET = process.env.JWT_SECRET || "smart_safe_secret_key";
const JWT_EXPIRES_IN = process.env.JWT_EXPIRES_IN || "7d";

function createOtp() {
  return Math.floor(100000 + Math.random() * 900000).toString();
}

function isValidPassword(password) {
  return typeof password === "string" && password.length >= 6;
}

function signToken(user) {
  return jwt.sign(
    {
      id: user.id,
      username: user.username,
      role: user.role,
    },
    JWT_SECRET,
    { expiresIn: JWT_EXPIRES_IN }
  );
}

async function authRequired(req, res, next) {
  try {
    const header = req.headers.authorization || "";
    const token = header.startsWith("Bearer ") ? header.slice(7) : null;

    if (!token) {
      return res.status(401).json({
        success: false,
        message: "Missing token",
      });
    }

    const decoded = jwt.verify(token, JWT_SECRET);

    const [rows] = await db.query(
      `SELECT id, full_name, username, email, phone, role, status
       FROM users
       WHERE id = ? AND status = 'active'
       LIMIT 1`,
      [decoded.id]
    );

    if (rows.length === 0) {
      return res.status(401).json({
        success: false,
        message: "User khong hop le hoac da bi khoa",
      });
    }

    req.user = rows[0];
    next();
  } catch (err) {
    return res.status(401).json({
      success: false,
      message: "Token khong hop le hoac da het han",
    });
  }
}

function adminRequired(req, res, next) {
  if (!req.user || req.user.role !== "admin") {
    return res.status(403).json({
      success: false,
      message: "Chi admin moi co quyen thuc hien",
    });
  }

  next();
}

// ===============================
// TEST API
// ===============================
app.get("/", (req, res) => {
  res.json({
    success: true,
    message: "SMART SAFE API OK",
  });
});

// ===============================
// TAO OTP MO KET
// ===============================
app.post("/api/request-open-otp", authRequired, async (req, res) => {
  try {
    const user_id = req.user.id;

    const otp = createOtp();
    const expiredAt = new Date(Date.now() + 5 * 60 * 1000);

    await db.query(
      `INSERT INTO otp_codes(user_id, otp_code, purpose, expired_at, used)
       VALUES (?, ?, 'OPEN_SAFE', ?, 0)`,
      [user_id, otp, expiredAt]
    );

    const response = {
      success: true,
      message: "OTP mo ket da duoc tao",
    };

    if (process.env.NODE_ENV !== "production") {
      response.debug_otp = otp;
    }

    res.json(response);
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// ===============================
// XAC NHAN OTP VA TAO LENH MO KET
// ===============================
app.post("/api/verify-otp-open", authRequired, async (req, res) => {
  try {
    const user_id = req.user.id;
    const { otp } = req.body;

    const [rows] = await db.query(
      `SELECT * FROM otp_codes
       WHERE user_id = ?
       AND otp_code = ?
       AND purpose = 'OPEN_SAFE'
       AND used = 0
       AND expired_at > NOW()
       ORDER BY id DESC
       LIMIT 1`,
      [user_id, otp]
    );

    if (rows.length === 0) {
      return res.status(400).json({
        success: false,
        message: "OTP sai hoac da het han",
      });
    }

    await db.query("UPDATE otp_codes SET used = 1 WHERE id = ?", [
      rows[0].id,
    ]);

    await db.query(
      `INSERT INTO safe_commands(command, command_value, status, created_by)
       VALUES ('OPEN_SAFE', 'OPEN', 'pending', ?)`,
      [user_id]
    );

    await db.query(
      `INSERT INTO events(event_type, message, network_type, status)
       VALUES ('UNLOCK_REQUEST', ?, 'APP', 'active')`,
      [`${req.user.username} da xac thuc OTP va tao lenh mo ket`]
    );

    res.json({
      success: true,
      message: "Da tao lenh mo ket",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// ===============================
// ESP32 LAY LENH MO KET
// ===============================
app.get("/api/esp32/commands", async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT * FROM safe_commands
       WHERE status = 'pending'
       ORDER BY id ASC
       LIMIT 1`
    );

    if (rows.length === 0) {
      return res.json({
        success: true,
        command: null,
      });
    }

    res.json({
      success: true,
      command: rows[0],
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// ===============================
// ESP32 BAO DA CHAY LENH
// ===============================
app.post("/api/esp32/command-done", async (req, res) => {
  try {
    const { command_id, status } = req.body;

    await db.query("UPDATE safe_commands SET status = ? WHERE id = ?", [
      status || "done",
      command_id,
    ]);

    res.json({
      success: true,
      message: "Command updated",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
async function createNotification(user_id, title, body, type = "SYSTEM", ref_id = null) {
  await db.query(
    `INSERT INTO notifications(user_id, title, body, type, ref_id)
     VALUES (?, ?, ?, ?, ?)`,
    [user_id, title, body, type, ref_id]
  );
}

async function createNotificationForAdmins(title, body, type = "ALARM", ref_id = null) {
  const [admins] = await db.query(
    `SELECT id
     FROM users
     WHERE role = 'admin'
     AND status = 'active'`
  );

  for (const admin of admins) {
    await createNotification(admin.id, title, body, type, ref_id);
  }
}
async function sendPushToUser(userId, title, body, type = "SYSTEM", refId = null) {
  try {
    if (!firebaseAdmin) {
      console.log("[FCM] Firebase Admin not initialized");
      return;
    }

    const [tokens] = await db.query(
      `SELECT device_token
       FROM device_tokens
       WHERE user_id = ?
       AND status = 'active'`,
      [userId]
    );

    if (tokens.length === 0) {
      console.log(`[FCM] No active token for user ${userId}`);
      return;
    }

    const tokenList = tokens.map((t) => t.device_token);

    const response = await firebaseAdmin.messaging().sendEachForMulticast({
      tokens: tokenList,
      notification: {
        title,
        body,
      },
      data: {
        type: String(type || "SYSTEM"),
        ref_id: refId ? String(refId) : "",
      },
      android: {
        priority: "high",
        notification: {
          channelId: "smart_safe_alerts",
          sound: "default",
        },
      },
    });

    console.log(
      `[FCM] Sent to user ${userId}: success=${response.successCount}, failed=${response.failureCount}`
    );

    for (let i = 0; i < response.responses.length; i++) {
      const r = response.responses[i];

      if (!r.success) {
        const errorCode = r.error?.code || "";
        console.log("[FCM TOKEN ERROR]", errorCode);

        if (
          errorCode.includes("registration-token-not-registered") ||
          errorCode.includes("invalid-registration-token")
        ) {
          await db.query(
            `UPDATE device_tokens
             SET status = 'inactive'
             WHERE device_token = ?`,
            [tokenList[i]]
          );
        }
      }
    }
  } catch (err) {
    console.error("[FCM SEND USER ERROR]", err.message);
  }
}

async function sendPushToAdmins(title, body, type = "ALARM", refId = null) {
  try {
    const [admins] = await db.query(
      `SELECT id
       FROM users
       WHERE role = 'admin'
       AND status = 'active'`
    );

    for (const admin of admins) {
      await sendPushToUser(admin.id, title, body, type, refId);
    }
  } catch (err) {
    console.error("[FCM SEND ADMINS ERROR]", err.message);
  }
}
// ===============================
// ESP32 GUI EVENT LEN SERVER
// ===============================
app.post("/api/events", async (req, res) => {
  try {
    const { event_type, message, gps_lat, gps_lng, network_type } = req.body;

    const finalEventType = (event_type || "UNKNOWN").toUpperCase();
    const finalMessage = message || "";

    const [result] = await db.query(
      `INSERT INTO events(event_type, message, gps_lat, gps_lng, network_type, status)
       VALUES (?, ?, ?, ?, ?, 'active')`,
      [
        finalEventType,
        finalMessage,
        gps_lat ?? null,
        gps_lng ?? null,
        network_type || "WIFI",
      ]
    );

const alarmKeywords = [
  "INTRUSION",
  "VIBRATION",
  "DOOR",
  "GAS",
  "SMOKE",
  "FIRE",
  "FLAME",
  "UNLOCK_FAILED",
  "WRONG_PASSWORD",
  "SAFE_MOVED",
  "ALARM",
];

const isAlarmEvent = alarmKeywords.some((key) =>
  finalEventType.includes(key)
);

if (isAlarmEvent) {
  const title = "Cảnh báo két thông minh";
  const body =
    finalMessage || `Phát hiện sự kiện bất thường: ${finalEventType}`;

  try {
    await createNotificationForAdmins(
      title,
      body,
      finalEventType,
      result.insertId
    );
  } catch (notifyErr) {
    console.error("[NOTIFICATION ERROR]", notifyErr.message);
  }

  try {
    await sendPushToAdmins(
      title,
      body,
      finalEventType,
      result.insertId
    );
  } catch (pushErr) {
    console.error("[PUSH ERROR]", pushErr.message);
  }
}

    res.json({
      success: true,
      message: "Event saved",
      event_id: result.insertId,
    });

  } catch (err) {
    console.error("[SAVE EVENT ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
// ===============================
// APP LAY LICH SU EVENT
// GET /api/events
// GET /api/events?date=2026-06-05
// ===============================
app.get("/api/events", async (req, res) => {
  try {
    const { date, status } = req.query;

    let sql = `
      SELECT id, event_type, message, gps_lat, gps_lng, network_type, status,
             CONVERT_TZ(created_at, '+00:00', '+07:00') AS created_at
      FROM events
    `;
    const params = [];

    if (status) {
      sql += " WHERE status = ?";
      params.push(status);
    } else {
      sql += " WHERE status = 'active'";
    }

    if (date) {
      sql += status ? " AND DATE(CONVERT_TZ(created_at, '+00:00', '+07:00')) = ?" 
                    : " AND DATE(CONVERT_TZ(created_at, '+00:00', '+07:00')) = ?";
      params.push(date);
    }

    sql += " ORDER BY created_at DESC LIMIT 200";

    const [rows] = await db.query(sql, params);

    res.json({
      success: true,
      data: rows,
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
// ===============================
// SMS RECEIVERS
// ===============================
app.get("/api/sms-receivers", async (req, res) => {
  try {
    const [rows] = await db.query(
      "SELECT * FROM sms_receivers WHERE is_active = 1 ORDER BY id DESC"
    );

    res.json({
      success: true,
      data: rows,
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

app.post("/api/sms-receivers", async (req, res) => {
  try {
    const { name, phone } = req.body;

    await db.query(
      `INSERT INTO sms_receivers(name, phone, receive_alarm, is_active)
       VALUES (?, ?, 1, 1)`,
      [name, phone]
    );

    res.json({
      success: true,
      message: "Da them so SMS",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

app.delete("/api/sms-receivers/:id", async (req, res) => {
  try {
    const id = req.params.id;

    await db.query("UPDATE sms_receivers SET is_active = 0 WHERE id = ?", [
      id,
    ]);

    res.json({
      success: true,
      message: "Da xoa so SMS",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// ===============================
// CONFIG
// ===============================
app.get("/api/config", async (req, res) => {
  try {
    const [rows] = await db.query(
      "SELECT * FROM safe_config ORDER BY id ASC"
    );

    res.json({
      success: true,
      data: rows,
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

app.post("/api/config", async (req, res) => {
  try {
    const { config_key, config_value } = req.body;

    if (!config_key) {
      return res.status(400).json({
        success: false,
        message: "config_key is required",
      });
    }

    await db.query(
      `INSERT INTO safe_config(config_key, config_value)
       VALUES (?, ?)
       ON DUPLICATE KEY UPDATE
       config_value = VALUES(config_value),
       updated_at = CURRENT_TIMESTAMP`,
      [config_key, config_value]
    );

    res.json({
      success: true,
      message: "Config updated",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// ===============================
// SAFE STATUS
// ===============================
app.get("/api/safe/status", async (req, res) => {
  try {
    const [rows] = await db.query("SELECT * FROM safe_status WHERE id = 1");

    res.json({
      success: true,
      data: rows[0] || null,
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

app.post("/api/safe/status", async (req, res) => {
  try {
    const {
      safe_state,
      wifi_status,
      sim_status,
      gps_status,
      alarm_status,
      flame_status,
      pump_status,
    } = req.body;

    await db.query(
      `INSERT INTO safe_status
       (id, safe_state, wifi_status, sim_status, gps_status, alarm_status, flame_status, pump_status)
       VALUES (1, ?, ?, ?, ?, ?, ?, ?)
       ON DUPLICATE KEY UPDATE
       safe_state = VALUES(safe_state),
       wifi_status = VALUES(wifi_status),
       sim_status = VALUES(sim_status),
       gps_status = VALUES(gps_status),
       alarm_status = VALUES(alarm_status),
       flame_status = VALUES(flame_status),
       pump_status = VALUES(pump_status),
       updated_at = CURRENT_TIMESTAMP`,
      [
        safe_state || "LOCKED",
        wifi_status || "ONLINE",
        sim_status || "READY",
        gps_status || "NO_FIX",
        alarm_status || "OFF",
        flame_status || "NORMAL",
        pump_status || "OFF",
      ]
    );

    res.json({
      success: true,
      message: "Safe status updated",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// ===============================
// GET AUTH METHODS
// ===============================
app.get("/api/auth-methods", async (req, res) => {
  try {
    const [rows] = await db.query(
      "SELECT * FROM auth_methods WHERE status = 'active' ORDER BY id DESC"
    );

    res.json({
      success: true,
      data: rows
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// TẠO AUTH METHOD (ADD / ENROLL)
// ===============================
app.post("/api/auth-methods/enroll", async (req, res) => {
  try {
    const { user_name, method_type } = req.body;

    if (!user_name || !method_type) {
      return res.status(400).json({
        success: false,
        message: "Missing user_name or method_type"
      });
    }

    const command = method_type === "RFID" ? "ADD_RFID" : "ADD_FINGER";

    await db.query(
      `INSERT INTO safe_commands(command, command_value, status)
       VALUES (?, ?, 'pending')`,
      [
        command,
        JSON.stringify({ user_name, method_type })
      ]
    );

    res.json({
      success: true,
      message: "Enroll command created"
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// ESP32 GỬI KẾT QUẢ ENROLL
// ===============================
app.post("/api/auth-methods/enroll-result", async (req, res) => {
  try {
    const { user_name, method_type, method_value, command_id } = req.body;

    // Kiểm tra đã tồn tại chưa (chỉ tính active)
    const [exists] = await db.query(
      `SELECT id, user_name
       FROM auth_methods
       WHERE method_type = ? AND method_value = ? AND status = 'active'
       LIMIT 1`,
      [method_type, method_value]
    );

    if (exists.length > 0) {
      return res.status(409).json({
        success: false,
        message: `Đã được gán cho ${exists[0].user_name}`
      });
    }

    // Thêm auth method mới
    await db.query(
      `INSERT INTO auth_methods(user_name, method_type, method_value, status)
       VALUES (?, ?, ?, 'active')`,
      [user_name, method_type, method_value]
    );

    // Cập nhật trạng thái lệnh nếu có
    if (command_id) {
      await db.query(
        "UPDATE safe_commands SET status = 'done' WHERE id = ?",
        [command_id]
      );
    }

    res.json({
      success: true,
      message: "Auth method saved"
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// KIỂM TRA AUTH METHOD
// ===============================
app.post("/api/auth-methods/check", async (req, res) => {
  try {
    const { method_type, method_value } = req.body;

    if (!method_type || !method_value) {
      return res.status(400).json({
        valid: false,
        message: 'Missing method_type or method_value'
      });
    }

    const [rows] = await db.query(
      `SELECT *
       FROM auth_methods
       WHERE method_type = ? AND method_value = ? AND status = 'active'
       LIMIT 1`,
      [method_type, method_value]
    );

    if (rows.length > 0) {
      return res.json({
        valid: true,
        user_name: rows[0].user_name
      });
    }

    return res.json({
      valid: false
    });

  } catch (err) {
    console.error('[CHECK AUTH ERROR]', err);
    return res.status(500).json({
      valid: false,
      message: 'Server error'
    });
  }
});

// ===============================
// XÓA AUTH METHOD
// ===============================
app.post("/api/auth-methods/remove", async (req, res) => {
  try {
    const { method_type, method_value } = req.body;

    if (!method_type || !method_value) {
      return res.status(400).json({
        success: false,
        message: "Missing method_type or method_value"
      });
    }

    // Kiểm tra auth method tồn tại và đang active
    const [rows] = await db.query(
      `SELECT id
       FROM auth_methods
       WHERE method_type = ? AND method_value = ? AND status = 'active'
       LIMIT 1`,
      [method_type, method_value]
    );

    if (rows.length === 0) {
      return res.status(404).json({
        success: false,
        message: "Auth method not found"
      });
    }

    // Xóa bằng cách đổi status = 'deleted'
    await db.query(
      `UPDATE auth_methods
       SET status='deleted', updated_at=NOW()
       WHERE id = ?`,
      [rows[0].id]
    );

    res.json({
      success: true,
      message: `${method_type} removed successfully`
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});
// POST /api/events/remove
app.post("/api/events/remove", async (req, res) => {
  try {
    const { ids } = req.body;

    if (!Array.isArray(ids) || ids.length === 0) {
      return res.status(400).json({
        success: false,
        message: "Missing ids",
      });
    }

    await db.query(
      "UPDATE events SET status='deleted' WHERE id IN (?)",
      [ids]
    );

    res.json({
      success: true,
      message: `Deleted ${ids.length} events`,
    });

  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// Optional: Remove all events
app.post("/api/events/remove-all", async (req, res) => {
  try {
    await db.query("UPDATE events SET status='deleted', updated_at=NOW() WHERE status='active'");
    res.json({
      success: true,
      message: "All active events have been deleted",
    });
  } catch (err) {
    console.error("[REMOVE ALL EVENTS ERROR]", err);
    res.status(500).json({ success: false, error: err.message });
  }
});

app.post("/api/events/restore", async (req, res) => {
  try {
    const { ids } = req.body; // ids = [44, 45, 46]

    if (!Array.isArray(ids) || ids.length === 0) {
      return res.status(400).json({ success: false, message: "Missing ids" });
    }

    // Tạo placeholders để expand từng id
    const placeholders = ids.map(() => "?").join(", ");
    await db.query(
      `UPDATE events SET status='active' WHERE id IN (${placeholders})`,
      ids
    );

    res.json({
      success: true,
      message: `Restored ${ids.length} events`,
    });
  } catch (err) {
    console.error("[RESTORE EVENTS ERROR]", err);
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// AUTH - TAO ADMIN DAU TIEN
// ===============================
app.post("/api/auth/setup-admin", async (req, res) => {
  try {
    const { full_name, username, email, phone, password } = req.body;

    if (!full_name || !username || !password) {
      return res.status(400).json({
        success: false,
        message: "Missing full_name, username or password",
      });
    }

    if (!isValidPassword(password)) {
      return res.status(400).json({
        success: false,
        message: "Mat khau phai co it nhat 6 ky tu",
      });
    }

    const [countRows] = await db.query("SELECT COUNT(*) AS total FROM users");

    if (countRows[0].total > 0) {
      return res.status(403).json({
        success: false,
        message: "He thong da co user, khong the tao admin dau tien",
      });
    }

    const passwordHash = await bcrypt.hash(password, 10);

    await db.query(
      `INSERT INTO users(full_name, username, email, phone, password_hash, role, status)
       VALUES (?, ?, ?, ?, ?, 'admin', 'active')`,
      [full_name, username, email || null, phone || null, passwordHash]
    );

    res.json({
      success: true,
      message: "Admin dau tien da duoc tao",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// AUTH - LOGIN
// ===============================
app.post("/api/auth/login", async (req, res) => {
  try {
    const { login, password } = req.body;

    if (!login || !password) {
      return res.status(400).json({
        success: false,
        message: "Missing login or password",
      });
    }

    const [rows] = await db.query(
      `SELECT *
       FROM users
       WHERE (username = ? OR email = ? OR phone = ?)
       AND status <> 'deleted'
       LIMIT 1`,
      [login, login, login]
    );

    if (rows.length === 0) {
      return res.status(401).json({
        success: false,
        message: "Tai khoan hoac mat khau khong dung",
      });
    }

    const user = rows[0];

    if (
      user.status === "locked" &&
      user.locked_until &&
      new Date(user.locked_until) > new Date()
    ) {
      return res.status(423).json({
        success: false,
        message: "Tai khoan dang bi khoa tam thoi",
      });
    }

    const ok = await bcrypt.compare(password, user.password_hash);

    if (!ok) {
      const failed = (user.failed_login_attempts || 0) + 1;

      if (failed >= 5) {
        const lockedUntil = new Date(Date.now() + 15 * 60 * 1000);

        await db.query(
          `UPDATE users
           SET failed_login_attempts = ?, status = 'locked', locked_until = ?
           WHERE id = ?`,
          [failed, lockedUntil, user.id]
        );

        return res.status(423).json({
          success: false,
          message: "Sai mat khau qua 5 lan, tai khoan bi khoa 15 phut",
        });
      }

      await db.query(
        `UPDATE users SET failed_login_attempts = ? WHERE id = ?`,
        [failed, user.id]
      );

      return res.status(401).json({
        success: false,
        message: "Tai khoan hoac mat khau khong dung",
      });
    }

    await db.query(
      `UPDATE users
       SET failed_login_attempts = 0,
           locked_until = NULL,
           status = 'active',
           last_login_at = NOW()
       WHERE id = ?`,
      [user.id]
    );

    await db.query(
      `INSERT INTO events(event_type, message, network_type, status)
       VALUES ('LOGIN_SUCCESS', ?, 'APP', 'active')`,
      [`${user.username} da dang nhap thanh cong`]
    );

    const token = signToken(user);

    res.json({
      success: true,
      message: "Dang nhap thanh cong",
      token,
      user: {
        id: user.id,
        full_name: user.full_name,
        username: user.username,
        email: user.email,
        phone: user.phone,
        role: user.role,
      },
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// AUTH - THONG TIN USER DANG NHAP
// ===============================
app.get("/api/auth/me", authRequired, async (req, res) => {
  res.json({
    success: true,
    data: req.user,
  });
});

// ===============================
// AUTH - DOI MAT KHAU KHI DA LOGIN
// ===============================
app.post("/api/auth/change-password", authRequired, async (req, res) => {
  try {
    const { old_password, new_password } = req.body;

    if (!old_password || !new_password) {
      return res.status(400).json({
        success: false,
        message: "Missing old_password or new_password",
      });
    }

    if (!isValidPassword(new_password)) {
      return res.status(400).json({
        success: false,
        message: "Mat khau moi phai co it nhat 6 ky tu",
      });
    }

    const [rows] = await db.query(
      "SELECT password_hash FROM users WHERE id = ? LIMIT 1",
      [req.user.id]
    );

    const ok = await bcrypt.compare(old_password, rows[0].password_hash);

    if (!ok) {
      return res.status(400).json({
        success: false,
        message: "Mat khau cu khong dung",
      });
    }

    const hash = await bcrypt.hash(new_password, 10);

    await db.query(
      "UPDATE users SET password_hash = ? WHERE id = ?",
      [hash, req.user.id]
    );

    res.json({
      success: true,
      message: "Doi mat khau thanh cong",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// AUTH - YEU CAU RESET MAT KHAU
// ===============================
app.post("/api/auth/request-reset-password", async (req, res) => {
  try {
    const { login } = req.body;

    if (!login) {
      return res.status(400).json({
        success: false,
        message: "Missing login",
      });
    }

    const [users] = await db.query(
      `SELECT id, username, email, phone
       FROM users
       WHERE (username = ? OR email = ? OR phone = ?)
       AND status <> 'deleted'
       LIMIT 1`,
      [login, login, login]
    );

    if (users.length === 0) {
      return res.json({
        success: true,
        message: "Neu tai khoan ton tai, OTP reset mat khau se duoc tao",
      });
    }

    const user = users[0];
    const otp = createOtp();
    const otpHash = await bcrypt.hash(otp, 8);
    const expiredAt = new Date(Date.now() + 10 * 60 * 1000);

    await db.query(
      `INSERT INTO password_reset_otps(user_id, otp_hash, expired_at, used)
       VALUES (?, ?, ?, 0)`,
      [user.id, otpHash, expiredAt]
    );

    await createNotification(
      user.id,
      "Reset mật khẩu",
      "Có yêu cầu đặt lại mật khẩu cho tài khoản của bạn",
      "RESET_PASSWORD",
      null
    );

    const response = {
      success: true,
      message: "OTP reset mat khau da duoc tao",
    };

    if (process.env.NODE_ENV !== "production") {
      response.debug_otp = otp;
    }

    res.json(response);
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// AUTH - XAC NHAN OTP RESET MAT KHAU
// ===============================
app.post("/api/auth/reset-password", async (req, res) => {
  try {
    const { login, otp, new_password } = req.body;

    if (!login || !otp || !new_password) {
      return res.status(400).json({
        success: false,
        message: "Missing login, otp or new_password",
      });
    }

    if (!isValidPassword(new_password)) {
      return res.status(400).json({
        success: false,
        message: "Mat khau moi phai co it nhat 6 ky tu",
      });
    }

    const [users] = await db.query(
      `SELECT id
       FROM users
       WHERE (username = ? OR email = ? OR phone = ?)
       AND status <> 'deleted'
       LIMIT 1`,
      [login, login, login]
    );

    if (users.length === 0) {
      return res.status(400).json({
        success: false,
        message: "OTP khong hop le",
      });
    }

    const user = users[0];

    const [otps] = await db.query(
      `SELECT *
       FROM password_reset_otps
       WHERE user_id = ?
       AND used = 0
       AND expired_at > NOW()
       ORDER BY id DESC
       LIMIT 1`,
      [user.id]
    );

    if (otps.length === 0) {
      return res.status(400).json({
        success: false,
        message: "OTP sai hoac da het han",
      });
    }

    const validOtp = await bcrypt.compare(otp, otps[0].otp_hash);

    if (!validOtp) {
      return res.status(400).json({
        success: false,
        message: "OTP sai hoac da het han",
      });
    }

    const newHash = await bcrypt.hash(new_password, 10);

    await db.query(
      `UPDATE users
       SET password_hash = ?,
           failed_login_attempts = 0,
           locked_until = NULL,
           status = 'active'
       WHERE id = ?`,
      [newHash, user.id]
    );

    await db.query(
      "UPDATE password_reset_otps SET used = 1 WHERE id = ?",
      [otps[0].id]
    );

    await createNotification(
      user.id,
      "Mật khẩu đã thay đổi",
      "Mật khẩu tài khoản của bạn vừa được đặt lại thành công",
      "PASSWORD_CHANGED",
      null
    );

    res.json({
      success: true,
      message: "Reset mat khau thanh cong",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// USERS - ADMIN LAY DANH SACH USER
// ===============================
app.get("/api/users", authRequired, adminRequired, async (req, res) => {
  try {
    const { search, role, status } = req.query;

    let sql = `
      SELECT id, full_name, username, email, phone, role, status,
             failed_login_attempts, locked_until, last_login_at, created_at
      FROM users
      WHERE status <> 'deleted'
    `;

    const params = [];

    if (search) {
      sql += ` AND (full_name LIKE ? OR username LIKE ? OR email LIKE ? OR phone LIKE ?)`;
      const keyword = `%${search}%`;
      params.push(keyword, keyword, keyword, keyword);
    }

    if (role) {
      sql += ` AND role = ?`;
      params.push(role);
    }

    if (status) {
      sql += ` AND status = ?`;
      params.push(status);
    }

    sql += ` ORDER BY id DESC`;

    const [rows] = await db.query(sql, params);

    res.json({
      success: true,
      data: rows,
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// USERS - ADMIN TAO USER
// ===============================
app.post("/api/users", authRequired, adminRequired, async (req, res) => {
  try {
    const { full_name, username, email, phone, password, role } = req.body;

    if (!full_name || !username || !password) {
      return res.status(400).json({
        success: false,
        message: "Missing full_name, username or password",
      });
    }

    if (!isValidPassword(password)) {
      return res.status(400).json({
        success: false,
        message: "Mat khau phai co it nhat 6 ky tu",
      });
    }

    const passwordHash = await bcrypt.hash(password, 10);

    await db.query(
      `INSERT INTO users(full_name, username, email, phone, password_hash, role, status)
       VALUES (?, ?, ?, ?, ?, ?, 'active')`,
      [
        full_name,
        username,
        email || null,
        phone || null,
        passwordHash,
        role === "admin" ? "admin" : "user",
      ]
    );

    res.json({
      success: true,
      message: "Tao user thanh cong",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// USERS - LAY CHI TIET USER
// ===============================
app.get("/api/users/:id", authRequired, async (req, res) => {
  try {
    const id = Number(req.params.id);

    if (req.user.role !== "admin" && req.user.id !== id) {
      return res.status(403).json({
        success: false,
        message: "Khong co quyen xem user nay",
      });
    }

    const [rows] = await db.query(
      `SELECT id, full_name, username, email, phone, role, status,
              last_login_at, created_at
       FROM users
       WHERE id = ? AND status <> 'deleted'
       LIMIT 1`,
      [id]
    );

    if (rows.length === 0) {
      return res.status(404).json({
        success: false,
        message: "User not found",
      });
    }

    res.json({
      success: true,
      data: rows[0],
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// USERS - CAP NHAT USER
// ===============================
app.put("/api/users/:id", authRequired, async (req, res) => {
  try {
    const id = Number(req.params.id);

    if (req.user.role !== "admin" && req.user.id !== id) {
      return res.status(403).json({
        success: false,
        message: "Khong co quyen cap nhat user nay",
      });
    }

    const { full_name, email, phone } = req.body;

    await db.query(
      `UPDATE users
       SET full_name = COALESCE(?, full_name),
           email = COALESCE(?, email),
           phone = COALESCE(?, phone)
       WHERE id = ? AND status <> 'deleted'`,
      [full_name || null, email || null, phone || null, id]
    );

    res.json({
      success: true,
      message: "Cap nhat user thanh cong",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// USERS - ADMIN DOI ROLE
// ===============================
app.patch("/api/users/:id/role", authRequired, adminRequired, async (req, res) => {
  try {
    const { role } = req.body;

    if (!["admin", "user"].includes(role)) {
      return res.status(400).json({
        success: false,
        message: "Role khong hop le",
      });
    }

    await db.query(
      "UPDATE users SET role = ? WHERE id = ? AND status <> 'deleted'",
      [role, req.params.id]
    );

    res.json({
      success: true,
      message: "Cap nhat role thanh cong",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// USERS - ADMIN KHOA / MO KHOA USER
// ===============================
app.patch("/api/users/:id/status", authRequired, adminRequired, async (req, res) => {
  try {
    const { status } = req.body;

    if (!["active", "locked"].includes(status)) {
      return res.status(400).json({
        success: false,
        message: "Status khong hop le",
      });
    }

    await db.query(
      `UPDATE users
       SET status = ?,
           locked_until = NULL
       WHERE id = ? AND status <> 'deleted'`,
      [status, req.params.id]
    );

    res.json({
      success: true,
      message: "Cap nhat trang thai user thanh cong",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// USERS - ADMIN XOA MEM USER
// ===============================
app.delete("/api/users/:id", authRequired, adminRequired, async (req, res) => {
  try {
    if (Number(req.params.id) === req.user.id) {
      return res.status(400).json({
        success: false,
        message: "Admin khong the tu xoa chinh minh",
      });
    }

    await db.query(
      "UPDATE users SET status = 'deleted' WHERE id = ?",
      [req.params.id]
    );

    res.json({
      success: true,
      message: "Xoa user thanh cong",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// DEVICE TOKEN - APP DANG KY THIET BI
// ===============================
app.post("/api/device-tokens", authRequired, async (req, res) => {
  try {
    const { device_token, platform } = req.body;

    if (!device_token) {
      return res.status(400).json({
        success: false,
        message: "Missing device_token",
      });
    }

    await db.query(
      `INSERT INTO device_tokens(user_id, device_token, platform, status, last_seen_at)
       VALUES (?, ?, ?, 'active', NOW())
       ON DUPLICATE KEY UPDATE
       user_id = VALUES(user_id),
       platform = VALUES(platform),
       status = 'active',
       last_seen_at = NOW()`,
      [req.user.id, device_token, platform || "android"]
    );

    res.json({
      success: true,
      message: "Device token saved",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// DEVICE TOKEN - HUY DANG KY THIET BI
// ===============================
app.delete("/api/device-tokens", authRequired, async (req, res) => {
  try {
    const { device_token } = req.body;

    await db.query(
      `UPDATE device_tokens
       SET status = 'inactive'
       WHERE user_id = ? AND device_token = ?`,
      [req.user.id, device_token]
    );

    res.json({
      success: true,
      message: "Device token disabled",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// NOTIFICATIONS - APP LAY THONG BAO
// ===============================
app.get("/api/notifications", authRequired, async (req, res) => {
  try {
    const { unread_only } = req.query;

    let sql = `
      SELECT id, title, body, type, ref_id, is_read, created_at
      FROM notifications
      WHERE user_id = ?
      AND status = 'active'
    `;

    const params = [req.user.id];

    if (unread_only === "1") {
      sql += " AND is_read = 0";
    }

    sql += " ORDER BY id DESC LIMIT 100";

    const [rows] = await db.query(sql, params);

    res.json({
      success: true,
      data: rows,
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// NOTIFICATIONS - DANH DAU DA DOC
// ===============================
app.patch("/api/notifications/:id/read", authRequired, async (req, res) => {
  try {
    await db.query(
      `UPDATE notifications
       SET is_read = 1
       WHERE id = ? AND user_id = ?`,
      [req.params.id, req.user.id]
    );

    res.json({
      success: true,
      message: "Notification marked as read",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// NOTIFICATIONS - DOC TAT CA
// ===============================
app.patch("/api/notifications/read-all", authRequired, async (req, res) => {
  try {
    await db.query(
      `UPDATE notifications
       SET is_read = 1
       WHERE user_id = ? AND status = 'active'`,
      [req.user.id]
    );

    res.json({
      success: true,
      message: "All notifications marked as read",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// ADMIN GUI THONG BAO DEN USER
// ===============================
app.post("/api/admin/notifications", authRequired, adminRequired, async (req, res) => {
  try {
    const { title, body, type, target_user_ids, send_all } = req.body;

    if (!title) {
      return res.status(400).json({
        success: false,
        message: "Missing title",
      });
    }

    let users = [];

    if (send_all) {
      const [rows] = await db.query(
        `SELECT id FROM users
         WHERE status = 'active'`
      );
      users = rows;
    } else if (Array.isArray(target_user_ids) && target_user_ids.length > 0) {
      const placeholders = target_user_ids.map(() => "?").join(",");
      const [rows] = await db.query(
        `SELECT id FROM users
         WHERE id IN (${placeholders})
         AND status = 'active'`,
        target_user_ids
      );
      users = rows;
    } else {
      return res.status(400).json({
        success: false,
        message: "Missing target_user_ids or send_all",
      });
    }

    for (const user of users) {
      await createNotification(
        user.id,
        title,
        body || "",
        type || "ADMIN_MESSAGE",
        null
      );
    }

    res.json({
      success: true,
      message: `Da gui thong bao cho ${users.length} user`,
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// ADMIN DASHBOARD
// ===============================
app.get("/api/admin/dashboard", authRequired, adminRequired, async (req, res) => {
  try {
    const [[userCount]] = await db.query(
      `SELECT COUNT(*) AS total FROM users WHERE status <> 'deleted'`
    );

    const [[activeAlarmCount]] = await db.query(
      `SELECT COUNT(*) AS total FROM events WHERE status = 'active'`
    );

    const [[authMethodCount]] = await db.query(
      `SELECT COUNT(*) AS total FROM auth_methods WHERE status = 'active'`
    );

    const [safeRows] = await db.query(
      "SELECT * FROM safe_status WHERE id = 1 LIMIT 1"
    );

    const [latestEvents] = await db.query(
      `SELECT id, event_type, message, gps_lat, gps_lng, network_type, status, created_at
       FROM events
       ORDER BY id DESC
       LIMIT 10`
    );

    res.json({
      success: true,
      data: {
        total_users: userCount.total,
        active_events: activeAlarmCount.total,
        auth_methods: authMethodCount.total,
        safe_status: safeRows[0] || null,
        latest_events: latestEvents,
      },
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// ADMIN TAO LENH DIEU KHIEN KET
// ===============================
app.post("/api/admin/commands", authRequired, adminRequired, async (req, res) => {
  try {
    const { command, command_value } = req.body;

    const allowedCommands = [
      "OPEN_SAFE",
      "CLOSE_SAFE",
      "TURN_OFF_ALARM",
      "TURN_ON_ALARM",
      "ADD_RFID",
      "ADD_FINGER",
      "REMOVE_RFID",
      "REMOVE_FINGER",
    ];

    if (!allowedCommands.includes(command)) {
      return res.status(400).json({
        success: false,
        message: "Command khong hop le",
      });
    }

    await db.query(
      `INSERT INTO safe_commands(command, command_value, status, created_by)
       VALUES (?, ?, 'pending', ?)`,
      [
        command,
        typeof command_value === "object"
          ? JSON.stringify(command_value)
          : command_value || "",
        req.user.id,
      ]
    );

    await db.query(
      `INSERT INTO events(event_type, message, network_type, status)
       VALUES ('ADMIN_COMMAND', ?, 'APP', 'active')`,
      [`Admin ${req.user.username} tao lenh ${command}`]
    );

    res.json({
      success: true,
      message: "Da tao lenh dieu khien ket",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});

// ===============================
// ADMIN TAT CANH BAO
// ===============================
app.post("/api/admin/alarm/off", authRequired, adminRequired, async (req, res) => {
  try {
    await db.query(
      `INSERT INTO safe_commands(command, command_value, status, created_by)
       VALUES ('TURN_OFF_ALARM', 'OFF', 'pending', ?)`,
      [req.user.id]
    );

    await db.query(
      `UPDATE safe_status
       SET alarm_status = 'OFF', updated_at = CURRENT_TIMESTAMP
       WHERE id = 1`
    );

    await db.query(
      `INSERT INTO events(event_type, message, network_type, status)
       VALUES ('ALARM_OFF', ?, 'APP', 'active')`,
      [`Admin ${req.user.username} da tat canh bao`]
    );

    res.json({
      success: true,
      message: "Da tao lenh tat canh bao",
    });
  } catch (err) {
    res.status(500).json({ success: false, error: err.message });
  }
});
app.get("/db-test", async (req, res) => {
  try {
    const [rows] = await db.query("SELECT 1 AS ok");

    res.json({
      success: true,
      message: "MYSQL OK",
      data: rows,
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      message: "MYSQL ERROR",
      error: err.message,
    });
  }
});
app.post("/api/test-push", authRequired, async (req, res) => {
  try {
    const title = req.body.title || "Smart Safe";
    const body = req.body.body || "Test thông báo từ backend";

    await sendPushToUser(
      req.user.id,
      title,
      body,
      "TEST_PUSH",
      null
    );

    res.json({
      success: true,
      message: "Test push sent",
    });
  } catch (err) {
    console.error("[TEST PUSH ERROR]", err);
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
// ===============================
// START SERVER
// ===============================
app.listen(PORT, "0.0.0.0", async () => {
  try {
    await db.query("SELECT 1");
    console.log("MYSQL CONNECTED");
    console.log(`SMART SAFE BACKEND RUNNING ON PORT ${PORT}`);
  } catch (err) {
    console.log("MYSQL ERROR:", err.message);
  }
});
