#include <Arduino.h>

#include "core/globals.h"
#include "core/events.h"

#include "services/telegram_service.h"


void taskTelegram(void *pv)
{
    SystemEvent event;

    while (1)
    {
        if (
            xQueueReceive(
                telegramQueue,
                &event,
                portMAX_DELAY
            )
        )
        {
            String msg = "";

            switch(event.type)
            {
                case EVENT_UNLOCK:

                    msg =
                    "🔓 SAFE OPENED";

                    break;

                case EVENT_LOCK:

                    msg =
                    "🔒 SAFE LOCKED";

                    break;

                case EVENT_UNAUTHORIZED:

                    msg =
                    "🚨 UNAUTHORIZED ACCESS";

                    break;

                default:
                    continue;
            }

            sendTelegram(msg);

            // IMPORTANT
            // avoid SIM overload
            vTaskDelay(
                1000 /
                portTICK_PERIOD_MS
            );
        }
    }
}