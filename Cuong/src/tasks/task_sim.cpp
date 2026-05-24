// =====================================================
// task_sim.cpp
// FINAL GPS PRODUCTION VERSION
// =====================================================

#include <Arduino.h>

#include "services/telegram_service.h"

#include "core/globals.h"
#include "core/events.h"


// =====================================================
// UART SIM
// =====================================================
HardwareSerial simSerial(1);


// =====================================================
// LAST EVENT
// chống spam telegram
// =====================================================
EventType lastEventType =
    EVENT_ALARM_OFF;

unsigned long
lastEventTime = 0;


// =====================================================
// INIT SIM
// =====================================================
void simInit()
{
    simSerial.begin(
        115200,
        SERIAL_8N1,
        25,
        26
    );

    delay(3000);

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "[SIM] INIT DONE"
    );
}


// =====================================================
// SEND AT
// =====================================================
bool simSendCommand(
    String cmd,
    String expected,
    int timeout = 3000
)
{
    // =============================================
    // TAKE MUTEX
    // =============================================
    if (
        xSemaphoreTake(
            simMutex,
            5000 /
            portTICK_PERIOD_MS
        ) != pdTRUE
    )
    {
        return false;
    }

    Serial.println();

    Serial.println(
        "[SIM] SEND: " + cmd
    );

    // =============================================
    // CLEAR BUFFER
    // =============================================
    while (
        simSerial.available()
    )
    {
        simSerial.read();
    }

    simSerial.println(cmd);

    String response = "";

    unsigned long start =
        millis();

    while (
        millis() - start
        < timeout
    )
    {
        while (
            simSerial.available()
        )
        {
            char c =
                simSerial.read();

            response += c;
        }

        vTaskDelay(
            10 /
            portTICK_PERIOD_MS
        );
    }

    Serial.println(
        response
    );

    // =============================================
    // RELEASE MUTEX
    // =============================================
    xSemaphoreGive(
        simMutex
    );

    return (
        response.indexOf(expected)
        != -1
    );
}


// =====================================================
// CONNECT NETWORK
// =====================================================
bool simConnect()
{
    Serial.println();

    Serial.println(
        "[SIM] CONNECT NETWORK"
    );

    // =============================================
    // BASIC AT
    // =============================================
    if (
        !simSendCommand(
            "AT",
            "OK"
        )
    )
    {
        return false;
    }

    // =============================================
    // PDP CONTEXT
    // =============================================
    if (
        !simSendCommand(
            "AT+CGDCONT=1,\"IP\",\"v-internet\"",
            "OK"
        )
    )
    {
        return false;
    }

    // =============================================
    // NETWORK OPEN
    // =============================================
    if (
        !simSendCommand(
            "AT+NETOPEN",
            "OK",
            8000
        )
    )
    {
        return false;
    }

    Serial.println();

    Serial.println(
        "[SIM] NETWORK OK"
    );

    return true;
}


// =====================================================
// APPEND GPS
// =====================================================
void appendGPS(String &msg)
{
    if (!gpsValid)
    {
        return;
    }

    msg +=
    "\n\n📍 Location:\n";

    msg +=
    String(gpsLat, 6);

    msg += ",";

    msg +=
    String(gpsLng, 6);

    msg +=
    "\n\n🗺 Map:\n";

    msg +=
    "https://maps.google.com/?q=";

    msg +=
    String(gpsLat, 6);

    msg += ",";

    msg +=
    String(gpsLng, 6);
}


// =====================================================
// HANDLE EVENT
// =====================================================
void handleSIMEvent(
    SystemEvent event
)
{
    // =============================================
    // ANTI DUPLICATE
    // =============================================
    if (
        event.type ==
        lastEventType
    )
    {
        if (
            millis() - lastEventTime
            < 3000
        )
        {
            Serial.println(
                "[SIM] DUPLICATE EVENT"
            );

            return;
        }
    }

    // =============================================
    // SAVE EVENT
    // =============================================
    lastEventType =
        event.type;

    lastEventTime =
        millis();

    // =============================================
    // MESSAGE
    // =============================================
    String msg = "";

    switch(event.type)
    {
        // =========================================
        // SAFE OPEN
        // =========================================
        case EVENT_UNLOCK:

            msg =
            "🔓 SMART SAFE\n\n"
            "Safe Opened\n"
            "Status: OPEN";

            break;

        // =========================================
        // SAFE LOCK
        // =========================================
        case EVENT_LOCK:

            msg =
            "🔒 SMART SAFE\n\n"
            "Safe Locked\n"
            "Status: SECURE";

            break;

        // =========================================
        // UNAUTHORIZED
        // =========================================
        case EVENT_UNAUTHORIZED:

            msg =
            "🚨 SMART SAFE ALERT\n\n"
            "Unauthorized Access\n"
            "Status: ALARM";

            appendGPS(msg);

            break;

        // =========================================
        // VIBRATION
        // =========================================
        case EVENT_VIBRATION:

            msg =
            "🚨 SMART SAFE ALERT\n\n"
            "Vibration Detected\n"
            "Status: WARNING";

            appendGPS(msg);

            break;

        // =========================================
        // SMOKE
        // =========================================
        case EVENT_SMOKE:

            msg =
            "🔥 SMART SAFE ALERT\n\n"
            "Smoke Detected\n"
            "Status: FIRE WARNING";

            appendGPS(msg);

            break;

        // =========================================
        // PASSWORD FAIL
        // =========================================
        case EVENT_PASSWORD_FAIL:

            msg =
            "⚠️ SMART SAFE\n\n"
            "Wrong Password Attempt";

            break;

        default:
            return;
    }

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "[SIM] SEND TELEGRAM"
    );

    // =============================================
    // SEND TELEGRAM
    // =============================================
    sendTelegram(
        msg
    );

    // =============================================
    // REST SIM
    // =============================================
    vTaskDelay(
        1000 /
        portTICK_PERIOD_MS
    );
}


// =====================================================
// TASK SIM
// =====================================================
void taskSIM(void *pv)
{
    // =============================================
    // INIT SIM
    // =============================================
    simInit();

    vTaskDelay(
        3000 /
        portTICK_PERIOD_MS
    );

    // =============================================
    // CONNECT NETWORK
    // =============================================
    bool connected =
        simConnect();

    // =============================================
    // ONLINE
    // =============================================
    if (connected)
    {
        sendTelegram(
            "🟢 SMART SAFE ONLINE"
        );
    }

    // =============================================
    // EVENT
    // =============================================
    SystemEvent event;

    // =============================================
    // MAIN LOOP
    // =============================================
    while (1)
    {
        // =========================================
        // RECEIVE EVENT
        // =========================================
        if (
            xQueueReceive(
                systemQueue,
                &event,
                portMAX_DELAY
            )
        )
        {
            Serial.println();

            Serial.println(
                "================================="
            );

            Serial.println(
                "[SIM] EVENT RECEIVED"
            );

            handleSIMEvent(
                event
            );
        }

        // =========================================
        // TASK DELAY
        // =========================================
        vTaskDelay(
            50 /
            portTICK_PERIOD_MS
        );
    }
}