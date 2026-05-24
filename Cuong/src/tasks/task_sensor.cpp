// =====================================================
// task_sensor.cpp
// FINAL STABLE VERSION
// =====================================================

#include <Arduino.h>

#include "config/pins.h"

#include "core/globals.h"
#include "core/system_bits.h"
#include "core/events.h"
#include "core/safe_state.h"


// =====================================================
// TASK SENSOR
// =====================================================
void taskSensor(void *pv)
{
    // =================================================
    // INIT SENSOR
    // =================================================
    pinMode(
        VIBRATION_PIN,
        INPUT_PULLUP
    );

    pinMode(
        DOOR_PIN,
        INPUT_PULLUP
    );

    // =================================================
    // SW420 VARIABLES
    // =================================================
    int lastVibrationState =
        HIGH;

    int vibrationCount = 0;

    unsigned long
    firstTriggerTime = 0;

    unsigned long
    lastVibrationAlert = 0;

    // =================================================
    // DOOR VARIABLES
    // =================================================
    int lastDoorState =
        LOW;

    unsigned long
    lastDoorEvent = 0;

    bool unauthorizedSent =
        false;

    bool lockSent =
        false;

    // =================================================
    // EVENT OBJECT
    // =================================================
    SystemEvent event;

    Serial.println(
        "SENSOR TASK STARTED"
    );

    // =================================================
    // MAIN LOOP
    // =================================================
    while (1)
    {
        // =============================================
        // VIBRATION SENSOR
        // =============================================
        int currentVibrationState =
            digitalRead(
                VIBRATION_PIN
            );

        // falling edge
        if (
            lastVibrationState == HIGH &&
            currentVibrationState == LOW
        )
        {
            // first trigger
            if (vibrationCount == 0)
            {
                firstTriggerTime =
                    millis();
            }

            vibrationCount++;

            Serial.print(
                "Vibration Count = "
            );

            Serial.println(
                vibrationCount
            );

            // debounce
            vTaskDelay(
                20 /
                portTICK_PERIOD_MS
            );
        }

        lastVibrationState =
            currentVibrationState;

        // =============================================
        // VIBRATION ALERT
        // =============================================
        if (
            vibrationCount >= 12 &&
            millis() - firstTriggerTime
            < 2000
        )
        {
            // anti spam
            if (
                millis() - lastVibrationAlert
                > 10000
            )
            {
                Serial.println();

                Serial.println(
                    "!!! VIBRATION ALERT !!!"
                );

                // =====================================
                // SAFE STATE
                // =====================================
                safeState =
                    SAFE_ALARM;

                // =====================================
                // EVENT
                // =====================================
                event.type =
                    EVENT_VIBRATION;

                strcpy(
                    event.message,
                    "SAFE_ALERT_VIBRATION"
                );

                // =====================================
                // SEND QUEUE
                // =====================================
                xQueueSend(
                    systemQueue,
                    &event,
                    0
                );

                // =====================================
                // ALARM ON
                // =====================================
                xEventGroupSetBits(
                    systemEvents,
                    BIT_ALARM_ACTIVE
                );

                lastVibrationAlert =
                    millis();
            }

            vibrationCount = 0;
        }

        // =============================================
        // RESET COUNTER
        // =============================================
        if (
            millis() - firstTriggerTime
            > 2000
        )
        {
            vibrationCount = 0;
        }

        // =============================================
        // DOOR SENSOR
        // =============================================
        int currentDoorState =
            digitalRead(
                DOOR_PIN
            );

        // =============================================
        // DOOR STATE CHANGE
        // =============================================
        if (
            currentDoorState !=
            lastDoorState
        )
        {
            // debounce
            vTaskDelay(
                80 /
                portTICK_PERIOD_MS
            );

            currentDoorState =
                digitalRead(
                    DOOR_PIN
                );

            // double check
            if (
                currentDoorState !=
                lastDoorState
            )
            {
                EventBits_t bits =
                    xEventGroupGetBits(
                        systemEvents
                    );

                // =====================================
                // DOOR OPEN
                // =====================================
                if (
                    currentDoorState == HIGH
                )
                {
                    Serial.println();

                    Serial.println(
                        "DOOR OPEN"
                    );

                    xEventGroupSetBits(
                        systemEvents,
                        BIT_DOOR_OPEN
                    );

                    lockSent = false;

                    // =================================
                    // AUTHORIZED
                    // =================================
                    if (
                        bits & BIT_AUTH_OK
                    )
                    {
                        Serial.println(
                            "AUTHORIZED ACCESS"
                        );

                        safeState =
                            SAFE_OPEN;

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
                    }

                    // =================================
                    // UNAUTHORIZED
                    // =================================
                    else
                    {
                        // anti spam
                        if (
                            !unauthorizedSent &&
                            millis() - lastDoorEvent
                            > 10000
                        )
                        {
                            Serial.println(
                                "!!! UNAUTHORIZED ACCESS !!!"
                            );

                            unauthorizedSent =
                                true;

                            lastDoorEvent =
                                millis();

                            safeState =
                                SAFE_ALARM;

                            event.type =
                                EVENT_UNAUTHORIZED;

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
                                BIT_ALARM_ACTIVE
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

                    Serial.println(
                        "DOOR CLOSED"
                    );

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_DOOR_OPEN
                    );

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_AUTH_OK
                    );

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_ALARM_ACTIVE
                    );

                    unauthorizedSent =
                        false;

                    // anti spam
                    if (
                        !lockSent
                    )
                    {
                        lockSent = true;

                        safeState =
                            SAFE_LOCKED;

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
                }

                lastDoorState =
                    currentDoorState;
            }
        }

        // =============================================
        // TASK DELAY
        // =============================================
        vTaskDelay(
            20 /
            portTICK_PERIOD_MS
        );
    }
}