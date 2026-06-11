#include <Arduino.h>

#include "config/pins.h"

#define BUZZER_CHANNEL 7

void buzzerBeep(
    int frequency,
    int duration
) {

    ledcWriteTone(
        BUZZER_CHANNEL,
        frequency
    );

    vTaskDelay(
        duration / portTICK_PERIOD_MS
    );

    ledcWriteTone(
        BUZZER_CHANNEL,
        0
    );
}