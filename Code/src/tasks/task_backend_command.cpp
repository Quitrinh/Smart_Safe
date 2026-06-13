#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "core/system_bits.h"
#include "core/globals.h"
#include "core/events.h"

String commandBackendUrl = "https://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

extern String lcdLine1;
extern String lcdLine2;
extern unsigned long lcdMessageTime;

extern String waitRFIDCardFromRC522(uint32_t timeoutMs);
extern int enrollFingerprintFromAS608(uint32_t timeoutMs);

// =====================================================
// JSON HELPER
// =====================================================
String getJsonValue(String json, String key)
{
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, json);

    if (error || doc[key].isNull())
    {
        return "";
    }

    return doc[key].as<String>();
}

// =====================================================
// CHECK AUTH FROM BACKEND
// Dùng khi quẹt RFID hoặc xác thực vân tay để mở két
// =====================================================
bool checkAuthFromBackend(String methodType, String methodValue)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[AUTH] WIFI NOT CONNECTED");
        return false;
    }

    HTTPClient http;

    http.begin(commandBackendUrl + "/api/auth-methods/check");
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(512);
    doc["method_type"] = methodType;
    doc["method_value"] = methodValue;

    String body;
    serializeJson(doc, body);

    Serial.println("[AUTH] CHECK BODY:");
    Serial.println(body);

    int code = http.POST(body);
    String response = http.getString();

    Serial.print("[AUTH] HTTP CODE = ");
    Serial.println(code);

    Serial.print("[AUTH] RESPONSE = ");
    Serial.println(response);

    http.end();

    if (code != 200)
    {
        return false;
    }

    DynamicJsonDocument resDoc(512);
    DeserializationError error = deserializeJson(resDoc, response);

    if (error)
    {
        Serial.println("[AUTH] JSON PARSE ERROR");
        return false;
    }

    bool valid = resDoc["valid"] | false;

    if (valid)
    {
        Serial.println("[AUTH] VALID");
        return true;
    }
    else
    {
        Serial.println("[AUTH] INVALID");
        return false;
    }
}

// =====================================================
// OPEN SAFE AFTER AUTH OK
// Chỉ gọi hàm này khi đã check backend hợp lệ
// =====================================================
void openSafeAfterAuth(String message)
{
    authenticated = true;
    xEventGroupSetBits(systemEvents, BIT_AUTH_OK);

    lcdLine1 = "AUTH OK";
    lcdLine2 = "SAFE OPEN";
    lcdMessageTime = millis();

    Serial.println("[SAFE] OPEN");
    Serial.println(message);
}

// =====================================================
// MARK COMMAND DONE
// =====================================================
void markCommandDone(int commandId)
{
    HTTPClient http;

    http.begin(commandBackendUrl + "/api/esp32/command-done");
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(256);
    doc["command_id"] = commandId;
    doc["status"] = "done";

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);

    Serial.print("[BACKEND] COMMAND DONE = ");
    Serial.println(code);
    Serial.println(http.getString());

    http.end();
}

// =====================================================
// MARK COMMAND FAILED
// =====================================================
void markCommandFailed(int commandId)
{
    HTTPClient http;

    http.begin(commandBackendUrl + "/api/esp32/command-done");
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(256);
    doc["command_id"] = commandId;
    doc["status"] = "failed";

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);

    Serial.print("[BACKEND] COMMAND FAILED = ");
    Serial.println(code);
    Serial.println(http.getString());

    http.end();
}

// =====================================================
// SEND EVENT
// =====================================================
void sendBackendEvent(String type, String msg)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    HTTPClient http;

    http.begin(commandBackendUrl + "/api/events");
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

    Serial.print("[BACKEND] EVENT = ");
    Serial.println(code);

    http.end();
}

