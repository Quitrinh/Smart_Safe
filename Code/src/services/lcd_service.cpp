#include <Arduino.h>

#include "lcd_service.h"

#include "core/globals.h"
#include "core/lcd_message.h"

void showLCD(
    String line1,
    String line2,
    int duration
)
{
    LCDMessage msg;

    line1.toCharArray(
        msg.line1,
        17
    );

    line2.toCharArray(
        msg.line2,
        17
    );

    msg.duration =
        duration;

    xQueueSend(
        lcdQueue,
        &msg,
        0
    );
}