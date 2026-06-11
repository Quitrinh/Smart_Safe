// =====================================================
// task_sensor.cpp
// SMART SAFE - SENSOR VERSION
// =====================================================

#include <Arduino.h>

#include "config/pins.h"
#include "core/globals.h"
#include "core/system_bits.h"
#include "core/events.h"
#include "core/safe_state.h"

// =====================================================
// CONFIG
// =====================================================
#define VIBRATION_TRIGGER_COUNT 12
#define VIBRATION_TIME_WINDOW   2000
#define VIBRATION_ALARM_TIME    5000
#define VIBRATION_SPAM_DELAY    10000

// =====================================================
// TASK SENSOR
// =====================================================
void taskSensor(void *pv)
{
    pinMode(VIBRATION_PIN, INPUT_PULLUP);
    pinMode(DOOR_PIN, INPUT_PULLUP);

    // =========================
    // VIBRATION
    // =========================
    int lastVibrationState = HIGH;
    int vibrationCount = 0;

    bool vibrationActive = false;

    unsigned long firstTriggerTime = 0;
    unsigned long lastVibrationAlert = 0;
    unsigned long vibrationAlarmStart = 0;

    // =========================
    // DOOR
    // =========================
    int lastDoorState = digitalRead(DOOR_PIN);

    unsigned long lastDoorEvent = 0;

    bool unauthorizedSent = false;
    bool lockSent = false;

    SystemEvent event;

    Serial.println("SENSOR TASK STARTED");

    while (1)
    {
        // =================================================
        // VIBRATION SENSOR
        // =================================================
        int currentVibrationState = digitalRead(VIBRATION_PIN);

        // SW420 thường LOW khi rung
        if (
            lastVibrationState == HIGH &&
            currentVibrationState == LOW
        )
        {
            if (vibrationCount == 0)
            {
                firstTriggerTime = millis();
            }

            vibrationCount++;

            Serial.print("Vibration Count = ");
            Serial.println(vibrationCount);

            vTaskDelay(pdMS_TO_TICKS(20));
        }

        lastVibrationState = currentVibrationState;

        // =========================
        // VIBRATION ALERT
        // =========================
        if (
            vibrationCount >= VIBRATION_TRIGGER_COUNT &&
            millis() - firstTriggerTime < VIBRATION_TIME_WINDOW
        )
        {
            if (
                millis() - lastVibrationAlert > VIBRATION_SPAM_DELAY
            )
            {
                Serial.println();
                Serial.println("!!! VIBRATION ALERT !!!");

                vibrationActive = true;
                vibrationAlarmStart = millis();
                lastVibrationAlert = millis();

                safeState = SAFE_ALARM;

                event.type = EVENT_VIBRATION;

                strcpy(
                    event.message,
                    "SAFE_ALERT_VIBRATION"
                );

                xQueueSend(
                    systemQueue,
                    &event,
                    0
                );

                xEventGroupSetBits(
                    systemEvents,
                    BIT_ALARM_ACTIVE |
                    BIT_NEED_GPS |
                    BIT_TRACKING_MODE
                );
            }

            vibrationCount = 0;
        }

        // =========================
        // RESET VIBRATION COUNTER
        // =========================
        if (
            millis() - firstTriggerTime > VIBRATION_TIME_WINDOW
        )
        {
            vibrationCount = 0;
        }

        // =========================
        // AUTO CLEAR VIBRATION ALARM
        // =========================
        if (
            vibrationActive &&
            millis() - vibrationAlarmStart > VIBRATION_ALARM_TIME
        )
        {
            EventBits_t bits =
                xEventGroupGetBits(systemEvents);

            // Chỉ tắt alarm rung nếu cửa không mở và không cháy
            if (
                !(bits & BIT_DOOR_OPEN) &&
                !(bits & BIT_FLAME_ACTIVE)
            )
            {
                Serial.println("[VIBRATION] ALARM CLEARED");

                vibrationActive = false;

                xEventGroupClearBits(
                    systemEvents,
                    BIT_ALARM_ACTIVE
                );

                if (safeState == SAFE_ALARM)
                {
                    safeState = SAFE_LOCKED;
                }
            }
        }

        // =================================================
        // DOOR SENSOR
        // =================================================
        int currentDoorState = digitalRead(DOOR_PIN);

        if (currentDoorState != lastDoorState)
        {
            vTaskDelay(pdMS_TO_TICKS(80));

            currentDoorState = digitalRead(DOOR_PIN);

            if (currentDoorState != lastDoorState)
            {
                EventBits_t bits =
                    xEventGroupGetBits(systemEvents);

                // =====================================
                // DOOR OPEN
                // =====================================
                if (currentDoorState == HIGH)
                {
                    Serial.println();
                    Serial.println("DOOR OPEN");

                    xEventGroupSetBits(
                        systemEvents,
                        BIT_DOOR_OPEN
                    );

                    lockSent = false;

                    if (bits & BIT_AUTH_OK)
                    {
                        Serial.println("AUTHORIZED ACCESS");

                        safeState = SAFE_OPEN;

                        event.type = EVENT_UNLOCK;

                        strcpy(
                            event.message,
                            "SAFE_OPENED"
                        );

                        xQueueSend(
                            systemQueue,
                            &event,
                            0
                        );
                    }
                    else
                    {
                        if (
                            !unauthorizedSent &&
                            millis() - lastDoorEvent > 10000
                        )
                        {
                            Serial.println("!!! UNAUTHORIZED ACCESS !!!");

                            unauthorizedSent = true;
                            lastDoorEvent = millis();

                            safeState = SAFE_ALARM;

                            event.type = EVENT_UNAUTHORIZED;

                            strcpy(
                                event.message,
                                "SAFE_ALERT_UNAUTHORIZED"
                            );

                            xQueueSend(
                                systemQueue,
                                &event,
                                0
                            );

                            xEventGroupSetBits(
                                systemEvents,
                                BIT_ALARM_ACTIVE |
                                BIT_NEED_GPS |
                                BIT_TRACKING_MODE
                            );
                        }
                    }
                }

                // =====================================
                // DOOR CLOSED
                // =====================================
                else
                {
                    Serial.println();
                    Serial.println("DOOR CLOSED");

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_DOOR_OPEN |
                        BIT_AUTH_OK |
                        BIT_RFID_OK |
                        BIT_FINGER_OK
                    );

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_ALARM_ACTIVE
                    );

                    unauthorizedSent = false;
                    vibrationActive = false;

                    if (!lockSent)
                    {
                        lockSent = true;

                        safeState = SAFE_LOCKED;

                        event.type = EVENT_LOCK;

                        strcpy(
                            event.message,
                            "SAFE_LOCKED"
                        );

                        xQueueSend(
                            systemQueue,
                            &event,
                            0
                        );

                        Serial.println("SAFE LOCKED");
                    }
                }

                lastDoorState = currentDoorState;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}