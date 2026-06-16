const express = require("express");
const cors = require("cors");
const bcrypt = require("bcryptjs");
const jwt = require("jsonwebtoken");
require("dotenv").config();

const db = require("./db");

const app = express();

app.use(cors());
app.use(express.json());

const PORT = process.env.PORT || 3000;

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
app.post("/api/request-open-otp", async (req, res) => {
  try {
    const { user_id } = req.body;

    const otp = Math.floor(100000 + Math.random() * 900000).toString();
    const expiredAt = new Date(Date.now() + 5 * 60 * 1000);

    await db.query(
      `INSERT INTO otp_codes(user_id, otp_code, purpose, expired_at, used)
       VALUES (?, ?, 'OPEN_SAFE', ?, 0)`,
      [user_id, otp, expiredAt]
    );

    res.json({
      success: true,
      message: "OTP da duoc tao",
      otp,
    });
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
app.post("/api/verify-otp-open", async (req, res) => {
  try {
    const { user_id, otp } = req.body;

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
      `INSERT INTO events(event_type, message, network_type)
       VALUES ('UNLOCK_REQUEST', 'App da xac thuc OTP va tao lenh mo ket', 'WIFI')`
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

// ===============================
// ESP32 GUI EVENT LEN SERVER
// ===============================
app.post("/api/events", async (req, res) => {
  try {
    const { event_type, message, gps_lat, gps_lng, network_type } = req.body;

    await db.query(
      `INSERT INTO events(event_type, message, gps_lat, gps_lng, network_type, status)
       VALUES (?, ?, ?, ?, ?, 'active')`,
      [
        event_type || "UNKNOWN",
        message || "",
        gps_lat ?? null,
        gps_lng ?? null,
        network_type || "WIFI",
      ]
    );

    res.json({
      success: true,
      message: "Event saved",
    });

  } catch (err) {
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
