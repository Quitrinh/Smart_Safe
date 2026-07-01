// =====================================================
// task_backend_command.cpp
// BACKEND COMMAND TASK - HTTP VERSION
// =====================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "core/system_bits.h"
#include "core/globals.h"
#include "core/events.h"
#include "config/pins.h"

// =====================================================
// BACKEND URL
// =====================================================
String commandBackendUrl =
    "http://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

// =====================================================
// EXTERN
// =====================================================
extern String lcdLine1;
extern String lcdLine2;
extern unsigned long lcdMessageTime;

extern String waitRFIDCardFromRC522(uint32_t timeoutMs);
extern int enrollFingerprintFromAS608(uint32_t timeoutMs);

extern double gpsLat;
extern double gpsLng;
extern bool gpsValid;
extern String correctPassword;
// =====================================================
// CONFIG GLOBAL
// =====================================================
bool alertVibrationEnabled = true;
bool alertDoorEnabled = true;
bool flameAlertEnabled = true;
bool gpsAlertEnabled = true;

int maxWrongPassword = 3;
int gpsAllowedRadiusM = 50;

// =====================================================
// CONFIG BOOL
// =====================================================
bool configBool(String v, bool defaultValue)
{
    v.trim();
    v.toLowerCase();

    if (v == "1" || v == "true" || v == "on") return true;
    if (v == "0" || v == "false" || v == "off") return false;

    return defaultValue;
}

// =====================================================
// JSON HELPER
// =====================================================
String getJsonValue(String json, String key)
{
    DynamicJsonDocument doc(512);

    DeserializationError error = deserializeJson(doc, json);

    if (error)
    {
        Serial.print("[JSON] Parse error: ");
        Serial.println(error.c_str());
        return "";
    }

    if (doc[key].isNull())
    {
        return "";
    }

    return doc[key].as<String>();
}

// =====================================================
// FETCH ESP32 CONFIG
// =====================================================
void fetchEsp32Config()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[CONFIG] WIFI NOT CONNECTED");
        return;
    }

    HTTPClient http;
    String url = commandBackendUrl + "/api/esp32/config";

    http.begin(url);
    http.setTimeout(8000);
    http.setReuse(false);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.GET();

    Serial.print("[CONFIG] HTTP CODE: ");
    Serial.println(httpCode);

    if (httpCode == 200)
    {
        String payload = http.getString();
        static String lastConfigPayload = "";

        if (payload != lastConfigPayload)
        {
            if (lastConfigPayload != "")
            {
                Serial.println("[CONFIG] CHANGED -> RESTART ESP32");

                lcdLine1 = "CONFIG UPDATED";
                lcdLine2 = "RESTARTING...";
                lcdMessageTime = millis();

                vTaskDelay(pdMS_TO_TICKS(1500));
                ESP.restart();
            }

            lastConfigPayload = payload;
        }

        Serial.print("[CONFIG] BODY: ");
        Serial.println(payload);

        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, payload);

        if (error)
        {
            Serial.print("[CONFIG] JSON ERROR: ");
            Serial.println(error.c_str());
            http.end();
            return;
        }

        JsonObject data = doc["data"];

        String keypadPass = String(data["keypad_password"] | "1234");
        String maxWrongStr = String(data["max_wrong_password"] | "3");

        String vibrationStr = String(data["alert_vibration_enabled"] | "1");
        String doorStr = String(data["alert_door_enabled"] | "1");
        String flameStr = String(data["flame_alert_enabled"] | "1");
        String gpsStr = String(data["gps_alert_enabled"] | "1");
        String gpsRadiusStr = String(data["gps_allowed_radius_m"] | "50");

        if (keypadPass.length() > 0)
        {
            correctPassword = keypadPass;
        }

        int newMax = maxWrongStr.toInt();

        if (newMax > 0)
        {
            maxWrongPassword = newMax;
        }

        alertVibrationEnabled = configBool(vibrationStr, true);
        alertDoorEnabled = configBool(doorStr, true);
        flameAlertEnabled = configBool(flameStr, true);
        gpsAlertEnabled = configBool(gpsStr, true);

        int newRadius = gpsRadiusStr.toInt();

        if (newRadius > 0)
        {
            gpsAllowedRadiusM = newRadius;
        }

        Serial.println("[CONFIG] UPDATED");

        Serial.print("correctPassword = ");
        Serial.println(correctPassword);

        Serial.print("maxWrongPassword = ");
        Serial.println(maxWrongPassword);
    }
    else
    {
        Serial.print("[CONFIG] GET FAILED: ");
        Serial.println(httpCode);
    }

    http.end();
}
// =====================================================
// MARK COMMAND STATUS
// =====================================================
void markCommandStatus(int commandId, String status)
{
    if (commandId <= 0) return;

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[BACKEND] WiFi not connected");
        return;
    }

    HTTPClient http;

    String url =
        commandBackendUrl +
        "/api/esp32/command-done";

    http.begin(url);
    http.setTimeout(8000);
    http.setReuse(false);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(256);

    doc["id"] = commandId;
    doc["command_id"] = commandId;
    doc["status"] = status;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);

    Serial.print("[BACKEND] COMMAND STATUS CODE = ");
    Serial.println(code);

    Serial.println(http.getString());

    http.end();
}

