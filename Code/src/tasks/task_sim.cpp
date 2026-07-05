// =====================================================
// task_sim.cpp
// SMART SAFE - SMS MULTI PHONE + SMS OUTBOX OTP VERSION
// =====================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "core/globals.h"
#include "core/events.h"
#include <Preferences.h>

Preferences smsPrefs;
// =====================================================
// UART SIM
// =====================================================
HardwareSerial simSerial(1);

#define SIM_RX 25
#define SIM_TX 26

// =====================================================
// BACKEND
// =====================================================
String eventBackendUrl =
    "http://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

// =====================================================
// SMS PHONE LIST FROM BACKEND
// =====================================================
#define MAX_SMS_PHONES 10

String smsPhones[MAX_SMS_PHONES];
int smsPhoneCount = 0;
bool simWaitFor(String expected, int timeout);
// =====================================================
// TIME CONFIG
// =====================================================
#define SMS_RECIPIENT_FETCH_INTERVAL 300000
#define SMS_OUTBOX_CHECK_INTERVAL    1000
#define SMS_SEND_DELAY               3000
#define SMS_DUPLICATE_DELAY          3000

// =====================================================
// LAST EVENT
// =====================================================
EventType lastEventType = EVENT_ALARM_OFF;
unsigned long lastEventTime = 0;

// =====================================================
// NORMALIZE PHONE
// =====================================================
String normalizePhoneForSMS(String phone)
{
    phone.trim();
    phone.replace(" ", "");
    phone.replace("-", "");

    if (phone.startsWith("+84"))
    {
        return phone;
    }

    if (phone.startsWith("84"))
    {
        return "+" + phone;
    }

    if (phone.startsWith("0"))
    {
        return "+84" + phone.substring(1);
    }

    return phone;
}
void saveSmsRecipientsLocal()
{
    smsPrefs.begin("sms_cfg", false);

    smsPrefs.putInt("count", smsPhoneCount);

    for (int i = 0; i < smsPhoneCount; i++)
    {
        smsPrefs.putString(("phone" + String(i)).c_str(), smsPhones[i]);
    }

    smsPrefs.end();

    Serial.println("[SMS RECIPIENTS] SAVED LOCAL");
}

