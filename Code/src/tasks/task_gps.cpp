// =====================================================
// task_gps.cpp
// GPS chi doc vi tri, khong gui HTTP
// SMART SAFE
// =====================================================

#include <Arduino.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

#include "core/globals.h"
#include "core/system_bits.h"
#include "config/pins.h"

// =====================================================
// GPS OBJECT
// =====================================================
TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);

// =====================================================
// GLOBAL GPS DATA
// =====================================================
extern double gpsLat;
extern double gpsLng;
extern bool gpsValid;

// Config được cập nhật từ backend /api/esp32/config
extern bool gpsAlertEnabled;

// =====================================================
// STATE
// =====================================================
bool gpsConnected = false;
unsigned long lastGPSFix = 0;

// =====================================================
// CONFIG
// =====================================================
#define GPS_BAUD_RATE 9600
#define GPS_READ_TIMEOUT_MS 8000
#define GPS_TRACKING_INTERVAL_MS 60000
#define GPS_LOOP_DELAY_MS 1000

// =====================================================
// READ GPS
// =====================================================
bool readGPS(uint32_t timeoutMs)
{
    unsigned long start = millis();

    while (millis() - start < timeoutMs)
    {
        while (gpsSerial.available())
        {
            char c = gpsSerial.read();
            gps.encode(c);
        }

        if (gps.location.isUpdated())
        {
            gpsLat = gps.location.lat();
            gpsLng = gps.location.lng();
            gpsValid = true;
            gpsConnected = true;
            lastGPSFix = millis();

            Serial.println();
            Serial.println("====== GPS OK ======");
            Serial.print("LAT: ");
            Serial.println(gpsLat, 6);
            Serial.print("LNG: ");
            Serial.println(gpsLng, 6);
            Serial.println("====================");

            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    Serial.println("[GPS] NO FIX TIMEOUT");

    return false;
}

// =====================================================
// TASK GPS
// =====================================================
void taskGPS(void *pv)
{
    gpsSerial.begin(GPS_BAUD_RATE);

    Serial.println();
    Serial.println("[GPS] TASK STARTED");

    unsigned long lastReadGPS = 0;
    unsigned long lastDisabledLog = 0;

    while (1)
    {
        EventBits_t bits = xEventGroupGetBits(systemEvents);

        bool needGPSByAlert =
            bits & BIT_NEED_GPS;

        bool needGPSByTracking =
            (bits & BIT_TRACKING_MODE) &&
            (millis() - lastReadGPS > GPS_TRACKING_INTERVAL_MS);

        bool needGPS =
            needGPSByAlert || needGPSByTracking;

        // =================================================
        // GPS CONFIG DISABLED
        // =================================================
        if (!gpsAlertEnabled)
        {
            // Nếu có task khác yêu cầu GPS thì bỏ qua
            if (needGPS)
            {
                xEventGroupClearBits(systemEvents, BIT_NEED_GPS);

                if (millis() - lastDisabledLog > 5000)
                {
                    lastDisabledLog = millis();
                    Serial.println("[GPS] DISABLED BY CONFIG");
                }
            }

            vTaskDelay(pdMS_TO_TICKS(GPS_LOOP_DELAY_MS));
            continue;
        }

        // =================================================
        // NO NEED GPS
        // =================================================
        if (!needGPS)
        {
            vTaskDelay(pdMS_TO_TICKS(GPS_LOOP_DELAY_MS));
            continue;
        }

        // Xoá yêu cầu GPS một lần
        xEventGroupClearBits(systemEvents, BIT_NEED_GPS);

        Serial.println();
        Serial.println("[GPS] GET LOCATION");

        bool ok = readGPS(GPS_READ_TIMEOUT_MS);

        if (ok)
        {
            lastReadGPS = millis();

            xEventGroupSetBits(
                systemEvents,
                BIT_GPS_READY
            );

            // Nếu GPS được yêu cầu do cảnh báo,
            // sau khi có GPS thì cho phép SIM gửi cảnh báo
            if (needGPSByAlert)
            {
                xEventGroupSetBits(
                    systemEvents,
                    BIT_NEED_SIM
                );
            }

            Serial.println("[GPS] READY");
        }
        else
        {
            Serial.println("[GPS] NO FIX");

            // Nếu không lấy được GPS nhưng đây là cảnh báo,
            // vẫn có thể cho SIM gửi cảnh báo không kèm vị trí.
            // Nếu bạn không muốn gửi SMS khi không có GPS thì xoá đoạn này.
            if (needGPSByAlert)
            {
                xEventGroupSetBits(
                    systemEvents,
                    BIT_NEED_SIM
                );
            }
        }

        vTaskDelay(pdMS_TO_TICKS(GPS_LOOP_DELAY_MS));
    }
}