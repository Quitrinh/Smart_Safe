#include <Arduino.h>

#include "config/pins.h"
#include "core/globals.h"
#include "core/system_bits.h"

#define BUZZER_CHANNEL 7

bool alarmState = false;

void taskAlarm(void *pv) {

    // =========================
    // INIT BUZZER
    // =========================
    ledcSetup(
        BUZZER_CHANNEL,
        2000,
        8
    );

    ledcAttachPin(
        BUZZER_PIN,
        BUZZER_CHANNEL
    );

    Serial.println(
        "ALARM TASK STARTED"
    );

    while (1) {

        EventBits_t bits;

        bits = xEventGroupGetBits(
            systemEvents
        );

        // =========================
        // ALARM ACTIVE
        // =========================
        if (bits & BIT_ALARM_ACTIVE) {

            if (!alarmState) {

                Serial.println(
                    "ALARM ON"
                );

                alarmState = true;
            }

            ledcWriteTone(
                BUZZER_CHANNEL,
                2000
            );
        }

        // =========================
        // ALARM OFF
        // =========================
        else {

            if (alarmState) {

                Serial.println(
                    "ALARM OFF"
                );

                alarmState = false;
            }

            ledcWriteTone(
                BUZZER_CHANNEL,
                0
            );
        }

        vTaskDelay(
            100 / portTICK_PERIOD_MS
        );
    }
}