// =====================================================
// task_keypad.cpp
// CLEAN VERSION
// RFID -> FINGER -> PASSWORD
// =====================================================

#include <Arduino.h>

#include "config/pins.h"

#include "core/globals.h"
#include "core/system_bits.h"
#include "core/buzzer.h"
#include "core/led.h"
#include "core/rfid_modes.h"

// =====================================================
// LCD
// =====================================================
extern String lcdLine1;
extern String lcdLine2;
extern unsigned long lcdMessageTime;

// =====================================================
// PASSWORD
// =====================================================
String passwordInput = "";

// =====================================================
// DEFAULT PASSWORD
// =====================================================
String correctPassword = "1234";

// =====================================================
// HOLD PROTECTION
// =====================================================
bool keyPressed = false;

// =====================================================
// TASK KEYPAD
// =====================================================
void taskKeypad(void *pv)
{
pinMode(KEYPAD_SCL, OUTPUT);
pinMode(KEYPAD_SDO, INPUT);

digitalWrite(KEYPAD_SCL, HIGH);

Serial.println("KEYPAD TASK START");

while (1)
{
    EventBits_t bits =
        xEventGroupGetBits(systemEvents);

    // =========================================
    // ADMIN SCREEN
    // =========================================
    if (
        (bits & BIT_ADMIN_MODE) &&
        currentRFIDMode == RFID_MODE_NORMAL
    )
    {
        lcdLine1 = "ADMIN MODE";
        lcdLine2 = "1ADD 2DEL 3EXIT";
    }

    // =========================================
    // WAIT RFID
    // =========================================
    else if (
        !(bits & BIT_RFID_OK) &&
        !adminMode
    )
    {
        lcdLine1 = "SAFE LOCKED";
        lcdLine2 = "SCAN RFID";

        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
    }

    // =========================================
    // READ KEYPAD
    // =========================================
    uint8_t keys = 0;

    for (int i = 0; i < 8; i++)
    {
        digitalWrite(KEYPAD_SCL, LOW);
        delayMicroseconds(100);

        int bitValue =
            digitalRead(KEYPAD_SDO);

        if (bitValue)
        {
            keys |= (1 << i);
        }

        digitalWrite(KEYPAD_SCL, HIGH);
        delayMicroseconds(100);
    }

    // =========================================
    // NO KEY
    // =========================================
    if (keys == 0xFF)
    {
        keyPressed = false;

        vTaskDelay(pdMS_TO_TICKS(30));
        continue;
    }

    // =========================================
    // HOLD PROTECTION
    // =========================================
    if (keyPressed)
    {
        vTaskDelay(pdMS_TO_TICKS(30));
        continue;
    }

    keyPressed = true;

    // =========================================
    // DETECT KEY
    // =========================================
    for (int i = 0; i < 8; i++)
    {
        if (!(keys & (1 << i)))
        {
            int key = i + 1;

            Serial.print("KEY: ");
            Serial.println(key);

            buzzerBeep(2200, 80);

            // =====================================
            // ADMIN MODE
            // =====================================
            if (adminMode)
            {
                if (key == 1)
                {
                    currentRFIDMode =
                        RFID_MODE_ADD;

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_ADMIN_MODE
                    );

                    Serial.println(
                        "ADD RFID MODE"
                    );
                }
                else if (key == 2)
                {
                    currentRFIDMode =
                        RFID_MODE_DELETE;

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_ADMIN_MODE
                    );

                    Serial.println(
                        "DELETE RFID MODE"
                    );
                }
                else if (key == 3)
                {
                    adminMode = false;

                    currentRFIDMode =
                        RFID_MODE_NORMAL;

                    xEventGroupClearBits(
                        systemEvents,
                        BIT_ADMIN_MODE
                    );

                    lcdLine1 =
                        "EXIT ADMIN";

                    lcdLine2 = "";

                    lcdMessageTime =
                        millis();

                    Serial.println(
                        "EXIT ADMIN"
                    );
                }

                break;
            }

            // =====================================
            // PASSWORD INPUT
            // RFID + FINGER REQUIRED
            // =====================================
            if (
                rfidAuthenticated &&
                fingerAuthenticated
            )
            {
                passwordInput +=
                    String(key);

                Serial.print(
                    "PASSWORD: "
                );

                Serial.println(
                    passwordInput
                );

                lcdLine1 =
                    "ENTER PASS";

                lcdLine2 = "";

                for (
                    int j = 0;
                    j < passwordInput.length();
                    j++
                )
                {
                    lcdLine2 += "*";
                }

                lcdMessageTime =
                    millis();
            }
            else
            {
                buzzerBeep(
                    1000,
                    100
                );
            }

            break;
        }
    }

    // =========================================
    // CHECK PASSWORD
    // =========================================
    if (
        rfidAuthenticated &&
        fingerAuthenticated &&
        passwordInput.length() >= 4
    )
    {
        if (
            passwordInput ==
            correctPassword
        )
        {
            Serial.println(
                "PASSWORD OK"
            );

            authenticated = true;
            failedAttempts = 0;

            xEventGroupSetBits(
                systemEvents,
                BIT_AUTH_OK |
                BIT_KEYPAD_OK
            );

            xEventGroupClearBits(
                systemEvents,
                BIT_ALARM_ACTIVE
            );

            lcdLine1 =
                "ACCESS OK";

            lcdLine2 =
                "SAFE OPEN";

            lcdMessageTime =
                millis();

            buzzerBeep(
                3000,
                200
            );

            ledPulse(
                LED_MODE_GREEN,
                400
            );

            Serial.println(
                "SAFE UNLOCKED"
            );

            // reset session
            passwordInput = "";

            rfidAuthenticated =
                false;

            fingerAuthenticated =
                false;
        }
        else
        {
            Serial.println(
                "PASSWORD FAIL"
            );

            authenticated =
                false;

            failedAttempts++;

            xEventGroupClearBits(
                systemEvents,
                BIT_AUTH_OK
            );

            lcdLine1 =
                "WRONG PASS";

            lcdLine2 =
                "TRY AGAIN";

            lcdMessageTime =
                millis();

            buzzerBeep(
                1000,
                500
            );

            ledPulse(
                LED_MODE_RED,
                500
            );

            if (
                failedAttempts >= 3
            )
            {
                Serial.println(
                    "ALARM ACTIVE"
                );

                xEventGroupSetBits(
                    systemEvents,
                    BIT_ALARM_ACTIVE |
                    BIT_NEED_GPS |
                    BIT_TRACKING_MODE
                );
            }

            passwordInput = "";
        }
    }

    vTaskDelay(
        pdMS_TO_TICKS(30)
    );
}
}
