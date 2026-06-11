#include <Arduino.h>

#include "alert_service.h"
#include "telegram_service.h"

unsigned long lastAlert = 0;

void triggerAlarm(String reason)
{
    // ANTI SPAM
    if (
        millis() - lastAlert < 15000
    )
    {
        return;
    }

    lastAlert = millis();

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "TRIGGER ALARM"
    );

    Serial.println(reason);

    // SEND TELEGRAM
    sendTelegram(
        "SAFE ALERT: " + reason
    );
}