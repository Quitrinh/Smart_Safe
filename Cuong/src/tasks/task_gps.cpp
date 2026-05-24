// =====================================================
// task_gps.cpp
// FINAL CLEAN VERSION
// =====================================================

#include <Arduino.h>

#include <TinyGPS++.h>

#include "core/globals.h"
#include "config/pins.h"


// =====================================================
// GPS
// =====================================================
TinyGPSPlus gps;

HardwareSerial gpsSerial(2);


// =====================================================
// EXTERN
// =====================================================
extern double gpsLat;

extern double gpsLng;

extern bool gpsValid;


// =====================================================
// GPS STATUS
// =====================================================
bool gpsConnected = false;

unsigned long lastGPSFix = 0;

unsigned long lastDebugTime = 0;


// =====================================================
// TASK GPS
// =====================================================
void taskGPS(void *pv)
{
    // =============================================
    // GPS UART
    //
    // GPS TX -> ESP32 RX
    // GPS RX -> ESP32 TX
    // =============================================
    gpsSerial.begin(
        9600,
        SERIAL_8N1,
        GPS_RX,
        GPS_TX
    );

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "GPS TASK STARTED"
    );

    Serial.println(
        "WAIT GPS SIGNAL..."
    );

    Serial.println(
        "================================="
    );

    while (1)
    {
        // =========================================
        // READ GPS DATA
        // =========================================
        while (
            gpsSerial.available()
        )
        {
            char c =
                gpsSerial.read();

            gps.encode(c);

            gpsConnected = true;
        }

        // =========================================
        // GPS VALID
        // =========================================
        if (
            gps.location.isValid()
        )
        {
            gpsValid = true;
        }

        // =========================================
        // GPS UPDATED
        // =========================================
        if (
            gps.location.isUpdated()
        )
        {
            gpsLat =
                gps.location.lat();

            gpsLng =
                gps.location.lng();

            lastGPSFix =
                millis();

            Serial.println();

            Serial.println(
                "========== GPS =========="
            );

            // =====================================
            // LAT
            // =====================================
            Serial.print(
                "LAT: "
            );

            Serial.println(
                gpsLat,
                6
            );

            // =====================================
            // LNG
            // =====================================
            Serial.print(
                "LNG: "
            );

            Serial.println(
                gpsLng,
                6
            );

            // =====================================
            // SATELLITES
            // =====================================
            Serial.print(
                "SAT: "
            );

            Serial.println(
                gps.satellites.value()
            );

            // =====================================
            // HDOP
            // =====================================
            Serial.print(
                "HDOP: "
            );

            Serial.println(
                gps.hdop.hdop()
            );

            // =====================================
            // SPEED
            // =====================================
            Serial.print(
                "SPEED KMH: "
            );

            Serial.println(
                gps.speed.kmph()
            );

            // =====================================
            // ALTITUDE
            // =====================================
            Serial.print(
                "ALTITUDE: "
            );

            Serial.println(
                gps.altitude.meters()
            );

            // =====================================
            // GOOGLE MAP
            // =====================================
            Serial.println();

            Serial.println(
                "GOOGLE MAP:"
            );

            Serial.print(
                "https://maps.google.com/?q="
            );

            Serial.print(
                gpsLat,
                6
            );

            Serial.print(",");

            Serial.println(
                gpsLng,
                6
            );

            Serial.println(
                "========================="
            );
        }

        // =========================================
        // DEBUG EVERY 5S
        // =========================================
        if (
            millis() - lastDebugTime
            > 5000
        )
        {
            lastDebugTime =
                millis();

            // =====================================
            // NO GPS UART
            // =====================================
            if (!gpsConnected)
            {
                Serial.println(
                    "[GPS] NO UART DATA"
                );
            }

            // =====================================
            // WAIT SATELLITE
            // =====================================
            else if (
                !gps.location.isValid()
            )
            {
                Serial.println(
                    "[GPS] WAIT SATELLITE..."
                );

                Serial.print(
                    "SAT: "
                );

                Serial.println(
                    gps.satellites.value()
                );

                Serial.print(
                    "HDOP: "
                );

                Serial.println(
                    gps.hdop.hdop()
                );
            }
        }

        // =========================================
        // GPS LOST
        // =========================================
        if (
            gpsValid &&
            millis() - lastGPSFix
            > 10000
        )
        {
            Serial.println(
                "[GPS] SIGNAL LOST"
            );

            gpsValid = false;
        }

        // =========================================
        // TASK DELAY
        // =========================================
        vTaskDelay(
            100 /
            portTICK_PERIOD_MS
        );
    }
}