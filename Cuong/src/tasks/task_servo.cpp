// =====================================================
// task_servo.cpp
// SMART SAFE VERSION
// =====================================================

#include <Arduino.h>

#include <ESP32Servo.h>

#include "config/pins.h"

#include "core/globals.h"
#include "core/system_bits.h"
#include "core/events.h"
#include "core/safe_state.h"

// =====================================================
// SERVO
// =====================================================
Servo lockServo;


// =====================================================
// ANGLE
// =====================================================
#define LOCK_ANGLE     0
#define UNLOCK_ANGLE   90


// =====================================================
// STATE
// =====================================================
bool unlocked = false;


// =====================================================
// TASK SERVO
// =====================================================
void taskServo(void *pv)
{
    // =============================================
    // CONFIG SERVO
    // =============================================
    lockServo.setPeriodHertz(50);

    lockServo.attach(
        SERVO_PIN,
        500,
        2400
    );

    // =============================================
    // DEFAULT LOCK
    // =============================================
    lockServo.write(
        LOCK_ANGLE
    );

    Serial.println(
        "SERVO TASK STARTED"
    );

    // =============================================
    // EVENT OBJECT
    // =============================================
    SystemEvent event;

    // =============================================
    // MAIN LOOP
    // =============================================
    while (1)
    {
        EventBits_t bits =
            xEventGroupGetBits(
                systemEvents
            );

        // =========================================
        // UNLOCK REQUEST
        // =========================================
        if (
            (bits & BIT_AUTH_OK) &&
            !unlocked
        )
        {
            Serial.println();

            Serial.println(
                "SERVO UNLOCK"
            );

            // =====================================
            // OPEN SERVO
            // =====================================
            lockServo.write(
                UNLOCK_ANGLE
            );

            unlocked = true;

            // =====================================
            // SAFE STATE
            // =====================================
            safeState =
                SAFE_OPEN;

            // =====================================
            // SEND EVENT
            // =====================================
            event.type =
                EVENT_UNLOCK;

            strcpy(
                event.message,
                "SAFE_OPENED"
            );

            xQueueSend(
                systemQueue,
                &event,
                0
            );

            Serial.println(
                "SAFE OPEN"
            );
        }

        // =========================================
        // LOCK REQUEST
        // =========================================
        else if (
            !(bits & BIT_AUTH_OK) &&
            unlocked
        )
        {
            Serial.println();

            Serial.println(
                "SERVO LOCK"
            );

            // =====================================
            // CLOSE SERVO
            // =====================================
            lockServo.write(
                LOCK_ANGLE
            );

            unlocked = false;

            // =====================================
            // SAFE STATE
            // =====================================
            safeState =
                SAFE_LOCKED;

            // =====================================
            // SEND EVENT
            // =====================================
            event.type =
                EVENT_LOCK;

            strcpy(
                event.message,
                "SAFE_LOCKED"
            );

            xQueueSend(
                systemQueue,
                &event,
                0
            );

            Serial.println(
                "SAFE LOCKED"
            );
        }

        // =========================================
        // ALARM STATE
        // =========================================
        if (
            bits & BIT_ALARM_ACTIVE
        )
        {
            safeState =
                SAFE_ALARM;
        }

        // =========================================
        // TASK DELAY
        // =========================================
        vTaskDelay(
            100 / portTICK_PERIOD_MS
        );
    }
}