// =====================================================
// SEND ENROLL RESULT
// =====================================================
bool sendEnrollResult(
    int commandId,
    String userName,
    String methodType,
    String methodValue)
{
    HTTPClient http;

    http.begin(commandBackendUrl + "/api/auth-methods/enroll-result");
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(512);

    doc["command_id"] = commandId;
    doc["user_name"] = userName;
    doc["method_type"] = methodType;
    doc["method_value"] = methodValue;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String response = http.getString();

    Serial.print("[ENROLL] RESULT POST = ");
    Serial.println(code);
    Serial.println(response);

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
// HANDLE OPEN SAFE FROM APP
// =====================================================
void handleOpenSafe(int commandId)
{
    Serial.println("[BACKEND] OPEN_SAFE RECEIVED");

    openSafeAfterAuth("Safe opened by app OTP");

    sendBackendEvent(
        "UNLOCK",
        "Safe opened by app OTP");

    markCommandDone(commandId);
}

// =====================================================
// HANDLE ADD RFID
// =====================================================
void handleAddRFID(int commandId, String commandValue)
{
    String userName = getJsonValue(commandValue, "user_name");

    Serial.println("[BACKEND] ADD_RFID RECEIVED");
    Serial.print("[BACKEND] USER = ");
    Serial.println(userName);

    if (userName == "")
    {
        Serial.println("[BACKEND] USER EMPTY");
        markCommandFailed(commandId);
        return;
    }

    lcdLine1 = "ADD RFID";
    lcdLine2 = userName;
    lcdMessageTime = millis();

    String uid = waitRFIDCardFromRC522(15000);

    if (uid.length() > 0)
    {
        bool ok = sendEnrollResult(
            commandId,
            userName,
            "RFID",
            uid);

        if (ok)
        {
            lcdLine1 = "RFID ADDED";
            lcdLine2 = uid;
            lcdMessageTime = millis();

            sendBackendEvent(
                "ADD_RFID",
                "Added RFID for " + userName);

            markCommandDone(commandId);
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

        Serial.println("[BACKEND] ADD_RFID FAILED");

        markCommandFailed(commandId);
    }
}

// =====================================================
// HANDLE ADD FINGER
// =====================================================
void handleAddFinger(int commandId, String commandValue)
{
    String userName = getJsonValue(commandValue, "user_name");

    Serial.println("[BACKEND] ADD_FINGER RECEIVED");
    Serial.print("[BACKEND] USER = ");
    Serial.println(userName);

    if (userName == "")
    {
        Serial.println("[BACKEND] USER EMPTY");
        markCommandFailed(commandId);
        return;
    }

    lcdLine1 = "ADD FINGER";
    lcdLine2 = userName;
    lcdMessageTime = millis();

    int fingerId = enrollFingerprint();

    if (fingerId > 0)
    {
        bool ok = sendEnrollResult(
            commandId,
            userName,
            "FINGERPRINT",
            String(fingerId));

        if (ok)
        {
            lcdLine1 = "FINGER ADDED";
            lcdLine2 = "ID: " + String(fingerId);
            lcdMessageTime = millis();

            sendBackendEvent(
                "ADD_FINGER",
                "Added fingerprint for " + userName);

            markCommandDone(commandId);
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

        Serial.println("[BACKEND] ADD_FINGER FAILED");

        markCommandFailed(commandId);
    }
}

// =====================================================
// TASK BACKEND COMMAND
// =====================================================
void taskBackendCommand(void *pv)
{
    while (1)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            HTTPClient http;

            http.begin(commandBackendUrl + "/api/esp32/commands");

            int code = http.GET();

            if (code == 200)
            {
                String payload = http.getString();

                DynamicJsonDocument doc(2048);

                DeserializationError error =
                    deserializeJson(doc, payload);

                if (!error && !doc["command"].isNull())
                {
                    int commandId =
                        doc["command"]["id"] | 0;

                    String command =
                        doc["command"]["command"].as<String>();

                    String commandValue =
                        doc["command"]["command_value"].isNull()
                            ? ""
                            : doc["command"]["command_value"].as<String>();

                    Serial.print("[BACKEND] COMMAND ID = ");
                    Serial.println(commandId);

                    Serial.print("[BACKEND] COMMAND = ");
                    Serial.println(command);

                    if (command == "OPEN_SAFE")
                    {
                        handleOpenSafe(commandId);
                    }
                    else if (command == "ADD_RFID")
                    {
                        handleAddRFID(commandId, commandValue);
                    }
                    else if (command == "ADD_FINGER")
                    {
                        handleAddFinger(commandId, commandValue);
                    }
                }
            }

            http.end();
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}