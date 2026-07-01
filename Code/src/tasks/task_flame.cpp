// =====================================================
// task_flame.cpp
// SMART SAFE
// =====================================================

#include <Arduino.h>

#include "config/pins.h"
#include "core/globals.h"
#include "core/events.h"
#include "core/system_bits.h"

// =====================================
// CONFIG FROM BACKEND
// =====================================
// Biến này được cập nhật từ /api/esp32/config
extern bool flameAlertEnabled;

// =====================================
// FLAME SENSOR CONFIG
// =====================================
// Đa số module cảm biến lửa digital:
// LOW  = phát hiện lửa
// HIGH = bình thường
#define FLAME_ACTIVE_LEVEL LOW

// Relay bơm
#define PUMP_ON_LEVEL  HIGH
#define PUMP_OFF_LEVEL LOW

// Gửi lại event mỗi 5 giây nếu vẫn còn lửa
#define FIRE_EVENT_DELAY 5000

// Chống nhiễu: cần phát hiện liên tục 3 lần mới tính là cháy
#define FLAME_STABLE_COUNT 3

// Chu kỳ đọc cảm biến
#define FLAME_READ_DELAY_MS 300

// =====================================
// STATE
// =====================================
static bool fireActive = false;
static unsigned long lastFireEventTime = 0;

static int fireCount = 0;
static int normalCount = 0;

// =====================================
// SEND EVENT
// =====================================
static void sendFireEvent()
{
    SystemEvent event;
    event.type = EVENT_FLAME_DETECTED;

    xQueueSend(systemQueue, &event, 0);
}

// =====================================
// RESET FLAME STATE
// =====================================
static void resetFlameState()
{
    fireActive = false;
    fireCount = 0;
    normalCount = 0;
    lastFireEventTime = 0;

    digitalWrite(PUMP_RELAY_PIN, PUMP_OFF_LEVEL);

    xEventGroupClearBits(
        systemEvents,
        BIT_FLAME_ACTIVE
    );

    // Nếu hệ thống của bạn có nhiều loại cảnh báo cùng dùng BIT_ALARM_ACTIVE,
    // cần cẩn thận khi clear bit này.
    // Hiện tại giữ giống logic cũ.
    xEventGroupClearBits(
        systemEvents,
        BIT_ALARM_ACTIVE
    );
}

// =====================================
// TASK
// =====================================
void taskFlame(void *pv)
{
    pinMode(FLAME_PIN, INPUT);

    pinMode(PUMP_RELAY_PIN, OUTPUT);
    digitalWrite(PUMP_RELAY_PIN, PUMP_OFF_LEVEL);

    Serial.println();
    Serial.println("[FLAME] TASK START");

    while (1)
    {
        // =====================================
        // Nếu admin tắt cảnh báo lửa trong app
        // =====================================
        if (!flameAlertEnabled)
        {
            if (fireActive)
            {
                Serial.println("[FLAME] DISABLED BY CONFIG -> RESET");
            }

            resetFlameState();

            vTaskDelay(pdMS_TO_TICKS(FLAME_READ_DELAY_MS));
            continue;
        }

        // =====================================
        // READ SENSOR
        // =====================================
        int flameValue = digitalRead(FLAME_PIN);

        bool detected = (flameValue == FLAME_ACTIVE_LEVEL);

        // Debug nếu cần
        // Serial.print("[FLAME] VALUE = ");
        // Serial.println(flameValue);

        // =====================================
        // FILTER chống nhiễu
        // =====================================
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

        // =====================================
        // FIRE DETECTED
        // =====================================
        if (
            fireCount >= FLAME_STABLE_COUNT &&
            !fireActive
        )
        {
            fireActive = true;
            lastFireEventTime = millis();

            Serial.println("[FLAME] FIRE DETECTED");

            digitalWrite(PUMP_RELAY_PIN, PUMP_ON_LEVEL);

            xEventGroupSetBits(
                systemEvents,
                BIT_FLAME_ACTIVE | BIT_ALARM_ACTIVE
            );

            sendFireEvent();
        }

        // =====================================
        // STILL BURNING
        // =====================================
        if (fireActive)
        {
            digitalWrite(PUMP_RELAY_PIN, PUMP_ON_LEVEL);

            if (
                millis() - lastFireEventTime >
                FIRE_EVENT_DELAY
            )
            {
                lastFireEventTime = millis();

                Serial.println("[FLAME] FIRE STILL ACTIVE -> SEND EVENT");

                sendFireEvent();
            }
        }

        // =====================================
        // FIRE CLEARED
        // =====================================
        if (
            normalCount >= FLAME_STABLE_COUNT &&
            fireActive
        )
        {
            Serial.println("[FLAME] FIRE CLEARED");

            resetFlameState();
        }

        vTaskDelay(pdMS_TO_TICKS(FLAME_READ_DELAY_MS));
    }
}