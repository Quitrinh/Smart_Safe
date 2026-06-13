// =====================================================
// task_sim.cpp
// SMS MULTI PHONE + WIFI EVENT LOG VERSION
// =====================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "core/globals.h"
#include "core/events.h"

// =====================================================
// UART SIM
// =====================================================
HardwareSerial simSerial(1);

#define SIM_RX 25
#define SIM_TX 26

// =====================================================
// BACKEND
// =====================================================
String eventBackendUrl = "http://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

// =====================================================
// SMS PHONE LIST
// =====================================================
String smsPhones[] =
{
    "+84986831371"
};

const int SMS_PHONE_COUNT =
    sizeof(smsPhones) /
    sizeof(smsPhones[0]);

// =====================================================
// LAST EVENT
// =====================================================
EventType lastEventType = EVENT_ALARM_OFF;
unsigned long lastEventTime = 0;

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

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    return false;
}

// =====================================================
// SEND ONE SMS
// =====================================================
bool sendSMS(String phone, String message)
{
    Serial.println();
    Serial.println("=================================");
    Serial.print("[SMS] SEND TO: ");
    Serial.println(phone);

    while (simSerial.available())
    {
        simSerial.read();
    }

    simSerial.println("AT");
    if (!simWaitFor("OK", 3000))
    {
        Serial.println("[SMS] AT FAIL");
        return false;
    }

    simSerial.println("AT+CMGF=1");
    if (!simWaitFor("OK", 3000))
    {
        Serial.println("[SMS] CMGF FAIL");
        return false;
    }

    simSerial.println("AT+CSCS=\"GSM\"");
    simWaitFor("OK", 3000);

    simSerial.print("AT+CMGS=\"");
    simSerial.print(phone);
    simSerial.println("\"");

    if (!simWaitFor(">", 7000))
    {
        Serial.println("[SMS] NO > PROMPT");
        return false;
    }

    simSerial.print(message);
    delay(500);
    simSerial.write(26);

    if (!simWaitFor("OK", 20000))
    {
        Serial.println("[SMS] SEND FAIL");
        return false;
    }

    Serial.println("[SMS] SEND OK");
    return true;
}

// =====================================================
// SEND SMS TO ALL PHONES
// =====================================================
void sendSMSAll(String message)
{
    if (xSemaphoreTake(simMutex, 5000 / portTICK_PERIOD_MS) != pdTRUE)
    {
        Serial.println("[SMS] MUTEX FAIL");
        return;
    }

    for (int i = 0; i < SMS_PHONE_COUNT; i++)
    {
        if (smsPhones[i].length() > 5)
        {
            sendSMS(
                smsPhones[i],
                message
            );

            vTaskDelay(
                3000 / portTICK_PERIOD_MS
            );
        }
    }

    xSemaphoreGive(simMutex);
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

void handleSIMEvent(SystemEvent event)
{
    if (event.type == lastEventType)
    {
        if (millis() - lastEventTime < 3000)
        {
            Serial.println("[SIM] DUPLICATE EVENT");
            return;
        }
    }

    lastEventType = event.type;
    lastEventTime = millis();

    String msg = "";
    String eventName = "";

    switch (event.type)
    {
        case EVENT_UNLOCK:
            eventName = "UNLOCK";
            msg =
                "SMART SAFE\n"
                "Su kien: Mo ket thanh cong\n"
                "Trang thai: OPEN\n"
                "Nguon: Xac thuc hop le";
            break;

        case EVENT_LOCK:
            eventName = "LOCK";
            msg =
                "SMART SAFE\n"
                "Su kien: Ket da khoa\n"
                "Trang thai: SECURE";
            break;

        case EVENT_UNAUTHORIZED:
            eventName = "UNAUTHORIZED";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Cua bi mo trai phep\n"
                "Muc do: CANH BAO\n"
                "Trang thai: ALARM";
            appendGPS(msg);
            break;

        case EVENT_VIBRATION:
            eventName = "VIBRATION";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Phat hien rung dong\n"
                "Muc do: CANH BAO\n"
                "Trang thai: WARNING";
            appendGPS(msg);
            break;

        case EVENT_SMOKE:
            eventName = "SMOKE";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Phat hien khoi/gas\n"
                "Muc do: NGUY HIEM\n"
                "Trang thai: FIRE WARNING";
            appendGPS(msg);
            break;

        case EVENT_PASSWORD_FAIL:
            eventName = "PASSWORD_FAIL";
            msg =
                "SMART SAFE\n"
                "Su kien: Nhap sai mat khau\n"
                "Muc do: CANH BAO";
            break;

        case EVENT_FLAME_DETECTED:
            eventName = "FLAME_DETECTED";
            msg =
                "SMART SAFE ALERT\n"
                "Su kien: Phat hien lua\n"
                "Muc do: KHAN CAP\n"
                "Trang thai: FIRE ALARM\n"
                "Xu ly: Da kich hoat coi va bom";
            appendGPS(msg);
            break;

        default:
            return;
    }

    Serial.println();
    Serial.println("=================================");
    Serial.println("[SIM] EVENT HANDLE");
    Serial.println(eventName);
    Serial.println(msg);

    sendBackendEventWiFi(eventName, msg);

    Serial.println("[SIM] SEND SMS TO ALL");
    sendSMSAll(msg);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

// =====================================================
// TASK SIM
// =====================================================
void taskSIM(void *pv)
{
    simInit();
    simSerial.println("AT+CSQ");
    simWaitFor("OK", 3000);

    simSerial.println("AT+CREG?");
    simWaitFor("OK", 3000);
    vTaskDelay(
        3000 / portTICK_PERIOD_MS
    );

    sendSMSAll(
        "SMART SAFE ONLINE"
    );

    SystemEvent event;

    while (1)
    {
        if (
            xQueueReceive(
                systemQueue,
                &event,
                portMAX_DELAY
            )
        )
        {
            Serial.println();
            Serial.println("=================================");
            Serial.println("[SIM] EVENT RECEIVED");

            handleSIMEvent(
                event
            );
        }

        vTaskDelay(
            50 / portTICK_PERIOD_MS
        );
    }
}