void markCommandDone(int commandId)
{
    markCommandStatus(commandId, "done");
}

void markCommandFailed(int commandId)
{
    markCommandStatus(commandId, "failed");
}

// =====================================================
// CHECK AUTH FROM BACKEND
// =====================================================
bool checkAuthFromBackend(String methodType, String methodValue)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[AUTH] WIFI NOT CONNECTED");
        return false;
    }

    HTTPClient http;

    String url =
        commandBackendUrl +
        "/api/auth-methods/check";

    http.begin(url);
    http.setTimeout(8000);
    http.setReuse(false);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(512);

    doc["method_type"] = methodType;
    doc["method_value"] = methodValue;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String response = http.getString();

    Serial.print("[AUTH] HTTP CODE = ");
    Serial.println(code);

    Serial.print("[AUTH] RESPONSE = ");
    Serial.println(response);

    http.end();

    if (code != 200) return false;

    DynamicJsonDocument resDoc(512);

    DeserializationError error =
        deserializeJson(resDoc, response);

    if (error) return false;

    bool valid =
        resDoc["valid"] | false;

    return valid;
}

// =====================================================
// OPEN SAFE AFTER AUTH
// =====================================================
void openSafeAfterAuth(String message)
{
    failedAttempts = 0;

    rfidAuthenticated = false;
    fingerAuthenticated = false;

    authenticated = true;

    xEventGroupClearBits(
        systemEvents,
        BIT_RFID_OK |
        BIT_FINGER_OK |
        BIT_KEYPAD_OK
    );

    xEventGroupSetBits(
        systemEvents,
        BIT_AUTH_OK
    );

    lcdLine1 = "AUTH OK";
    lcdLine2 = "SAFE OPEN";
    lcdMessageTime = millis();

    Serial.println("[SAFE] OPEN REQUEST");
    Serial.println(message);
}

// =====================================================
// SEND BACKEND EVENT
// =====================================================
void sendBackendEvent(String type, String msg)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[BACKEND] WIFI NOT CONNECTED");
        return;
    }

    HTTPClient http;

    String url =
        commandBackendUrl +
        "/api/events";

    http.begin(url);
    http.setTimeout(8000);
    http.setReuse(false);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(512);

    doc["event_type"] = type;
    doc["message"] = msg;
    doc["gps_lat"] = gpsValid ? gpsLat : 0;
    doc["gps_lng"] = gpsValid ? gpsLng : 0;
    doc["network_type"] = "WIFI";

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);

    Serial.print("[BACKEND] EVENT CODE = ");
    Serial.println(code);

    Serial.println(http.getString());

    http.end();
}

