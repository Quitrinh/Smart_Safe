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

const crypto = require("crypto");

const OTP_SECRET = process.env.OTP_SECRET;

if (!OTP_SECRET) {
  throw new Error("Missing OTP_SECRET");
}

function generateOtp() {
  return String(Math.floor(100000 + Math.random() * 900000));
}

function hashOtp(otp) {
  return crypto
    .createHash("sha256")
    .update(String(otp) + OTP_SECRET)
    .digest("hex");
}

async function sendOtpSms(phone, otp) {
  const message = `Ma OTP dat lai mat khau Smart Safe cua ban la: ${otp}. Ma co hieu luc trong 5 phut.`;

  // Cách 1: nếu backend có dịch vụ SMS thì gọi API SMS ở đây.

  // Cách 2: lưu vào sms_outbox để ESP32/SIM4G lấy và gửi
  await db.query(
    `INSERT INTO sms_outbox(phone, message, status)
     VALUES (?, ?, 'pending')`,
    [phone, message]
  );

  console.log("[OTP SMS QUEUED]", phone, otp);
}
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
function distanceMeters(lat1, lng1, lat2, lng2) {
  const R = 6371000;
  const toRad = (deg) => (deg * Math.PI) / 180;

  const dLat = toRad(lat2 - lat1);
  const dLng = toRad(lng2 - lng1);

  const a =
    Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.cos(toRad(lat1)) *
      Math.cos(toRad(lat2)) *
      Math.sin(dLng / 2) *
      Math.sin(dLng / 2);

  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));

  return R * c;
}

let lastSafeMovedAlertAt = 0;
const SAFE_MOVED_COOLDOWN_MS = 5 * 60 * 1000;
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

