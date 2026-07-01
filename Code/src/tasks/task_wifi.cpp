// =====================================================
// task_wifi.cpp
// WIFI MANAGER + BACKEND WIFI CONFIG
// App -> Backend -> ESP32 đổi WiFi không cần reset
// =====================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "core/globals.h"

bool wifiReady = false;

extern String commandBackendUrl;

Preferences wifiConfigPrefs;

String lastBackendWifiSsid = "";
String lastBackendWifiPass = "";

unsigned long lastWifiReconnect = 0;
unsigned long wifiDisconnectedAt = 0;
unsigned long lastFetchWifiConfig = 0;

const unsigned long WIFI_CONFIG_FETCH_INTERVAL = 30000;
const unsigned long WIFI_RECONNECT_INTERVAL = 5000;
const unsigned long WIFI_RESTART_TIMEOUT = 60000;

static const bool RESET_WIFI_CONFIG_ONCE = false;

bool isPlaceholderSsid(String ssid)
{
    ssid.trim();

    return ssid == "TEN_WIFI_2_4G" ||
           ssid == "TEN_WIFI_THAT_CUA_BAN" ||
           ssid == "SSID";
}

bool isInvalidBackendSsid(String ssid)
{
    ssid.trim();

    return ssid.length() == 0 || isPlaceholderSsid(ssid);
}

void clearBackendWifiMemory()
{
    wifiConfigPrefs.begin("wifi_cfg", false);
    wifiConfigPrefs.clear();
    wifiConfigPrefs.end();

    lastBackendWifiSsid = "";
    lastBackendWifiPass = "";

    Serial.println("[WIFI CFG] CLEARED BACKEND WIFI MEMORY");
}

void loadLastBackendWifiConfig()
{
    wifiConfigPrefs.begin("wifi_cfg", true);

    lastBackendWifiSsid =
        wifiConfigPrefs.getString("ssid", "");

    lastBackendWifiPass =
        wifiConfigPrefs.getString("pass", "");

    wifiConfigPrefs.end();

    Serial.print("[WIFI CFG] LAST SSID = ");
    Serial.println(lastBackendWifiSsid);
}

void saveLastBackendWifiConfig(String ssid, String pass)
{
    wifiConfigPrefs.begin("wifi_cfg", false);

    wifiConfigPrefs.putString("ssid", ssid);
    wifiConfigPrefs.putString("pass", pass);

    wifiConfigPrefs.end();

    lastBackendWifiSsid = ssid;
    lastBackendWifiPass = pass;

    Serial.println("[WIFI CFG] SAVED CONFIG");
}

bool connectToWifi(String ssid, String pass, uint32_t timeoutMs)
{
    Serial.println("=================================");
    Serial.println("[WIFI CFG] CONNECT WIFI");
    Serial.print("[WIFI CFG] SSID = ");
    Serial.println(ssid);
    Serial.println("=================================");

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    WiFi.begin(
        ssid.c_str(),
        pass.c_str()
    );

    unsigned long start = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - start < timeoutMs
    )
    {
        Serial.print(".");
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("[WIFI CFG] WIFI CONNECTED");
        Serial.print("[WIFI CFG] IP = ");
        Serial.println(WiFi.localIP());

        wifiReady = true;
        wifiDisconnectedAt = 0;

        return true;
    }

    Serial.println("[WIFI CFG] WIFI CONNECT FAILED");

    wifiReady = false;

    return false;
}

void applyNewWifiConfig(String ssid, String pass)
{
    ssid.trim();

    if (isInvalidBackendSsid(ssid))
    {
        Serial.println("[WIFI CFG] INVALID SSID, SKIP APPLY");
        return;
    }

    saveLastBackendWifiConfig(ssid, pass);

    bool ok =
        connectToWifi(
            ssid,
            pass,
            20000
        );

    if (ok)
    {
        Serial.println("[WIFI CFG] NEW WIFI APPLIED WITHOUT RESET");
    }
    else
    {
        Serial.println("[WIFI CFG] NEW WIFI FAILED, KEEP TASK RUNNING");
    }
}