// =====================================================
// SEND GPS
// =====================================================
bool sendGpsLocationToBackend()
{
    if (!gpsValid) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;

    String url =
        commandBackendUrl +
        "/api/esp32/gps";

    http.begin(url);
    http.setTimeout(8000);
    http.setReuse(false);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(256);

    doc["gps_lat"] = gpsLat;
    doc["gps_lng"] = gpsLng;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);

    Serial.print("[GPS POST] CODE = ");
    Serial.println(code);

    Serial.println(http.getString());

    http.end();

    return code == 200 || code == 201;
}

// =====================================================
// SEND ENROLL RESULT
// =====================================================
bool sendEnrollResult(
    int commandId,
    String userName,
    String methodType,
    String methodValue
)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[ENROLL] WIFI NOT CONNECTED");
        return false;
    }

    HTTPClient http;

    String url =
        commandBackendUrl +
        "/api/auth-methods/enroll-result";

    http.begin(url);
    http.setTimeout(10000);
    http.setReuse(false);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(512);

    doc["id"] = commandId;
    doc["command_id"] = commandId;
    doc["user_name"] = userName;
    doc["method_type"] = methodType;
    doc["method_value"] = methodValue;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);

    Serial.print("[ENROLL] RESULT POST = ");
    Serial.println(code);

    Serial.println(http.getString());

    http.end();

    return code == 200 || code == 201;
}

// =====================================================
// ENROLL FINGERPRINT
// =====================================================
int enrollFingerprint()
{
    return enrollFingerprintFromAS608(20000);
}

// =====================================================
// HANDLE OPEN_SAFE
// =====================================================
void handleOpenSafe(int commandId)
{
    Serial.println("[BACKEND] OPEN_SAFE RECEIVED");

    openSafeAfterAuth("Safe opened by app OTP");

    markCommandDone(commandId);
}

// =====================================================
// HANDLE ADD RFID
// =====================================================
void handleAddRFID(int commandId, String commandValue)
{
    String userName =
        getJsonValue(commandValue, "user_name");

    if (userName == "")
    {
        lcdLine1 = "RFID FAILED";
        lcdLine2 = "NO USER";
        lcdMessageTime = millis();

        markCommandFailed(commandId);
        return;
    }

    lcdLine1 = "ADD RFID";
    lcdLine2 = userName;
    lcdMessageTime = millis();

    String uid =
        waitRFIDCardFromRC522(15000);

    if (uid.length() > 0)
    {
        bool ok =
            sendEnrollResult(
                commandId,
                userName,
                "RFID",
                uid
            );

        if (ok)
        {
            lcdLine1 = "RFID ADDED";
            lcdLine2 = uid;
            lcdMessageTime = millis();

            markCommandDone(commandId);

            sendBackendEvent(
                "ADD_RFID",
                "Added RFID for " + userName
            );
        }
        else
        {
            lcdLine1 = "RFID SAVE FAIL";
            lcdLine2 = "";
            lcdMessageTime = millis();

            markCommandFailed(commandId);
        }
    }
    else
    {
        lcdLine1 = "RFID FAILED";
        lcdLine2 = "TIMEOUT";
        lcdMessageTime = millis();

        markCommandFailed(commandId);
    }
}

