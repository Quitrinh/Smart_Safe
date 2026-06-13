#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "core/globals.h"

String backendUrl = "http://smart-safe-api-etd9a7bsbhb6gyh8.southeastasia-01.azurewebsites.net";

extern String correctPassword;

void taskConfigSync(void *pv)
{
    while (1)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            HTTPClient http;

            http.begin(backendUrl + "/api/config");

            int httpCode = http.GET();

            if (httpCode == 200)
            {
                String payload = http.getString();

                DynamicJsonDocument doc(4096);
                deserializeJson(doc, payload);

                JsonArray data = doc["data"].as<JsonArray>();

                for (JsonObject item : data)
                {
                    String key = item["config_key"].as<String>();
                    String value = item["config_value"].as<String>();

                    if (key == "keypad_password")
                    {
                        correctPassword = value;

                        Serial.print("[CONFIG] Keypad Password = ");
                        Serial.println(correctPassword);
                    }

                    if (key == "wifi_ssid")
                    {
                        Serial.print("[CONFIG] WiFi SSID = ");
                        Serial.println(value);
                    }
                }
            }

            http.end();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}