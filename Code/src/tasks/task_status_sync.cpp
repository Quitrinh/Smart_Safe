#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "core/system_bits.h"
#include "core/globals.h"

String statusBackendUrl = "http://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

void taskStatusSync(void *pv)
{
    while (1)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            HTTPClient http;

            http.begin(
                statusBackendUrl +
                "/api/safe/status"
            );

            http.addHeader(
                "Content-Type",
                "application/json"
            );

            DynamicJsonDocument doc(512);

            doc["safe_state"] =
                safeState == SAFE_OPEN
                ? "OPEN"
                : "LOCKED";

            doc["wifi_status"] =
                "ONLINE";

            doc["sim_status"] =
                "READY";

            doc["gps_status"] =
                gpsValid
                ? "ACTIVE"
                : "NO_FIX";

            EventBits_t bits =
                xEventGroupGetBits(
                    systemEvents
                );

            doc["alarm_status"] =
                (bits & BIT_ALARM_ACTIVE)
                ? "ON"
                : "OFF";

            doc["flame_status"] =
                (bits & BIT_FLAME_ACTIVE)
                ? "FIRE"
                : "NORMAL";

            doc["pump_status"] =
                (bits & BIT_FLAME_ACTIVE)
                ? "ON"
                : "OFF";

            String body;

            serializeJson(
                doc,
                body
            );

            int code =
                http.POST(body);

            Serial.print(
                "[STATUS] POST = "
            );

            Serial.println(code);

            http.end();
        }

        vTaskDelay(
            pdMS_TO_TICKS(5000)
        );
    }
}