// =====================================================
// HANDLE ADD FINGER
// =====================================================
void handleAddFinger(int commandId, String commandValue)
{
    String userName =
        getJsonValue(commandValue, "user_name");

    if (userName == "")
    {
        lcdLine1 = "FINGER FAILED";
        lcdLine2 = "NO USER";
        lcdMessageTime = millis();

        markCommandFailed(commandId);
        return;
    }

    lcdLine1 = "ADD FINGER";
    lcdLine2 = userName;
    lcdMessageTime = millis();

    int fingerId =
        enrollFingerprint();

    if (fingerId > 0)
    {
        bool ok =
            sendEnrollResult(
                commandId,
                userName,
                "FINGERPRINT",
                String(fingerId)
            );

        if (ok)
        {
            lcdLine1 = "FINGER ADDED";
            lcdLine2 = "ID: " + String(fingerId);
            lcdMessageTime = millis();

            markCommandDone(commandId);

            sendBackendEvent(
                "ADD_FINGER",
                "Added fingerprint for " + userName
            );
        }
        else
        {
            lcdLine1 = "FINGER SAVE FAIL";
            lcdLine2 = "";
            lcdMessageTime = millis();

            markCommandFailed(commandId);
        }
    }
    else
    {
        lcdLine1 = "FINGER FAILED";
        lcdLine2 = "";
        lcdMessageTime = millis();

        markCommandFailed(commandId);
    }
}

// =====================================================
// HANDLE COMMAND
// =====================================================
void handleBackendCommand(
    int commandId,
    String command,
    String commandValue
)
{
    command.trim();

    Serial.print("[COMMAND HANDLE] ID = ");
    Serial.println(commandId);

    Serial.print("[COMMAND HANDLE] COMMAND = ");
    Serial.println(command);

    if (command == "OPEN_SAFE")
    {
        handleOpenSafe(commandId);
    }
    else if (command == "ADD_RFID")
    {
        handleAddRFID(commandId, commandValue);
    }
    else if (
        command == "ADD_FINGER" ||
        command == "ADD_FINGERPRINT"
    )
    {
        handleAddFinger(commandId, commandValue);
    }
    else
    {
        Serial.println("[COMMAND] UNKNOWN");

        lcdLine1 = "CMD UNKNOWN";
        lcdLine2 = command;
        lcdMessageTime = millis();

        markCommandFailed(commandId);
    }
}

// =====================================================
// TASK BACKEND COMMAND
// =====================================================
void taskBackendCommand(void *pv)
{
    Serial.println("[BACKEND] TASK STARTED");

    unsigned long lastGpsPost = 0;
    unsigned long lastConfigFetch = 0;

    while (1)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            // Lấy config định kỳ, độc lập với command
            if (millis() - lastConfigFetch > 30000)
            {
                lastConfigFetch = millis();
                fetchEsp32Config();
            }

            HTTPClient http;

            String url =
                commandBackendUrl +
                "/api/esp32/commands";

            http.begin(url);
            http.setTimeout(8000);
            http.setReuse(false);

            int code = http.GET();

            if (code == 200)
            {
                String payload =
                    http.getString();

                DynamicJsonDocument doc(2048);

                DeserializationError error =
                    deserializeJson(doc, payload);

                if (error)
                {
                    Serial.print("[BACKEND] JSON ERROR = ");
                    Serial.println(error.c_str());
                }
                else if (!doc["command"].isNull())
                {
                    int commandId =
                        doc["command"]["id"] | 0;

                    String command =
                        doc["command"]["command"] | "";

                    String commandValue =
                        doc["command"]["command_value"].isNull()
                        ? ""
                        : doc["command"]["command_value"].as<String>();

                    if (
                        commandId > 0 &&
                        command.length() > 0
                    )
                    {
                        handleBackendCommand(
                            commandId,
                            command,
                            commandValue
                        );
                    }
                }
            }
            else
            {
                Serial.print("[BACKEND] GET COMMAND CODE = ");
                Serial.println(code);
            }

            http.end();

            EventBits_t bits =
                xEventGroupGetBits(systemEvents);

            bool gpsReady =
                bits & BIT_GPS_READY;

            bool needPostGps =
                gpsValid &&
                (
                    gpsReady ||
                    millis() - lastGpsPost > 60000
                );

            if (needPostGps)
            {
                lastGpsPost = millis();

                bool ok =
                    sendGpsLocationToBackend();

                if (ok)
                {
                    xEventGroupClearBits(
                        systemEvents,
                        BIT_GPS_READY
                    );
                }
            }
        }
        else
        {
            Serial.println("[BACKEND] WIFI NOT CONNECTED");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}