void loadSmsRecipientsLocal()
{
    smsPrefs.begin("sms_cfg", true);

    smsPhoneCount = smsPrefs.getInt("count", 0);

    if (smsPhoneCount > MAX_SMS_PHONES)
    {
        smsPhoneCount = MAX_SMS_PHONES;
    }

    for (int i = 0; i < smsPhoneCount; i++)
    {
        smsPhones[i] = smsPrefs.getString(("phone" + String(i)).c_str(), "");
    }

    smsPrefs.end();

    Serial.print("[SMS RECIPIENTS] LOAD LOCAL COUNT = ");
    Serial.println(smsPhoneCount);
}
// =====================================================
// INIT SIM
// =====================================================
void simInit()
{
    simSerial.begin(
        115200,
        SERIAL_8N1,
        SIM_RX,
        SIM_TX
    );

    delay(3000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("[SIM] INIT START");

    simSerial.println("AT");
    simWaitFor("OK", 3000);

    // Tắt echo để phản hồi gọn hơn
    simSerial.println("ATE0");
    simWaitFor("OK", 3000);

    // SMS text mode
    simSerial.println("AT+CMGF=1");
    simWaitFor("OK", 3000);

    // Dùng GSM charset, nội dung nên không dấu
    simSerial.println("AT+CSCS=\"GSM\"");
    simWaitFor("OK", 3000);

    Serial.println("[SIM] INIT DONE");
}

// =====================================================
// WAIT RESPONSE
// =====================================================
bool simWaitFor(String expected, int timeout)
{
    String response = "";
    unsigned long start = millis();

    while (millis() - start < timeout)
    {
        while (simSerial.available())
        {
            char c = simSerial.read();
            Serial.write(c);
            response += c;

            if (response.indexOf(expected) != -1)
            {
                return true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return false;
}

// =====================================================
// SEND ONE SMS
// =====================================================
bool sendSMS(String phone, String message)
{
    phone = normalizePhoneForSMS(phone);

    Serial.println();
    Serial.println("=================================");
    Serial.print("[SMS] SEND TO: ");
    Serial.println(phone);

    if (phone.length() <= 5)
    {
        Serial.println("[SMS] INVALID PHONE");
        return false;
    }

    while (simSerial.available())
    {
        simSerial.read();
    }

    // Kiểm tra SIM còn phản hồi không
    simSerial.println("AT");
    if (!simWaitFor("OK", 2000))
    {
        Serial.println("[SMS] AT FAIL");
        return false;
    }

    simSerial.print("AT+CMGS=\"");
    simSerial.print(phone);
    simSerial.println("\"");

    if (!simWaitFor(">", 5000))
    {
        Serial.println("[SMS] NO > PROMPT");
        return false;
    }

    simSerial.print(message);
    delay(200);
    simSerial.write(26);

    // Chờ gửi thành công
    if (!simWaitFor("OK", 15000))
    {
        Serial.println("[SMS] SEND FAIL");
        return false;
    }

    Serial.println("[SMS] SEND OK");
    return true;
}
// =====================================================
// FETCH SMS RECIPIENTS FROM BACKEND
// =====================================================
bool fetchSmsRecipients()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[SMS RECIPIENTS] WIFI NOT CONNECTED");
        return false;
    }

    HTTPClient http;

    String url =
        eventBackendUrl +
        "/api/esp32/sms-recipients";

    http.begin(url);

    int code = http.GET();

    Serial.print("[SMS RECIPIENTS] HTTP CODE = ");
    Serial.println(code);

    if (code != 200)
    {
        http.end();
        return false;
    }

    String payload = http.getString();

    Serial.print("[SMS RECIPIENTS] BODY = ");
    Serial.println(payload);

    JsonDocument doc;

    DeserializationError error =
        deserializeJson(doc, payload);

    if (error)
    {
        Serial.print("[SMS RECIPIENTS] JSON ERROR = ");
        Serial.println(error.c_str());

        http.end();
        return false;
    }

    if (!doc["success"].as<bool>())
    {
        http.end();
        return false;
    }

    JsonArray arr =
        doc["data"].as<JsonArray>();

    smsPhoneCount = 0;

    for (JsonObject item : arr)
    {
        if (smsPhoneCount >= MAX_SMS_PHONES)
        {
            break;
        }

        String phone =
            item["phone"] | "";

        phone =
            normalizePhoneForSMS(phone);

        if (phone.length() > 5)
        {
            smsPhones[smsPhoneCount] = phone;
            smsPhoneCount++;
        }
    }

    Serial.print("[SMS RECIPIENTS] COUNT = ");
    Serial.println(smsPhoneCount);
    saveSmsRecipientsLocal();
    http.end();
    return true;
}

// =====================================================
// SEND SMS TO ALL RECIPIENTS
// =====================================================
void sendSMSAll(String message)
{
    if (smsPhoneCount <= 0)
    {
        Serial.println("[SMS] NO RECIPIENTS");
        return;
    }

    if (xSemaphoreTake(simMutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        Serial.println("[SMS] MUTEX FAIL");
        return;
    }

    for (int i = 0; i < smsPhoneCount; i++)
    {
        if (smsPhones[i].length() > 5)
        {
            sendSMS(
                smsPhones[i],
                message
            );

            vTaskDelay(
                pdMS_TO_TICKS(SMS_SEND_DELAY)
            );
        }
    }

    xSemaphoreGive(simMutex);
}

// =====================================================
// MARK SMS OUTBOX STATUS
// status = sent hoặc failed
// =====================================================
void markSmsOutboxStatus(int id, String status)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[SMS OUTBOX] WIFI NOT CONNECTED");
        return;
    }

    HTTPClient http;

    String url =
        eventBackendUrl +
        "/api/esp32/sms-outbox/" +
        String(id) +
        "/" +
        status;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int code =
        http.PATCH("{}");

    Serial.print("[SMS OUTBOX] MARK ");
    Serial.print(status);
    Serial.print(" CODE = ");
    Serial.println(code);

    http.end();
}

// =====================================================
// PROCESS SMS OUTBOX OTP / PENDING SMS
// =====================================================
void processSmsOutbox()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    HTTPClient http;

    String url =
        eventBackendUrl +
        "/api/esp32/sms-outbox/next";

    http.begin(url);

    int code =
        http.GET();

    if (code != 200)
    {
        http.end();
        return;
    }

    String payload =
        http.getString();

    JsonDocument doc;

    DeserializationError error =
        deserializeJson(doc, payload);

    if (error)
    {
        Serial.print("[SMS OUTBOX] JSON ERROR = ");
        Serial.println(error.c_str());

        http.end();
        return;
    }

    if (!doc["success"].as<bool>())
    {
        http.end();
        return;
    }

    if (doc["data"].isNull())
    {
        http.end();
        return;
    }

    int id =
        doc["data"]["id"] | 0;

    String phone =
        doc["data"]["phone"] | "";

    String message =
        doc["data"]["message"] | "";

    phone =
        normalizePhoneForSMS(phone);

    http.end();

    if (
        id <= 0 ||
        phone.length() <= 5 ||
        message.length() == 0
    )
    {
        return;
    }

    Serial.println();
    Serial.println("=================================");
    Serial.println("[SMS OUTBOX] SEND PENDING SMS");
    Serial.print("ID: ");
    Serial.println(id);
    Serial.print("PHONE: ");
    Serial.println(phone);
    Serial.println("MESSAGE:");
    Serial.println(message);

    bool ok = false;

    if (xSemaphoreTake(simMutex, pdMS_TO_TICKS(5000)) == pdTRUE)
    {
        ok =
            sendSMS(
                phone,
                message
            );

        xSemaphoreGive(simMutex);
    }
    else
    {
        Serial.println("[SMS OUTBOX] MUTEX FAIL");
    }

    if (ok)
    {
        markSmsOutboxStatus(
            id,
            "sent"
        );
    }
    else
    {
        markSmsOutboxStatus(
            id,
            "failed"
        );
    }
}