async function createNotificationForAdmins(title, body, type = "ALARM", refId = null) {
  try {
    await db.query(
      `INSERT INTO notifications(user_id, title, body, type, ref_id, is_read)
       SELECT id, ?, ?, ?, ?, 0
       FROM users
       WHERE status = 'active'`,
      [title, body, type, refId]
    );

    console.log("[NOTIFICATION] Created for all active users");
  } catch (err) {
    console.error("[CREATE NOTIFICATION ALL ERROR]", err.message);
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
    console.log("[FCM] sendPushToAdmins redirected to all active devices");

    const result = await sendPushToAllActiveDevices(
      title,
      body,
      {
        type,
        ref_id: refId || "",
      }
    );

    return result;
  } catch (err) {
    console.error("[FCM SEND ALL ERROR]", err.message);
    return {
      successCount: 0,
      failureCount: 1,
      error: err.message,
    };
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
    const {
      command_id,
      id,
      user_name,
      method_type,
      method_value,
    } = req.body;

    const commandId = command_id || id;

    if (!user_name || !method_type || !method_value) {
      return res.status(400).json({
        success: false,
        message: "Missing user_name, method_type or method_value",
      });
    }

    const finalType = String(method_type).toUpperCase();
    const finalValue = String(method_value).trim().toLowerCase();

    const [exists] = await db.query(
      `SELECT id, status
       FROM auth_methods
       WHERE method_type = ?
       AND method_value = ?`,
      [finalType, finalValue]
    );

    if (exists.length > 0) {
      await db.query(
        `UPDATE auth_methods
         SET user_name = ?,
             status = 'active',
             updated_at = CURRENT_TIMESTAMP
         WHERE id = ?`,
        [user_name, exists[0].id]
      );
    } else {
      await db.query(
        `INSERT INTO auth_methods(user_name, method_type, method_value, status)
         VALUES (?, ?, ?, 'active')`,
        [user_name, finalType, finalValue]
      );
    }

    if (commandId) {
      await db.query(
        `UPDATE safe_commands
         SET status = 'done',
             executed_at = CURRENT_TIMESTAMP
         WHERE id = ?`,
        [commandId]
      );
    }

    res.json({
      success: true,
      message: "Enroll result saved",
      data: {
        user_name,
        method_type: finalType,
        method_value: finalValue,
      },
    });
  } catch (err) {
    console.error("[ENROLL RESULT ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
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
// app.post("/api/auth/login", async (req, res) => {
//   try {
//     const { login, password } = req.body;

//     if (!login || !password) {
//       return res.status(400).json({
//         success: false,
//         message: "Missing login or password",
//       });
//     }

//     const [rows] = await db.query(
//       `SELECT *
//        FROM users
//        WHERE (username = ? OR email = ? OR phone = ?)
//        AND status <> 'deleted'
//        LIMIT 1`,
//       [login, login, login]
//     );

//     if (rows.length === 0) {
//       return res.status(401).json({
//         success: false,
//         message: "Tai khoan hoac mat khau khong dung",
//       });
//     }

//     const user = rows[0];

//     if (
//       user.status === "locked" &&
//       user.locked_until &&
//       new Date(user.locked_until) > new Date()
//     ) {
//       return res.status(423).json({
//         success: false,
//         message: "Tai khoan dang bi khoa tam thoi",
//       });
//     }

//     const ok = await bcrypt.compare(password, user.password_hash);

//     if (!ok) {
//       const failed = (user.failed_login_attempts || 0) + 1;

//       if (failed >= 5) {
//         const lockedUntil = new Date(Date.now() + 15 * 60 * 1000);

//         await db.query(
//           `UPDATE users
//            SET failed_login_attempts = ?, status = 'locked', locked_until = ?
//            WHERE id = ?`,
//           [failed, lockedUntil, user.id]
//         );

//         return res.status(423).json({
//           success: false,
//           message: "Sai mat khau qua 5 lan, tai khoan bi khoa 15 phut",
//         });
//       }

//       await db.query(
//         `UPDATE users SET failed_login_attempts = ? WHERE id = ?`,
//         [failed, user.id]
//       );

//       return res.status(401).json({
//         success: false,
//         message: "Tai khoan hoac mat khau khong dung",
//       });
//     }

//     await db.query(
//       `UPDATE users
//        SET failed_login_attempts = 0,
//            locked_until = NULL,
//            status = 'active',
//            last_login_at = NOW()
//        WHERE id = ?`,
//       [user.id]
//     );

//     await db.query(
//       `INSERT INTO events(event_type, message, network_type, status)
//        VALUES ('LOGIN_SUCCESS', ?, 'APP', 'active')`,
//       [`${user.username} da dang nhap thanh cong`]
//     );

//     const token = signToken(user);

//     res.json({
//       success: true,
//       message: "Dang nhap thanh cong",
//       token,
//       user: {
//         id: user.id,
//         full_name: user.full_name,
//         username: user.username,
//         email: user.email,
//         phone: user.phone,
//         role: user.role,
//       },
//     });
//   } catch (err) {
//     res.status(500).json({ success: false, error: err.message });
//   }
// });
function normalizePhone(phone) {
  let p = String(phone || "").trim();

  p = p.replace(/\s+/g, "");
  p = p.replace(/-/g, "");

  if (p.startsWith("+84")) {
    p = "0" + p.substring(3);
  }

  return p;
}

function normalizeUsername(username) {
  return String(username || "")
    .trim()
    .toLowerCase();
}

app.post("/api/auth/login", async (req, res) => {
  try {
    const { login, password } = req.body;

    if (!login || !password) {
      return res.status(400).json({
        success: false,
        message: "Missing login or password",
      });
    }

    const rawLogin = String(login).trim();
    const usernameLogin = normalizeUsername(rawLogin);
    const phoneLogin = normalizePhone(rawLogin);

    const [rows] = await db.query(
      `SELECT *
       FROM users
       WHERE (
            LOWER(username) = ?
         OR phone = ?
       )
       AND status <> 'deleted'
       LIMIT 1`,
      [usernameLogin, phoneLogin]
    );

    if (rows.length === 0) {
      return res.status(401).json({
        success: false,
        message: "Tai khoan hoac mat khau khong dung",
      });
    }

    const user = rows[0];

    if (user.status === "pending") {
      return res.status(403).json({
        success: false,
        code: "ACCOUNT_PENDING",
        message: "Tai khoan dang cho admin phe duyet",
      });
    }

    if (user.status === "rejected") {
      return res.status(403).json({
        success: false,
        code: "ACCOUNT_REJECTED",
        message: "Tai khoan da bi admin tu choi",
      });
    }

    if (user.status === "locked") {
      if (
        user.locked_until &&
        new Date(user.locked_until) <= new Date()
      ) {
        console.log("[LOGIN] Temporary lock expired");
      } else {
        return res.status(423).json({
          success: false,
          code: "ACCOUNT_LOCKED",
          message: "Tai khoan dang bi khoa",
        });
      }
    }

    const ok = await bcrypt.compare(
      String(password),
      user.password_hash
    );

    if (!ok) {
      const failed = (user.failed_login_attempts || 0) + 1;

      if (failed >= 5) {
        const lockedUntil = new Date(Date.now() + 15 * 60 * 1000);

        await db.query(
          `UPDATE users
           SET failed_login_attempts = ?,
               status = 'locked',
               locked_until = ?
           WHERE id = ?`,
          [failed, lockedUntil, user.id]
        );

        return res.status(423).json({
          success: false,
          code: "TEMP_LOCKED",
          message: "Sai mat khau qua 5 lan, tai khoan bi khoa 15 phut",
        });
      }

      await db.query(
        `UPDATE users
         SET failed_login_attempts = ?
         WHERE id = ?`,
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

    try {
      await db.query(
        `INSERT INTO events(event_type, message, network_type, status)
         VALUES ('LOGIN_SUCCESS', ?, 'APP', 'active')`,
        [
          `${
            user.username ||
            user.full_name ||
            user.phone ||
            "User"
          } da dang nhap thanh cong`,
        ]
      );
    } catch (eventErr) {
      console.log("[LOGIN EVENT SKIP]", eventErr.message);
    }

    const token = signToken({
      ...user,
      status: "active",
    });

    res.json({
      success: true,
      message: "Dang nhap thanh cong",
      token,
      user: {
        id: user.id,
        full_name: user.full_name,
        username: user.username,
        phone: user.phone,
        role: user.role,
        status: "active",
      },
    });
  } catch (err) {
    console.error("[LOGIN ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
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

// // ===============================
// // AUTH - DOI MAT KHAU KHI DA LOGIN
// // ===============================
// app.post("/api/auth/change-password", authRequired, async (req, res) => {
//   try {
//     const { old_password, new_password } = req.body;

//     if (!old_password || !new_password) {
//       return res.status(400).json({
//         success: false,
//         message: "Missing old_password or new_password",
//       });
//     }

//     if (!isValidPassword(new_password)) {
//       return res.status(400).json({
//         success: false,
//         message: "Mat khau moi phai co it nhat 6 ky tu",
//       });
//     }

//     const [rows] = await db.query(
//       "SELECT password_hash FROM users WHERE id = ? LIMIT 1",
//       [req.user.id]
//     );

//     const ok = await bcrypt.compare(old_password, rows[0].password_hash);

//     if (!ok) {
//       return res.status(400).json({
//         success: false,
//         message: "Mat khau cu khong dung",
//       });
//     }

//     const hash = await bcrypt.hash(new_password, 10);

//     await db.query(
//       "UPDATE users SET password_hash = ? WHERE id = ?",
//       [hash, req.user.id]
//     );

//     res.json({
//       success: true,
//       message: "Doi mat khau thanh cong",
//     });
//   } catch (err) {
//     res.status(500).json({ success: false, error: err.message });
//   }
// });
app.patch("/api/auth/change-password", authRequired, async (req, res) => {
  try {
    const { old_password, new_password } = req.body;

    if (!old_password || !new_password) {
      return res.status(400).json({
        success: false,
        message: "Missing old_password or new_password",
      });
    }

    if (String(new_password).length < 6) {
      return res.status(400).json({
        success: false,
        message: "Mat khau moi phai tu 6 ky tu",
      });
    }

    const [rows] = await db.query(
      `SELECT id, password_hash
       FROM users
       WHERE id = ?
       AND status <> 'deleted'
       LIMIT 1`,
      [req.user.id]
    );

    if (rows.length === 0) {
      return res.status(404).json({
        success: false,
        message: "Khong tim thay user",
      });
    }

    const user = rows[0];

    const ok = await bcrypt.compare(
      String(old_password),
      user.password_hash
    );

    if (!ok) {
      return res.status(401).json({
        success: false,
        message: "Mat khau cu khong dung",
      });
    }

    const newHash = await bcrypt.hash(String(new_password), 10);

    await db.query(
      `UPDATE users
       SET password_hash = ?,
           updated_at = CURRENT_TIMESTAMP
       WHERE id = ?`,
      [newHash, user.id]
    );

    res.json({
      success: true,
      message: "Doi mat khau thanh cong",
    });
  } catch (err) {
    console.error("[CHANGE PASSWORD ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
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

// // ===============================
// // DEVICE TOKEN - APP DANG KY THIET BI
// // ===============================
// app.post("/api/device-tokens", authRequired, async (req, res) => {
//   try {
//     const { device_token, platform } = req.body;

//     if (!device_token) {
//       return res.status(400).json({
//         success: false,
//         message: "Missing device_token",
//       });
//     }

//     await db.query(
//       `INSERT INTO device_tokens(user_id, device_token, platform, status, last_seen_at)
//        VALUES (?, ?, ?, 'active', NOW())
//        ON DUPLICATE KEY UPDATE
//        user_id = VALUES(user_id),
//        platform = VALUES(platform),
//        status = 'active',
//        last_seen_at = NOW()`,
//       [req.user.id, device_token, platform || "android"]
//     );

//     res.json({
//       success: true,
//       message: "Device token saved",
//     });
//   } catch (err) {
//     res.status(500).json({ success: false, error: err.message });
//   }
// });

// // ===============================
// // DEVICE TOKEN - HUY DANG KY THIET BI
// // ===============================
// app.delete("/api/device-tokens", authRequired, async (req, res) => {
//   try {
//     const { device_token } = req.body;

//     await db.query(
//       `UPDATE device_tokens
//        SET status = 'inactive'
//        WHERE user_id = ? AND device_token = ?`,
//       [req.user.id, device_token]
//     );

//     res.json({
//       success: true,
//       message: "Device token disabled",
//     });
//   } catch (err) {
//     res.status(500).json({ success: false, error: err.message });
//   }
// });
// app.patch("/api/device-tokens/disable-all", authRequired, async (req, res) => {
//   try {
//     await db.query(
//       `UPDATE device_tokens
//        SET status = 'inactive'
//        WHERE user_id = ?`,
//       [req.user.id]
//     );

//     res.json({
//       success: true,
//       message: "Da tat thong bao dien thoai",
//     });
//   } catch (err) {
//     res.status(500).json({
//       success: false,
//       error: err.message,
//     });
//   }
// });
app.get("/api/device-tokens/status", authRequired, async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT COUNT(*) AS active_count
       FROM device_tokens
       WHERE user_id = ?
       AND status = 'active'`,
      [req.user.id]
    );

    const enabled = Number(rows[0].active_count) > 0;

    res.json({
      success: true,
      enabled,
      active_count: Number(rows[0].active_count),
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.patch("/api/device-tokens/disable-all", authRequired, async (req, res) => {
  try {
    await db.query(
      `UPDATE device_tokens
       SET status = 'inactive'
       WHERE user_id = ?`,
      [req.user.id]
    );

    res.json({
      success: true,
      message: "Da tat thong bao dien thoai",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
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
       VALUES (?, ?, ?, 'active', CURRENT_TIMESTAMP)
       ON DUPLICATE KEY UPDATE
         user_id = VALUES(user_id),
         platform = VALUES(platform),
         status = 'active',
         last_seen_at = CURRENT_TIMESTAMP`,
      [
        req.user.id,
        device_token,
        platform || "android",
      ]
    );

    res.json({
      success: true,
      message: "Device token saved",
    });
  } catch (err) {
    console.error("[DEVICE TOKEN ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
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
// ADMIN DASHBOARD
// ===============================
app.get("/api/admin/dashboard", authRequired, async (req, res) => {
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

// // ===============================
// // ADMIN TAT CANH BAO
// // ===============================
// app.post("/api/admin/alarm/off", authRequired, adminRequired, async (req, res) => {
//   try {
//     await db.query(
//       `INSERT INTO safe_commands(command, command_value, status, created_by)
//        VALUES ('TURN_OFF_ALARM', 'OFF', 'pending', ?)`,
//       [req.user.id]
//     );

//     await db.query(
//       `UPDATE safe_status
//        SET alarm_status = 'OFF', updated_at = CURRENT_TIMESTAMP
//        WHERE id = 1`
//     );

//     await db.query(
//       `INSERT INTO events(event_type, message, network_type, status)
//        VALUES ('ALARM_OFF', ?, 'APP', 'active')`,
//       [`Admin ${req.user.username} da tat canh bao`]
//     );

//     res.json({
//       success: true,
//       message: "Da tao lenh tat canh bao",
//     });
//   } catch (err) {
//     res.status(500).json({ success: false, error: err.message });
//   }
// });
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
async function sendPushToAllActiveDevices(title, body, data = {}) {
  try {
    const [rows] = await db.query(
      `SELECT device_token
       FROM device_tokens
       WHERE status = 'active'`
    );

    const tokens = rows
      .map(r => r.device_token)
      .filter(Boolean);

    if (tokens.length === 0) {
      console.log("[FCM] No active device tokens");
      return {
        successCount: 0,
        failureCount: 0,
      };
    }

    const stringData = {};
    for (const key of Object.keys(data)) {
      stringData[key] =
        data[key] === null || data[key] === undefined
          ? ""
          : String(data[key]);
    }

    const result = await admin.messaging().sendEachForMulticast({
      tokens,
      notification: {
        title,
        body,
      },
      data: stringData,
    });

    console.log(
      `[FCM] Sent all: success=${result.successCount}, failed=${result.failureCount}`
    );

    return result;
  } catch (err) {
    console.error("[FCM SEND ALL ERROR]", err);
    return {
      successCount: 0,
      failureCount: 1,
      error: err.message,
    };
  }
}
app.post("/api/test-push", authRequired, async (req, res) => {
  try {
    const result = await sendPushToAllActiveDevices(
      "SMART SAFE TEST",
      "Day la thong bao test cho user",
      {
        type: "TEST_PUSH",
      }
    );

    res.json({
      success: true,
      message: "Test push sent",
      result,
    });
  } catch (err) {
    console.error("[TEST PUSH ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.post("/api/esp32/gps", async (req, res) => {
  try {
    const gpsLat = Number(req.body.gps_lat);
    const gpsLng = Number(req.body.gps_lng);

    if (!gpsLat || !gpsLng) {
      return res.status(400).json({
        success: false,
        message: "Missing gps_lat or gps_lng",
      });
    }

    await db.query(
      `UPDATE safe_status
       SET gps_lat = ?,
           gps_lng = ?,
           gps_updated_at = CURRENT_TIMESTAMP
       WHERE id = 1`,
      [gpsLat, gpsLng]
    );

    const [configs] = await db.query(
      `SELECT *
       FROM safe_location_config
       WHERE id = 1
       AND enabled = 1`
    );

    if (configs.length === 0) {
      return res.json({
        success: true,
        moved: false,
        message: "GPS updated, base location not set",
        data: {
          gps_lat: gpsLat,
          gps_lng: gpsLng,
        },
      });
    }

    const config = configs[0];

    if (!config.base_lat || !config.base_lng) {
      return res.json({
        success: true,
        moved: false,
        message: "GPS updated, base location empty",
        data: {
          gps_lat: gpsLat,
          gps_lng: gpsLng,
        },
      });
    }

    const distance = distanceMeters(
      Number(config.base_lat),
      Number(config.base_lng),
      gpsLat,
      gpsLng
    );

    const allowedRadius = Number(config.allowed_radius_m || 50);

    console.log("[GPS] Current:", gpsLat, gpsLng);
    console.log("[GPS] Base:", config.base_lat, config.base_lng);
    console.log("[GPS] Distance:", distance);

    if (distance > allowedRadius) {
      const now = Date.now();

      if (now - lastSafeMovedAlertAt > SAFE_MOVED_COOLDOWN_MS) {
        lastSafeMovedAlertAt = now;

        const message = `Phat hien ket bi di chuyen. Khoang cach: ${Math.round(
          distance
        )}m`;

        const [eventResult] = await db.query(
          `INSERT INTO events(event_type, message, network_type, status, gps_lat, gps_lng, distance_m)
           VALUES ('SAFE_MOVED', ?, 'GPS', 'active', ?, ?, ?)`,
          [message, gpsLat, gpsLng, distance]
        );

        try {
          if (typeof createNotificationForAdmins === "function") {
            await createNotificationForAdmins(
              "Cảnh báo két bị di chuyển",
              message,
              "SAFE_MOVED",
              eventResult.insertId
            );
          }
        } catch (notifyErr) {
          console.error("[GPS NOTIFICATION ERROR]", notifyErr.message);
        }

        try {
          if (typeof sendPushToAdmins === "function") {
            await sendPushToAdmins(
              "Cảnh báo két bị di chuyển",
              message,
              "SAFE_MOVED",
              eventResult.insertId
            );
          }
        } catch (pushErr) {
          console.error("[GPS PUSH ERROR]", pushErr.message);
        }

        return res.json({
          success: true,
          moved: true,
          distance_m: Math.round(distance),
          allowed_radius_m: allowedRadius,
          message: "Safe moved alert created",
        });
      }

      return res.json({
        success: true,
        moved: true,
        cooldown: true,
        distance_m: Math.round(distance),
        allowed_radius_m: allowedRadius,
        message: "Safe moved but alert cooldown active",
      });
    }

    res.json({
      success: true,
      moved: false,
      distance_m: Math.round(distance),
      allowed_radius_m: allowedRadius,
      message: "GPS updated, safe location normal",
    });
  } catch (err) {
    console.error("[ESP32 GPS ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.patch("/api/admin/location/config", authRequired, adminRequired, async (req, res) => {
  try {
    const { base_lat, base_lng, allowed_radius_m } = req.body;

    if (base_lat === undefined || base_lng === undefined) {
      return res.status(400).json({
        success: false,
        message: "Thieu base_lat hoac base_lng",
      });
    }

    const radius = allowed_radius_m || 50;

    const values = [
      ["safe_base_lat", String(base_lat)],
      ["safe_base_lng", String(base_lng)],
      ["gps_allowed_radius_m", String(radius)],
    ];

    for (const [key, value] of values) {
      await db.query(
        `INSERT INTO system_config(config_key, config_value)
         VALUES (?, ?)
         ON DUPLICATE KEY UPDATE config_value = VALUES(config_value)`,
        [key, value]
      );
    }

    res.json({
      success: true,
      message: "Da cap nhat vi tri chuan cua ket",
      data: {
        base_lat,
        base_lng,
        allowed_radius_m: radius,
      },
    });
  } catch (err) {
    console.error("[SET LOCATION CONFIG ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.post("/api/admin/location/set-current", authRequired, adminRequired, async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT gps_lat, gps_lng, gps_updated_at
       FROM safe_status
       WHERE id = 1`
    );

    if (rows.length === 0 || !rows[0].gps_lat || !rows[0].gps_lng) {
      return res.status(400).json({
        success: false,
        message: "Chua co du lieu GPS hien tai cua ket",
      });
    }

    const gpsLat = rows[0].gps_lat;
    const gpsLng = rows[0].gps_lng;

    await db.query(
      `INSERT INTO safe_location_config(id, base_lat, base_lng, allowed_radius_m, enabled, updated_by)
       VALUES (1, ?, ?, 50, 1, ?)
       ON DUPLICATE KEY UPDATE
       base_lat = VALUES(base_lat),
       base_lng = VALUES(base_lng),
       enabled = 1,
       updated_by = VALUES(updated_by),
       updated_at = CURRENT_TIMESTAMP`,
      [gpsLat, gpsLng, req.user.id]
    );

    await db.query(
      `INSERT INTO events(event_type, message, network_type, status, gps_lat, gps_lng)
       VALUES ('SET_BASE_LOCATION', ?, 'APP', 'active', ?, ?)`,
      [
        `Admin ${req.user.username} da dat vi tri hien tai lam vi tri chuan`,
        gpsLat,
        gpsLng,
      ]
    );

    res.json({
      success: true,
      message: "Da dat vi tri hien tai cua ket lam vi tri chuan",
      data: {
        base_lat: gpsLat,
        base_lng: gpsLng,
        allowed_radius_m: 50,
        gps_updated_at: rows[0].gps_updated_at,
      },
    });
  } catch (err) {
    console.error("[SET CURRENT LOCATION ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.get("/api/admin/location/config", authRequired, adminRequired, async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT config_key, config_value
       FROM system_config
       WHERE config_key IN (
         'safe_base_lat',
         'safe_base_lng',
         'gps_allowed_radius_m'
       )`
    );

    const config = {};
    for (const row of rows) {
      config[row.config_key] = row.config_value;
    }

    res.json({
      success: true,
      data: {
        base_lat: config.safe_base_lat || null,
        base_lng: config.safe_base_lng || null,
        allowed_radius_m: config.gps_allowed_radius_m || "50",
      },
    });
  } catch (err) {
    console.error("[GET LOCATION CONFIG ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.get("/api/esp32/ping", (req, res) => {
  res.json({
    success: true,
    message: "ESP32 connected to backend",
    time: new Date().toISOString(),
  });
});
app.post("/api/esp32/command-done", async (req, res) => {
  try {
    const { id, status } = req.body;

    if (!id) {
      return res.status(400).json({
        success: false,
        message: "Missing command id",
      });
    }

    await db.query(
      `UPDATE safe_commands
       SET status = ?,
           executed_at = CURRENT_TIMESTAMP
       WHERE id = ?`,
      [status || "done", id]
    );

    res.json({
      success: true,
      message: "Command marked as done",
    });
  } catch (err) {
    console.error("[COMMAND DONE ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.get("/api/admin/wifi-config", authRequired, adminRequired, async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT config_key, config_value
       FROM system_config
       WHERE config_key IN ('wifi_ssid', 'wifi_password')`
    );

    const config = {};
    rows.forEach((r) => {
      config[r.config_key] = r.config_value;
    });

    res.json({
      success: true,
      data: {
        wifi_ssid: config.wifi_ssid || "",
        has_password: !!config.wifi_password,
      },
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
// =====================================================
// SYSTEM CONFIG HELPERS
// =====================================================
async function getSystemConfig(keys = []) {
  if (!Array.isArray(keys) || keys.length === 0) {
    const [rows] = await db.query(
      `SELECT config_key, config_value, updated_at
       FROM system_config`
    );

    const config = {};

    rows.forEach((row) => {
      config[row.config_key] = row.config_value;
    });

    return config;
  }

  const placeholders = keys.map(() => "?").join(",");

  const [rows] = await db.query(
    `SELECT config_key, config_value, updated_at
     FROM system_config
     WHERE config_key IN (${placeholders})`,
    keys
  );

  const config = {};

  rows.forEach((row) => {
    config[row.config_key] = row.config_value;
  });

  return config;
}

async function setSystemConfig(key, value) {
  await db.query(
    `INSERT INTO system_config(config_key, config_value)
     VALUES (?, ?)
     ON DUPLICATE KEY UPDATE
     config_value = VALUES(config_value),
     updated_at = CURRENT_TIMESTAMP`,
    [key, value]
  );
}

// =====================================================
// OPTIONAL ESP32 CONFIG GUARD
// Nếu muốn bảo vệ API ESP32 config thì set ENV:
// ESP32_CONFIG_KEY=your_secret_key
// ESP32 gọi kèm header: x-esp32-key
// Nếu không set ENV thì API vẫn cho ESP32 gọi bình thường.
// =====================================================
function esp32ConfigGuard(req, res, next) {
  const requiredKey = process.env.ESP32_CONFIG_KEY;

  if (!requiredKey) {
    return next();
  }

  const inputKey = req.headers["x-esp32-key"] || req.query.key;

  if (inputKey !== requiredKey) {
    return res.status(401).json({
      success: false,
      message: "ESP32 config key invalid",
    });
  }

  next();
}

// =====================================================
// ADMIN - GET WIFI CONFIG
// App dùng API này để hiển thị WiFi hiện tại
// Không trả mật khẩu thật về app, chỉ trả has_password
// =====================================================
app.get("/api/admin/wifi-config", authRequired, adminRequired, async (req, res) => {
  try {
    const config = await getSystemConfig([
      "wifi_ssid",
      "wifi_password",
    ]);

    res.json({
      success: true,
      data: {
        wifi_ssid: config.wifi_ssid || "",
        has_password: !!config.wifi_password,
      },
    });
  } catch (err) {
    console.error("[GET WIFI CONFIG ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// =====================================================
// ADMIN - UPDATE WIFI CONFIG
// App Cài đặt gọi API này để đổi WiFi ESP32
// =====================================================
app.post("/api/admin/wifi-config", authRequired, adminRequired, async (req, res) => {
  try {
    const { wifi_ssid, wifi_password } = req.body;

    if (!wifi_ssid || String(wifi_ssid).trim().length === 0) {
      return res.status(400).json({
        success: false,
        message: "Ten WiFi SSID khong duoc rong",
      });
    }

    const finalSsid = String(wifi_ssid).trim();
    const finalPassword =
      wifi_password === undefined || wifi_password === null
        ? ""
        : String(wifi_password);

    await setSystemConfig("wifi_ssid", finalSsid);

    // Nếu app gửi password rỗng thì giữ password cũ
    if (finalPassword.length > 0) {
      await setSystemConfig("wifi_password", finalPassword);
    }

    await db.query(
      `INSERT INTO events(event_type, message, network_type, status)
       VALUES ('WIFI_CONFIG_UPDATED', ?, 'APP', 'active')`,
      [`Admin ${req.user.username} da cap nhat WiFi ESP32`]
    );

    res.json({
      success: true,
      message: "Da cap nhat cau hinh WiFi ESP32",
      data: {
        wifi_ssid: finalSsid,
      },
    });
  } catch (err) {
    console.error("[UPDATE WIFI CONFIG ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// =====================================================
// ESP32 - GET ALL CONFIG
// ESP32 gọi API này để lấy WiFi/keypad config
// =====================================================
app.get("/api/esp32/config", esp32ConfigGuard, async (req, res) => {
  try {
    const config = await getSystemConfig([
      "wifi_ssid",
      "wifi_password",
      "keypad_password",

      "max_wrong_password",
      "alert_vibration_enabled",
      "alert_door_enabled",
      "flame_alert_enabled",
      "gps_alert_enabled",
      "gps_allowed_radius_m",
    ]);

    res.json({
      success: true,
      data: {
        wifi_ssid: config.wifi_ssid || "",
        wifi_password: config.wifi_password || "",
        keypad_password: config.keypad_password || "1111",

        max_wrong_password: config.max_wrong_password || "5",
        alert_vibration_enabled: config.alert_vibration_enabled || "1",
        alert_door_enabled: config.alert_door_enabled || "1",
        flame_alert_enabled: config.flame_alert_enabled || "1",
        gps_alert_enabled: config.gps_alert_enabled || "1",
        gps_allowed_radius_m: config.gps_allowed_radius_m || "50",
      },
    });
  } catch (err) {
    console.error("[ESP32 CONFIG ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});

// =====================================================
// ESP32 - PING TEST
// Dùng để test ESP32 gọi backend được chưa
// =====================================================
app.get("/api/esp32/ping", async (req, res) => {
  res.json({
    success: true,
    message: "ESP32 backend OK",
    time: new Date().toISOString(),
  });
});
app.post("/api/auth/register", async (req, res) => {
  try {
    console.log("[REGISTER BODY]", req.body);

    const { full_name, username, phone, password } = req.body;

    const finalFullName = String(full_name || "").trim();
    const finalUsername = String(username || "").trim().toLowerCase();
    const finalPhone = normalizePhone(phone);

    console.log("[REGISTER DATA]", {
      finalFullName,
      finalUsername,
      finalPhone,
    });

    if (finalFullName.length < 2) {
      return res.status(400).json({
        success: false,
        message: "Ho ten khong hop le",
      });
    }

    if (!finalUsername) {
      return res.status(400).json({
        success: false,
        message: "Thieu ten dang nhap",
      });
    }

    if (!/^[a-zA-Z0-9_]{3,30}$/.test(finalUsername)) {
      return res.status(400).json({
        success: false,
        message: "Ten dang nhap chi gom chu, so, dau gach duoi va tu 3-30 ky tu",
      });
    }

    if (!/^0\d{9}$/.test(finalPhone)) {
      return res.status(400).json({
        success: false,
        message: "So dien thoai khong hop le",
      });
    }

    if (!password || String(password).length < 6) {
      return res.status(400).json({
        success: false,
        message: "Mat khau phai tu 6 ky tu",
      });
    }

    const [exists] = await db.query(
      `SELECT id, username, phone
       FROM users
       WHERE username = ?
          OR phone = ?
       LIMIT 1`,
      [finalUsername, finalPhone]
    );

    if (exists.length > 0) {
      if (exists[0].username === finalUsername) {
        return res.status(409).json({
          success: false,
          message: "Ten dang nhap da ton tai",
        });
      }

      if (exists[0].phone === finalPhone) {
        return res.status(409).json({
          success: false,
          message: "So dien thoai da duoc dang ky",
        });
      }
    }

    const passwordHash = await bcrypt.hash(String(password), 10);

    await db.query(
      `INSERT INTO users(username, full_name, phone, password_hash, role, status)
       VALUES (?, ?, ?, ?, 'user', 'pending')`,
      [
        finalUsername,
        finalFullName,
        finalPhone,
        passwordHash,
      ]
    );

    res.json({
      success: true,
      message: "Dang ky thanh cong, vui long cho admin phe duyet",
      status: "pending",
    });
  } catch (err) {
    console.error("[REGISTER ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
// app.post("/api/auth/register", async (req, res) => {
//   try {
//     const { full_name, phone, password } = req.body;

//     const finalPhone = normalizePhone(phone);

//     if (!full_name || String(full_name).trim().length < 2) {
//       return res.status(400).json({
//         success: false,
//         message: "Ho ten khong hop le",
//       });
//     }

//     if (!/^0\d{9}$/.test(finalPhone)) {
//       return res.status(400).json({
//         success: false,
//         message: "So dien thoai khong hop le",
//       });
//     }

//     if (!password || String(password).length < 6) {
//       return res.status(400).json({
//         success: false,
//         message: "Mat khau phai tu 6 ky tu",
//       });
//     }

//     const [exists] = await db.query(
//       `SELECT id FROM users WHERE phone = ? LIMIT 1`,
//       [finalPhone]
//     );

//     if (exists.length > 0) {
//       return res.status(409).json({
//         success: false,
//         message: "So dien thoai da duoc dang ky",
//       });
//     }

//     const passwordHash = await bcrypt.hash(String(password), 10);

//     await db.query(
//       `INSERT INTO users(full_name, phone, password_hash, role, status)
//        VALUES (?, ?, ?, 'user', 'active')`,
//       [
//         String(full_name).trim(),
//         finalPhone,
//         passwordHash,
//       ]
//     );

//     res.json({
//       success: true,
//       message: "Dang ky tai khoan thanh cong",
//     });
//   } catch (err) {
//     console.error("[REGISTER ERROR]", err);

//     res.status(500).json({
//       success: false,
//       error: err.message,
//     });
//   }
// });
app.get("/api/sms-recipients", authRequired, adminRequired, async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT id, user_id, name, phone, enabled, created_at
       FROM sms_recipients
       ORDER BY created_at DESC`
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
app.post("/api/sms-recipients", authRequired, adminRequired, async (req, res) => {
  try {
    const { name, phone } = req.body;

    if (!phone) {
      return res.status(400).json({
        success: false,
        message: "Thieu so dien thoai",
      });
    }

    await db.query(
      `INSERT INTO sms_recipients(name, phone, enabled, created_by)
       VALUES (?, ?, 1, ?)
       ON DUPLICATE KEY UPDATE
       name = VALUES(name),
       enabled = 1,
       updated_at = CURRENT_TIMESTAMP`,
      [
        name || "Owner",
        phone,
        req.user.id,
      ]
    );

    res.json({
      success: true,
      message: "Da them so dien thoai nhan SMS",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.delete("/api/sms-recipients/:id", authRequired, adminRequired, async (req, res) => {
  try {
    const id = req.params.id;

    await db.query(
      `DELETE FROM sms_recipients
       WHERE id = ?`,
      [id]
    );

    res.json({
      success: true,
      message: "Da xoa so nhan SMS",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.get("/api/esp32/sms-recipients", async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT phone
       FROM sms_recipients
       WHERE enabled = 1
       ORDER BY id ASC`
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
app.get("/api/admin/users/pending", authRequired, adminRequired, async (req, res) => {
  try {
const [rows] = await db.query(
    `SELECT id, username, full_name, phone, email, role, status, created_at
    FROM users
    WHERE status = 'pending'
    ORDER BY created_at DESC`
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
app.patch("/api/admin/users/:id/approve", authRequired, adminRequired, async (req, res) => {
  try {
    const userId = req.params.id;
    const { allow_sms } = req.body;

    const [rows] = await db.query(
      `SELECT id, full_name, phone, status
       FROM users
       WHERE id = ?
       LIMIT 1`,
      [userId]
    );

    if (rows.length === 0) {
      return res.status(404).json({
        success: false,
        message: "Khong tim thay user",
      });
    }

    const user = rows[0];

    if (user.status !== "pending") {
      return res.status(400).json({
        success: false,
        message: "Tai khoan nay khong o trang thai cho duyet",
      });
    }

    await db.query(
      `UPDATE users
       SET status = 'active',
           role = 'user',
           updated_at = CURRENT_TIMESTAMP
       WHERE id = ?`,
      [userId]
    );

    if (allow_sms === true || allow_sms === 1) {
      await db.query(
        `INSERT INTO sms_recipients(user_id, name, phone, enabled, created_by)
         VALUES (?, ?, ?, 1, ?)
         ON DUPLICATE KEY UPDATE
         user_id = VALUES(user_id),
         name = VALUES(name),
         enabled = 1,
         updated_at = CURRENT_TIMESTAMP`,
        [
          user.id,
          user.full_name || "User",
          user.phone,
          req.user.id,
        ]
      );
    }

    res.json({
      success: true,
      message:
        allow_sms === true || allow_sms === 1
          ? "Da phe duyet tai khoan va cho nhan SMS"
          : "Da phe duyet tai khoan",
    });
  } catch (err) {
    console.error("[APPROVE USER ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.patch("/api/admin/users/:id/reject", authRequired, adminRequired, async (req, res) => {
  try {
    const userId = req.params.id;

    const [result] = await db.query(
      `UPDATE users
       SET status = 'rejected',
           updated_at = CURRENT_TIMESTAMP
       WHERE id = ?
       AND status = 'pending'`,
      [userId]
    );

    if (result.affectedRows === 0) {
      return res.status(404).json({
        success: false,
        message: "Khong tim thay tai khoan cho duyet",
      });
    }

    res.json({
      success: true,
      message: "Da tu choi tai khoan",
    });
  } catch (err) {
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.delete("/api/admin/users/:id", authRequired, adminRequired, async (req, res) => {
  try {
    const userId = Number(req.params.id);

    if (!userId) {
      return res.status(400).json({
        success: false,
        message: "User id khong hop le",
      });
    }

    if (userId === req.user.id) {
      return res.status(400).json({
        success: false,
        message: "Admin khong the tu xoa tai khoan cua minh",
      });
    }

    const [rows] = await db.query(
      `SELECT id, username, phone, role, status
       FROM users
       WHERE id = ?
       LIMIT 1`,
      [userId]
    );

    if (rows.length === 0) {
      return res.status(404).json({
        success: false,
        message: "Khong tim thay user",
      });
    }

    const user = rows[0];

    if (user.role === "admin") {
      return res.status(403).json({
        success: false,
        message: "Khong duoc xoa tai khoan admin",
      });
    }

    if (user.status === "deleted") {
      return res.json({
        success: true,
        message: "Tai khoan nay da bi xoa truoc do",
      });
    }

    // Soft delete + đổi username/phone để số điện thoại có thể đăng ký lại sau này
    await db.query(
      `UPDATE users
       SET status = 'deleted',
           username = CONCAT('deleted_', id, '_', username),
           phone = CONCAT('del_', id),
           updated_at = CURRENT_TIMESTAMP
       WHERE id = ?`,
      [userId]
    );

    // Tắt token nhận push notification của user này
    try {
      await db.query(
        `UPDATE device_tokens
         SET status = 'inactive'
         WHERE user_id = ?`,
        [userId]
      );
    } catch (tokenErr) {
      console.log("[DELETE USER TOKEN SKIP]", tokenErr.message);
    }

    // Xoá khỏi danh sách nhận SMS nếu có
    try {
      await db.query(
        `DELETE FROM sms_recipients
         WHERE user_id = ?`,
        [userId]
      );
    } catch (smsErr) {
      console.log("[DELETE USER SMS SKIP]", smsErr.message);
    }

    // Ghi event
    try {
      await db.query(
        `INSERT INTO events(event_type, message, network_type, status)
         VALUES ('USER_DELETED', ?, 'APP', 'active')`,
        [`Admin da xoa user ${user.username || user.phone}`]
      );
    } catch (eventErr) {
      console.log("[DELETE USER EVENT SKIP]", eventErr.message);
    }

    res.json({
      success: true,
      message: "Da xoa tai khoan user",
    });
  } catch (err) {
    console.error("[DELETE USER ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.get("/api/admin/users", authRequired, adminRequired, async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT id, username, full_name, phone, role, status, created_at
       FROM users
       WHERE status <> 'deleted'
       ORDER BY id DESC`
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
app.post("/api/auth/forgot-password/request", async (req, res) => {
  try {
    const { login } = req.body;

    if (!login) {
      return res.status(400).json({
        success: false,
        message: "Missing login",
      });
    }

    const rawLogin = String(login).trim();
    const usernameLogin = normalizeUsername(rawLogin);
    const phoneLogin = normalizePhone(rawLogin);

    const [rows] = await db.query(
      `SELECT id, username, full_name, phone, status
       FROM users
       WHERE (
            LOWER(username) = ?
         OR phone = ?
       )
       AND status <> 'deleted'
       LIMIT 1`,
      [usernameLogin, phoneLogin]
    );

    if (rows.length === 0) {
      return res.json({
        success: true,
        message: "Neu tai khoan ton tai, ma OTP se duoc gui ve so dien thoai",
      });
    }

    const user = rows[0];

    if (user.status === "pending") {
      return res.status(403).json({
        success: false,
        message: "Tai khoan chua duoc admin phe duyet",
      });
    }

    if (user.status === "rejected") {
      return res.status(403).json({
        success: false,
        message: "Tai khoan da bi tu choi",
      });
    }

    const otp = generateOtp();
    const otpHash = hashOtp(otp);

    const expiresAt = new Date(Date.now() + 5 * 60 * 1000);

    await db.query(
      `UPDATE password_reset_otps
       SET used = 1
       WHERE user_id = ?
       AND used = 0`,
      [user.id]
    );

    await db.query(
      `INSERT INTO password_reset_otps(user_id, phone, otp_hash, expires_at)
       VALUES (?, ?, ?, ?)`,
      [user.id, user.phone, otpHash, expiresAt]
    );

    await sendOtpSms(user.phone, otp);

    res.json({
      success: true,
      message: "Ma OTP da duoc gui ve so dien thoai cua ban",
    });
  } catch (err) {
    console.error("[FORGOT PASSWORD REQUEST ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.post("/api/auth/forgot-password/reset", async (req, res) => {
  try {
    const { login, otp, new_password } = req.body;

    if (!login || !otp || !new_password) {
      return res.status(400).json({
        success: false,
        message: "Missing login, otp or new_password",
      });
    }

    if (String(new_password).length < 6) {
      return res.status(400).json({
        success: false,
        message: "Mat khau moi phai tu 6 ky tu",
      });
    }

    const rawLogin = String(login).trim();
    const usernameLogin = normalizeUsername(rawLogin);
    const phoneLogin = normalizePhone(rawLogin);

    const [users] = await db.query(
      `SELECT id, username, phone, status
       FROM users
       WHERE (
            LOWER(username) = ?
         OR phone = ?
       )
       AND status <> 'deleted'
       LIMIT 1`,
      [usernameLogin, phoneLogin]
    );

    if (users.length === 0) {
      return res.status(400).json({
        success: false,
        message: "Tai khoan khong hop le",
      });
    }

    const user = users[0];

    const otpHash = hashOtp(otp);

    const [otpRows] = await db.query(
      `SELECT *
       FROM password_reset_otps
       WHERE user_id = ?
       AND otp_hash = ?
       AND used = 0
       ORDER BY id DESC
       LIMIT 1`,
      [user.id, otpHash]
    );

    if (otpRows.length === 0) {
      return res.status(400).json({
        success: false,
        message: "Ma OTP khong dung",
      });
    }

    const otpData = otpRows[0];

    if (new Date(otpData.expires_at) < new Date()) {
      await db.query(
        `UPDATE password_reset_otps
         SET used = 1
         WHERE id = ?`,
        [otpData.id]
      );

      return res.status(400).json({
        success: false,
        message: "Ma OTP da het han",
      });
    }

    const passwordHash = await bcrypt.hash(String(new_password), 10);

    await db.query(
      `UPDATE users
       SET password_hash = ?,
           failed_login_attempts = 0,
           locked_until = NULL,
           status = CASE
             WHEN status = 'locked' THEN 'active'
             ELSE status
           END,
           updated_at = CURRENT_TIMESTAMP
       WHERE id = ?`,
      [passwordHash, user.id]
    );

    await db.query(
      `UPDATE password_reset_otps
       SET used = 1
       WHERE id = ?`,
      [otpData.id]
    );

    res.json({
      success: true,
      message: "Doi mat khau thanh cong",
    });
  } catch (err) {
    console.error("[RESET PASSWORD ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.get("/api/esp32/sms-outbox/next", async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT id, phone, message
       FROM sms_outbox
       WHERE status = 'pending'
       ORDER BY id ASC
       LIMIT 1`
    );

    if (rows.length === 0) {
      return res.json({
        success: true,
        data: null,
      });
    }

    res.json({
      success: true,
      data: rows[0],
    });
  } catch (err) {
    console.error("[SMS OUTBOX NEXT ERROR]", err);
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.patch("/api/esp32/sms-outbox/:id/sent", async (req, res) => {
  try {
    await db.query(
      `UPDATE sms_outbox
       SET status = 'sent',
           sent_at = NOW()
       WHERE id = ?`,
      [req.params.id]
    );

    res.json({
      success: true,
      message: "SMS marked as sent",
    });
  } catch (err) {
    console.error("[SMS SENT ERROR]", err);
    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.patch("/api/admin/config/bulk", authRequired, adminRequired, async (req, res) => {
  try {
    const { values } = req.body;

    if (!values || typeof values !== "object") {
      return res.status(400).json({
        success: false,
        message: "Missing values",
      });
    }

    const allowKeys = [
      "max_wrong_password",
      "gps_allowed_radius_m",
      "alert_vibration_enabled",
      "alert_door_enabled",
      "gps_alert_enabled",
      "flame_alert_enabled",
      "flame_threshold",
    ];

    for (const key of Object.keys(values)) {
      if (!allowKeys.includes(key)) {
        return res.status(400).json({
          success: false,
          message: `Config key khong hop le: ${key}`,
        });
      }
    }

    for (const [key, value] of Object.entries(values)) {
      await db.query(
        `INSERT INTO system_config(config_key, config_value)
         VALUES (?, ?)
         ON DUPLICATE KEY UPDATE
         config_value = VALUES(config_value),
         updated_at = CURRENT_TIMESTAMP`,
        [key, String(value)]
      );
    }

    res.json({
      success: true,
      message: "Da cap nhat cau hinh",
    });
  } catch (err) {
    console.error("[CONFIG BULK ERROR]", err);

    res.status(500).json({
      success: false,
      error: err.message,
    });
  }
});
app.get("/api/admin/config", authRequired, adminRequired, async (req, res) => {
  try {
    const [rows] = await db.query(
      `SELECT config_key, config_value
       FROM system_config
       ORDER BY config_key`
    );

    res.json({
      success: true,
      data: rows,
    });
  } catch (err) {
    console.error("[GET ADMIN CONFIG ERROR]", err);

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
