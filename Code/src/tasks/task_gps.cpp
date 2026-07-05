// =====================================================
// task_gps.cpp
// GPS READ + SAFE MOVED DETECT
// SMART SAFE
// =====================================================

#include <Arduino.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

#include "core/globals.h"
#include "core/system_bits.h"
#include "core/events.h"
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

extern double homeLat;
extern double homeLng;
extern int gpsAllowedRadiusM;
extern bool gpsAlertEnabled;

// =====================================================
// STATE
// =====================================================
bool gpsConnected = false;
bool gpsMoveAlarmActive = false;

unsigned long lastGPSFix = 0;

// =====================================================
// CONFIG
// =====================================================
#define GPS_BAUD_RATE 9600
#define GPS_READ_TIMEOUT_MS 8000
#define GPS_TRACKING_INTERVAL_MS 60000
#define GPS_LOOP_DELAY_MS 1000

// =====================================================
// CHECK SAFE MOVED
// =====================================================
void checkSafeMoved()
{
    if (!gpsAlertEnabled)
    {
        return;
    }

    if (!gpsValid)
    {
        return;
    }

    if (homeLat == 0 || homeLng == 0)
    {
        Serial.println("[GPS] HOME LOCATION NOT SET");
        return;
    }

    double distance =
        TinyGPSPlus::distanceBetween(
            gpsLat,
            gpsLng,
            homeLat,
            homeLng
        );

    Serial.print("[GPS] DISTANCE FROM HOME = ");
    Serial.print(distance);
    Serial.println(" m");

    if (
        distance > gpsAllowedRadiusM &&
        !gpsMoveAlarmActive
    )
    {
        gpsMoveAlarmActive = true;

        Serial.println("[GPS] SAFE MOVED ALERT");

        xEventGroupSetBits(
            systemEvents,
            BIT_ALARM_ACTIVE |
            BIT_NEED_SIM |
            BIT_TRACKING_MODE
        );

        SystemEvent event;
        event.type = EVENT_SAFE_MOVED;

        strcpy(
            event.message,
            "SAFE_MOVED"
        );

        xQueueSend(
            systemQueue,
            &event,
            0
        );
    }

    if (
        gpsMoveAlarmActive &&
        distance <= gpsAllowedRadiusM
    )
    {
        gpsMoveAlarmActive = false;

        Serial.println("[GPS] SAFE BACK TO HOME AREA");
    }
}

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

            checkSafeMoved();

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
        EventBits_t bits =
            xEventGroupGetBits(systemEvents);

        bool needGPSByAlert =
            bits & BIT_NEED_GPS;

        bool needGPSByTracking =
            (bits & BIT_TRACKING_MODE) &&
            (millis() - lastReadGPS > GPS_TRACKING_INTERVAL_MS);

        bool needGPSPeriodic =
            millis() - lastReadGPS > GPS_TRACKING_INTERVAL_MS;

        bool needGPS =
            needGPSByAlert || needGPSByTracking || needGPSPeriodic;
        if (!gpsAlertEnabled)
        {
            gpsMoveAlarmActive = false;

            if (needGPS)
            {
                xEventGroupClearBits(
                    systemEvents,
                    BIT_NEED_GPS
                );

                if (millis() - lastDisabledLog > 5000)
                {
                    lastDisabledLog = millis();
                    Serial.println("[GPS] DISABLED BY CONFIG");
                }
            }

            vTaskDelay(pdMS_TO_TICKS(GPS_LOOP_DELAY_MS));
            continue;
        }

        if (!needGPS)
        {
            vTaskDelay(pdMS_TO_TICKS(GPS_LOOP_DELAY_MS));
            continue;
        }

        xEventGroupClearBits(
            systemEvents,
            BIT_NEED_GPS
        );

        Serial.println();
        Serial.println("[GPS] GET LOCATION");

        bool ok =
            readGPS(GPS_READ_TIMEOUT_MS);

        if (ok)
        {
            lastReadGPS = millis();

            xEventGroupSetBits(
                systemEvents,
                BIT_GPS_READY
            );

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