void fetchWifiConfigFromBackend()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[WIFI CFG] WIFI NOT CONNECTED");
        return;
    }

    HTTPClient http;

    String url =
        commandBackendUrl +
        "/api/esp32/config";

    Serial.print("[WIFI CFG] GET URL = ");
    Serial.println(url);

    http.begin(url);
    http.setTimeout(8000);
    http.setReuse(false);

    int code =
        http.GET();

    Serial.print("[WIFI CFG] HTTP CODE = ");
    Serial.println(code);

    String response =
        http.getString();

    Serial.print("[WIFI CFG] RESPONSE = ");
    Serial.println(response);

    http.end();

    if (code != 200)
    {
        return;
    }

    JsonDocument doc;

    DeserializationError error =
        deserializeJson(doc, response);

    if (error)
    {
        Serial.print("[WIFI CFG] JSON ERROR = ");
        Serial.println(error.c_str());
        return;
    }

    bool success =
        doc["success"] | false;

    if (!success)
    {
        Serial.println("[WIFI CFG] BACKEND SUCCESS FALSE");
        return;
    }

    String newSsid =
        doc["data"]["wifi_ssid"] | "";

    String newPass =
        doc["data"]["wifi_password"] | "";

    newSsid.trim();

    if (isInvalidBackendSsid(newSsid))
    {
        Serial.print("[WIFI CFG] INVALID BACKEND SSID, SKIP: ");
        Serial.println(newSsid);
        return;
    }

    bool changed =
        newSsid != lastBackendWifiSsid ||
        newPass != lastBackendWifiPass;

    if (!changed)
    {
        Serial.println("[WIFI CFG] WIFI CONFIG NOT CHANGED");
        return;
    }

    Serial.println("[WIFI CFG] WIFI CONFIG CHANGED");

    applyNewWifiConfig(
        newSsid,
        newPass
    );
}

void taskWiFi(void *pv)
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("[WIFI] TASK STARTED");
    Serial.println("=================================");

    loadLastBackendWifiConfig();

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname("SMART_SAFE_ESP32");

    WiFiManager wm;

    if (RESET_WIFI_CONFIG_ONCE)
    {
        Serial.println("[WIFI] RESET WIFI CONFIG ONCE");

        wm.resetSettings();
        clearBackendWifiMemory();

        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (isPlaceholderSsid(lastBackendWifiSsid))
    {
        Serial.println("[WIFI] FOUND PLACEHOLDER SSID, CLEAR WIFI SETTINGS");

        wm.resetSettings();
        clearBackendWifiMemory();

        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    wm.setDebugOutput(true);
    wm.setConfigPortalTimeout(180);
    wm.setConnectTimeout(20);

    Serial.println("[WIFI] START CONFIG");
    Serial.println("[WIFI] AP NAME: SMART_SAFE_SETUP");
    Serial.println("[WIFI] AP PASS: 12345678");

    bool connected =
        wm.autoConnect(
            "SMART_SAFE_SETUP",
            "12345678"
        );

    if (!connected)
    {
        Serial.println("[WIFI] CONNECT FAIL OR PORTAL TIMEOUT");

        wifiReady = false;

        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP.restart();
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("[WIFI] CONNECTED");
        Serial.print("[WIFI] SSID: ");
        Serial.println(WiFi.SSID());

        Serial.print("[WIFI] IP: ");
        Serial.println(WiFi.localIP());

        Serial.print("[WIFI] RSSI: ");
        Serial.println(WiFi.RSSI());

        wifiReady = true;
        wifiDisconnectedAt = 0;

        if (lastBackendWifiSsid.length() == 0)
        {
            saveLastBackendWifiConfig(
                WiFi.SSID(),
                lastBackendWifiPass
            );
        }
    }
    else
    {
        Serial.println("[WIFI] AUTO CONNECT RETURNED BUT WIFI NOT CONNECTED");

        wifiReady = false;

        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
    }

    vTaskDelay(pdMS_TO_TICKS(5000));

    lastFetchWifiConfig = millis();
    fetchWifiConfigFromBackend();

    while (1)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            if (!wifiReady)
            {
                Serial.println("[WIFI] RECONNECTED");
                Serial.print("[WIFI] IP = ");
                Serial.println(WiFi.localIP());
            }

            wifiReady = true;
            wifiDisconnectedAt = 0;

            if (
                millis() - lastFetchWifiConfig >
                WIFI_CONFIG_FETCH_INTERVAL
            )
            {
                lastFetchWifiConfig = millis();

                fetchWifiConfigFromBackend();
            }
        }
        else
        {
            if (wifiReady)
            {
                Serial.println("[WIFI] LOST CONNECTION");
            }

            wifiReady = false;

            if (wifiDisconnectedAt == 0)
            {
                wifiDisconnectedAt = millis();
            }

            if (
                millis() - lastWifiReconnect >
                WIFI_RECONNECT_INTERVAL
            )
            {
                lastWifiReconnect = millis();

                Serial.println("[WIFI] TRY RECONNECT...");

                WiFi.disconnect();
                vTaskDelay(pdMS_TO_TICKS(500));

                if (lastBackendWifiSsid.length() > 0)
                {
                    WiFi.begin(
                        lastBackendWifiSsid.c_str(),
                        lastBackendWifiPass.c_str()
                    );
                }
                else
                {
                    WiFi.reconnect();
                }
            }

            if (
                millis() - wifiDisconnectedAt >
                WIFI_RESTART_TIMEOUT
            )
            {
                Serial.println("[WIFI] DISCONNECTED TOO LONG, RESTART");

                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP.restart();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}