// =====================================================
// SEND EVENT TO BACKEND BY WIFI
// =====================================================
void sendBackendEventWiFi(
    String eventType,
    String message
)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[EVENT] WIFI NOT CONNECTED");
        return;
    }

    HTTPClient http;

    http.begin(
        eventBackendUrl +
        "/api/events"
    );

    http.addHeader(
        "Content-Type",
        "application/json"
    );

    JsonDocument doc;

    doc["event_type"] =
        eventType;

    doc["message"] =
        message;

    if (gpsValid)
    {
        doc["gps_lat"] =
            gpsLat;

        doc["gps_lng"] =
            gpsLng;
    }
    else
    {
        doc["gps_lat"] =
            nullptr;

        doc["gps_lng"] =
            nullptr;
    }

    doc["network_type"] =
        "WIFI";

    String body;

    serializeJson(
        doc,
        body
    );

    int code =
        http.POST(body);

    Serial.print("[EVENT] POST = ");
    Serial.println(code);

    http.end();
}

// =====================================================
// APPEND GPS TO SMS
// =====================================================
void appendGPS(String &msg)
{
    if (!gpsValid)
    {
        msg += "\nGPS: Chua co tin hieu";
        return;
    }

    msg += "\n\nVi tri GPS:";
    msg += "\nLat: ";
    msg += String(gpsLat, 6);
    msg += "\nLng: ";
    msg += String(gpsLng, 6);

    msg += "\n\nGoogle Maps:";
    msg += "\nhttps://maps.google.com/?q=";
    msg += String(gpsLat, 6);
    msg += ",";
    msg += String(gpsLng, 6);
}

