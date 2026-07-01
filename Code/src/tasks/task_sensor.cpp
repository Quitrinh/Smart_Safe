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
// CONFIG FROM BACKEND
// =====================================================
extern bool alertVibrationEnabled;
extern bool alertDoorEnabled;

// =====================================================
// VIBRATION CONFIG
// =====================================================
// Ngưỡng rung: trước là 12, giờ chỉnh còn 4
#define VIBRATION_TRIGGER_COUNT 4

// Trong vòng 2 giây nếu rung đủ 4 lần thì cảnh báo
#define VIBRATION_TIME_WINDOW   2000

// Sau 5 giây tự clear cảnh báo rung nếu không còn alarm khác
#define VIBRATION_ALARM_TIME    5000

// Chống spam event rung
#define VIBRATION_SPAM_DELAY    10000

// SW420 thường LOW khi rung
#define VIBRATION_ACTIVE_LEVEL  LOW

// =====================================================
// DOOR CONFIG
// =====================================================
// MC-38 tuỳ cách đấu dây.
// Code cũ của bạn đang hiểu HIGH = cửa mở.
#define DOOR_OPEN_LEVEL HIGH

// Chống dội cửa
#define DOOR_DEBOUNCE_MS 80

// Chống spam event cửa mở trái phép
#define DOOR_EVENT_DELAY 10000

// =====================================================
// HELPER
// =====================================================
static void clearVibrationAlarmIfSafe(bool vibrationActive)
{
    EventBits_t bits = xEventGroupGetBits(systemEvents);

    // Chỉ tắt alarm nếu không còn cửa mở và không cháy
    if (
        !vibrationActive &&
        !(bits & BIT_DOOR_OPEN) &&
        !(bits & BIT_FLAME_ACTIVE)
    )
    {
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

// =====================================================
// TASK SENSOR
// =====================================================
void taskSensor(void *pv)
{
    pinMode(VIBRATION_PIN, INPUT_PULLUP);
    pinMode(DOOR_PIN, INPUT_PULLUP);

    // =========================
    // VIBRATION STATE
    // =========================
    int lastVibrationState = HIGH;
    int vibrationCount = 0;

    bool vibrationActive = false;

    unsigned long firstTriggerTime = 0;
    unsigned long lastVibrationAlert = 0;
    unsigned long vibrationAlarmStart = 0;

    // =========================
    // DOOR STATE
    // =========================
    int lastDoorState = digitalRead(DOOR_PIN);

    unsigned long lastDoorEvent = 0;

    bool unauthorizedSent = false;
    bool lockSent = false;

    SystemEvent event;

    Serial.println();
    Serial.println("[SENSOR] TASK STARTED");
    Serial.print("[SENSOR] VIBRATION_TRIGGER_COUNT = ");
    Serial.println(VIBRATION_TRIGGER_COUNT);

    while (1)
    {
        // =================================================
        // VIBRATION SENSOR
        // =================================================

        if (alertVibrationEnabled)
        {
            int currentVibrationState = digitalRead(VIBRATION_PIN);

            // Phát hiện cạnh HIGH -> LOW
            if (
                lastVibrationState == HIGH &&
                currentVibrationState == VIBRATION_ACTIVE_LEVEL
            )
            {
                if (vibrationCount == 0)
                {
                    firstTriggerTime = millis();
                }

                vibrationCount++;

                Serial.print("[VIBRATION] Count = ");
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
                vibrationCount > 0 &&
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
                Serial.println("[VIBRATION] ALARM CLEARED");

                vibrationActive = false;

                clearVibrationAlarmIfSafe(vibrationActive);
            }
        }
        else
        {
            // Nếu admin tắt cảnh báo rung trên app
            if (vibrationActive)
            {
                Serial.println("[VIBRATION] DISABLED BY CONFIG -> CLEAR");
            }

            vibrationCount = 0;
            vibrationActive = false;
            firstTriggerTime = 0;

            clearVibrationAlarmIfSafe(vibrationActive);
        }

        // =================================================
        // DOOR SENSOR
        // =================================================
        int currentDoorState = digitalRead(DOOR_PIN);

        if (currentDoorState != lastDoorState)
        {
            vTaskDelay(pdMS_TO_TICKS(DOOR_DEBOUNCE_MS));

            currentDoorState = digitalRead(DOOR_PIN);

            if (currentDoorState != lastDoorState)
            {
                EventBits_t bits = xEventGroupGetBits(systemEvents);

                // =====================================
                // DOOR OPEN
                // =====================================
                if (currentDoorState == DOOR_OPEN_LEVEL)
                {
                    Serial.println();
                    Serial.println("[DOOR] OPEN");

                    xEventGroupSetBits(
                        systemEvents,
                        BIT_DOOR_OPEN
                    );

                    lockSent = false;

                    if (bits & BIT_AUTH_OK)
                    {
                        Serial.println("[DOOR] AUTHORIZED ACCESS");

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
                        // Nếu admin bật cảnh báo cửa thì mới báo unauthorized
                        if (alertDoorEnabled)
                        {
                            if (
                                !unauthorizedSent &&
                                millis() - lastDoorEvent > DOOR_EVENT_DELAY
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
                        else
                        {
                            Serial.println("[DOOR] UNAUTHORIZED IGNORED BY CONFIG");
                        }
                    }
                }

                // =====================================
                // DOOR CLOSED
                // =====================================
                else
                {
                    Serial.println();
                    Serial.println("[DOOR] CLOSED");

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_DOOR_OPEN |
                        BIT_AUTH_OK |
                        BIT_RFID_OK |
                        BIT_FINGER_OK
                    );

                    unauthorizedSent = false;

                    // Nếu không còn cháy và không còn rung thì mới tắt alarm
                    EventBits_t newBits = xEventGroupGetBits(systemEvents);

                    if (
                        !vibrationActive &&
                        !(newBits & BIT_FLAME_ACTIVE)
                    )
                    {
                        xEventGroupClearBits(
                            systemEvents,
                            BIT_ALARM_ACTIVE
                        );
                    }

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

                        Serial.println("[DOOR] SAFE LOCKED");
                    }
                }

                lastDoorState = currentDoorState;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}