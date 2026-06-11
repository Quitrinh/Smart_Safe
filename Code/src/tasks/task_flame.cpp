// =====================================================
// task_flame.cpp
// SMART SAFE
// =====================================================

#include <Arduino.h>

#include "config/pins.h"
#include "core/globals.h"
#include "core/events.h"
#include "core/led.h"
#include "core/system_bits.h"

// =====================================
// CONFIG
// =====================================

#define FLAME_ACTIVE_LEVEL LOW

#define PUMP_ON_LEVEL  HIGH
#define PUMP_OFF_LEVEL LOW

#define FIRE_EVENT_DELAY 5000

// chống nhiễu
#define FLAME_STABLE_COUNT 3

// =====================================
// STATE
// =====================================

static bool fireActive = false;

static unsigned long
lastFireEventTime = 0;

static int fireCount = 0;
static int normalCount = 0;

// =====================================
// SEND EVENT
// =====================================

static void sendFireEvent()
{
    SystemEvent event;

    event.type =
        EVENT_FLAME_DETECTED;

    xQueueSend(
        systemQueue,
        &event,
        0
    );
}

// =====================================
// TASK
// =====================================

void taskFlame(void *pv)
{
    pinMode(
        FLAME_PIN,
        INPUT
    );

    pinMode(
        PUMP_RELAY_PIN,
        OUTPUT
    );

    digitalWrite(
        PUMP_RELAY_PIN,
        PUMP_OFF_LEVEL
    );

    Serial.println();
    Serial.println(
        "[FLAME] TASK START"
    );

    while (1)
    {
        int flameValue =
            digitalRead(
                FLAME_PIN
            );

        bool detected =
        (
            flameValue ==
            FLAME_ACTIVE_LEVEL
        );

        // ==========================
        // FILTER
        // ==========================

        if (detected)
        {
            fireCount++;
            normalCount = 0;
        }
        else
        {
            normalCount++;
            fireCount = 0;
        }

        // ==========================
        // FIRE DETECTED
        // ==========================

        if (
            fireCount >=
            FLAME_STABLE_COUNT &&
            !fireActive
        )
        {
            fireActive = true;

            lastFireEventTime =
                millis();

            Serial.println(
                "[FLAME] FIRE DETECTED"
            );

            digitalWrite(
                PUMP_RELAY_PIN,
                PUMP_ON_LEVEL
            );

            xEventGroupSetBits(
                systemEvents,
                BIT_FLAME_ACTIVE |
                BIT_ALARM_ACTIVE
            );

            ledPulse(
                LED_MODE_RED,
                1000
            );

            sendFireEvent();
        }

        // ==========================
        // STILL BURNING
        // ==========================

        if (fireActive)
        {
            digitalWrite(
                PUMP_RELAY_PIN,
                PUMP_ON_LEVEL
            );

            if (
                millis() -
                lastFireEventTime >
                FIRE_EVENT_DELAY
            )
            {
                lastFireEventTime =
                    millis();

                sendFireEvent();
            }
        }

        // ==========================
        // FIRE CLEARED
        // ==========================

        if (
            normalCount >=
            FLAME_STABLE_COUNT &&
            fireActive
        )
        {
            fireActive = false;

            Serial.println(
                "[FLAME] FIRE CLEARED"
            );

            digitalWrite(
                PUMP_RELAY_PIN,
                PUMP_OFF_LEVEL
            );

            xEventGroupClearBits(
                systemEvents,
                BIT_FLAME_ACTIVE
            );

            xEventGroupClearBits(
                systemEvents,
                BIT_ALARM_ACTIVE
            );
            setLEDColor(
                LED_MODE_OFF
            );

            ledPulse(
                LED_MODE_OFF,
                100
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(300)
        );
    }
}