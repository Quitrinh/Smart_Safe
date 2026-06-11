#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "core/globals.h"

bool wifiReady = false;

void taskWiFi(void *pv)
{
    WiFiManager wm;

    Serial.println("[WIFI] START CONFIG");

    bool connected = wm.autoConnect(
        "SMART_SAFE_SETUP",
        "12345678"
    );

    if (!connected)
    {
        Serial.println("[WIFI] CONNECT FAIL");
        wifiReady = false;
    }
    else
    {
        Serial.println("[WIFI] CONNECTED");
        Serial.println(WiFi.localIP());
        wifiReady = true;
    }

    while (1)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            wifiReady = true;
        }
        else
        {
            wifiReady = false;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}