// =====================================================
// HANDLE SIM EVENT
// =====================================================
void handleSIMEvent(SystemEvent event)
{
    static EventType lastSavedType = EVENT_ALARM_OFF;
    static unsigned long lastSavedTime = 0;

    String msg = "";
    String eventName = "";
    bool needSMS = false;

    switch (event.type)
    {
        case EVENT_UNLOCK:
            Serial.println("[SIM] SKIP GENERIC UNLOCK EVENT");
            return;
        case EVENT_LOCK:
            eventName = "LOCK";
            msg =
                "SMART SAFE\n"
                "Su kien: Ket da khoa\n"
                "Trang thai: SECURE";
            needSMS = false;
            break;

        case EVENT_UNAUTHORIZED:
            eventName = "UNAUTHORIZED";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Cua bi mo trai phep\n"
                "Muc do: CANH BAO\n"
                "Trang thai: ALARM";
            appendGPS(msg);
            needSMS = true;
            break;

        case EVENT_VIBRATION:
            eventName = "VIBRATION";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Phat hien rung dong\n"
                "Muc do: CANH BAO\n"
                "Trang thai: WARNING";
            appendGPS(msg);
            needSMS = true;
            break;

        case EVENT_SMOKE:
            eventName = "SMOKE";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Phat hien khoi/gas\n"
                "Muc do: NGUY HIEM";
            appendGPS(msg);
            needSMS = true;
            break;

        case EVENT_PASSWORD_FAIL:
            eventName = "PASSWORD_FAIL";
            msg =
                "SMART SAFE\n"
                "Su kien: Nhap sai mat khau";
            needSMS = false;
            break;

        case EVENT_FLAME_DETECTED:
            eventName = "FLAME_DETECTED";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Phat hien lua\n"
                "Muc do: KHAN CAP\n"
                "Trang thai: FIRE ALARM";
            appendGPS(msg);
            needSMS = true;
            break;
        case EVENT_SAFE_MOVED:
            eventName = "SAFE_MOVED";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Ket bi di chuyen\n"
                "Muc do: NGUY HIEM\n"
                "Trang thai: GPS ALERT";
            appendGPS(msg);
            needSMS = true;
            break;
        default:
            return;
    }

    // chống lưu lịch sử lặp trong 5 giây
    if (
        event.type == lastSavedType &&
        millis() - lastSavedTime < 5000
    )
    {
        Serial.println("[SIM] SKIP DUPLICATE EVENT SAVE");
        return;
    }

    lastSavedType = event.type;
    lastSavedTime = millis();

    Serial.println();
    Serial.println("=================================");
    Serial.println("[SIM] EVENT HANDLE");
    Serial.println(eventName);
    Serial.println(msg);

    // Lưu lịch sử lên app/backend
    sendBackendEventWiFi(eventName, msg);

    // Chỉ SMS cho cảnh báo nguy hiểm
    if (needSMS)
    {
        Serial.println("[SIM] SEND SMS TO ALL");
        sendSMSAll(msg);
    }
    else
    {
        Serial.println("[SIM] NO SMS FOR NORMAL EVENT");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
}
// =====================================================
// TASK SIM
// =====================================================
void taskSIM(void *pv)
{
    simInit();
    loadSmsRecipientsLocal();
    simSerial.println("AT+CSQ");
    simWaitFor("OK", 3000);

    simSerial.println("AT+CREG?");
    simWaitFor("OK", 3000);

    vTaskDelay(
        pdMS_TO_TICKS(3000)
    );

    fetchSmsRecipients();
    if (smsPhoneCount == 0)
    {
        loadSmsRecipientsLocal();

        Serial.println("[SMS] USE LOCAL FALLBACK PHONES");
    }

    SystemEvent event;

    unsigned long lastFetchRecipients = 0;
    unsigned long lastCheckOutbox = 0;
while (1)
{
    // Kiểm tra OTP / SMS pending mỗi 1 giây
    if (
        millis() - lastCheckOutbox >
        SMS_OUTBOX_CHECK_INTERVAL
    )
    {
        lastCheckOutbox =
            millis();

        processSmsOutbox();
    }

    // Nhận event cảnh báo từ các task khác
    if (
        xQueueReceive(
            systemQueue,
            &event,
            pdMS_TO_TICKS(200)
        )
    )
    {
        Serial.println();
        Serial.println("=================================");
        Serial.println("[SIM] EVENT RECEIVED");

        handleSIMEvent(event);
    }

    // Cập nhật danh sách số nhận SMS mỗi 5 phút
    if (
        millis() - lastFetchRecipients >
        SMS_RECIPIENT_FETCH_INTERVAL
    )
    {
        lastFetchRecipients =
            millis();

        fetchSmsRecipients();
    }

    vTaskDelay(
        pdMS_TO_TICKS(50)
    );
}
}