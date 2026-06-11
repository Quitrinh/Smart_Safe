#pragma once

#include <Arduino.h>

// LED màu dùng cho xác thực và cảnh báo
#define LED_ON HIGH
#define LED_OFF LOW

typedef enum {
    LED_MODE_OFF,
    LED_MODE_WHITE,
    LED_MODE_RED,
    LED_MODE_GREEN,
    LED_MODE_BLUE,
    LED_MODE_ALARM_FLASH
} LEDMode;

void initLEDs();
void setLEDColor(LEDMode mode);
void ledPulse(LEDMode mode, unsigned long duration);
void taskLED(void *pv);
