// =====================================================
// task_gps.cpp
// EVENT DRIVEN GPS VERSION
// GPS chỉ lấy vị trí khi có cảnh báo
// =====================================================

#include <Arduino.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include "core/globals.h"
#include "core/system_bits.h"
#include "config/pins.h"

TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX); // GPS dùng software

extern double gpsLat;
extern double gpsLng;
extern bool gpsValid;

bool gpsConnected = false;
unsigned long lastGPSFix = 0;

// =============================
// READ GPS FUNCTION
// =============================
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

    return false;
}

void taskGPS(void *pv)
{
    gpsSerial.begin(9600);

    Serial.println("GPS TASK STARTED");

    unsigned long lastSendGPS = 0;

    while (1)
    {
        EventBits_t bits = xEventGroupGetBits(systemEvents);

        bool needGPS =
            (bits & BIT_NEED_GPS) ||
            ((bits & BIT_TRACKING_MODE) && millis() - lastSendGPS > 60000);

        if (!needGPS)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        xEventGroupClearBits(systemEvents, BIT_NEED_GPS);

        Serial.println("[GPS] GET LOCATION");

        bool ok = readGPS(8000);

        if (ok)
        {
            lastSendGPS = millis();

            xEventGroupSetBits(
                systemEvents,
                BIT_GPS_READY | BIT_NEED_SIM
            );
        }
        else
        {
            Serial.println("[GPS] NO FIX");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
// void taskGPS(void *pv)
// {
//     Serial.println("GPS TASK STARTED - FAKE GPS MODE");

//     while (1)
//     {
//         gpsLat = 10.841716;
//         gpsLng = 106.770843;
//         gpsValid = true;

//         xEventGroupSetBits(
//             systemEvents,
//             BIT_GPS_READY
//         );

//         vTaskDelay(
//             pdMS_TO_TICKS(1000)
//         );
//     }
// }