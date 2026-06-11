#include <Arduino.h>

#include "config/pins.h"
#include "core/globals.h"
#include "core/led.h"
#include "core/system_bits.h"

static LEDMode currentBaseMode = LED_MODE_OFF;
static LEDMode temporaryMode = LED_MODE_OFF;

static unsigned long temporaryUntil = 0;
static bool temporaryActive = false;

static bool alarmBlinkOn = false;
static unsigned long alarmBlinkTime = 0;

static void writeColor(LEDMode mode)
{
    bool red = false;
    bool green = false;

    switch (mode)
    {
        case LED_MODE_RED:
            red = true;
            break;

        case LED_MODE_GREEN:
            green = true;
            break;

        case LED_MODE_ALARM_FLASH:
            red = true;
            break;

        case LED_MODE_OFF:
        default:
            break;
    }

    digitalWrite(LED_RED_PIN, red ? LED_ON : LED_OFF);
    digitalWrite(LED_GREEN_PIN, green ? LED_ON : LED_OFF);
}

void initLEDs()
{
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);

    currentBaseMode = LED_MODE_OFF;
    temporaryMode = LED_MODE_OFF;
    temporaryUntil = 0;
    temporaryActive = false;
    alarmBlinkOn = false;
    alarmBlinkTime = 0;

    writeColor(LED_MODE_OFF);
}

void setLEDColor(LEDMode mode)
{
    currentBaseMode = mode;

    if (!temporaryActive)
    {
        writeColor(mode);
    }
}

void ledPulse(LEDMode mode, unsigned long duration)
{
    temporaryMode = mode;
    temporaryUntil = millis() + duration;
    temporaryActive = true;

    writeColor(mode);
}

void taskLED(void *pv)
{
    initLEDs();

    while (1)
    {
        EventBits_t bits = xEventGroupGetBits(systemEvents);
        unsigned long now = millis();

        if ((bits & BIT_ALARM_ACTIVE) || safeState == SAFE_ALARM)
        {
            if (now - alarmBlinkTime > 250)
            {
                alarmBlinkTime = now;
                alarmBlinkOn = !alarmBlinkOn;

                writeColor(
                    alarmBlinkOn
                    ? LED_MODE_RED
                    : LED_MODE_OFF
                );
            }
        }
        else
        {
            if (temporaryActive)
            {
                if (now >= temporaryUntil)
                {
                    temporaryActive = false;
                    temporaryMode = LED_MODE_OFF;
                    writeColor(currentBaseMode);
                }
            }
            else
            {
                writeColor(currentBaseMode);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}