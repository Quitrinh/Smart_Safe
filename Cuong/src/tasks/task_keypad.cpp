// =====================================================
// task_keypad.cpp
// FINAL STABLE VERSION
// =====================================================

#include <Arduino.h>

#include "config/pins.h"

#include "core/globals.h"
#include "core/system_bits.h"
#include "core/buzzer.h"
#include "core/rfid_modes.h"

// =====================================================
// LCD
// =====================================================
extern String lcdLine1;

extern String lcdLine2;

extern unsigned long
lcdMessageTime;


// =====================================================
// PASSWORD
// =====================================================
String passwordInput = "";


// =====================================================
// DEFAULT PASSWORD
// =====================================================
String correctPassword =
    "1234";


// =====================================================
// HOLD PROTECTION
// =====================================================
bool keyPressed = false;


// =====================================================
// TASK KEYPAD
// =====================================================
void taskKeypad(void *pv)
{
    // =============================================
    // INIT
    // =============================================
    pinMode(
        KEYPAD_SCL,
        OUTPUT
    );

    pinMode(
        KEYPAD_SDO,
        INPUT
    );

    digitalWrite(
        KEYPAD_SCL,
        HIGH
    );

    Serial.println(
        "KEYPAD TASK START"
    );

    // =============================================
    // MAIN LOOP
    // =============================================
    while (1)
    {
        // =========================================
        // GET EVENT BITS
        // =========================================
        EventBits_t bits =
            xEventGroupGetBits(
                systemEvents
            );

        // =========================================
        // ADMIN SCREEN
        // =========================================
        // =========================================
// ADMIN SCREEN
// =========================================
if (
    (bits & BIT_ADMIN_MODE) &&
    currentRFIDMode ==
    RFID_MODE_NORMAL
)
{
    if (
        lcdLine1 != "ADMIN MODE"
    )
    {
        lcdLine1 =
            "ADMIN MODE";

        lcdLine2 =
            "1ADD 2DEL 3EXIT";
    }
}

        // =========================================
        // WAIT RFID
        // =========================================
        else if (
            !(bits & BIT_RFID_OK) &&
            !adminMode
        )
        {
            if (
    lcdLine1 != "SAFE LOCKED"
)
{
    lcdLine1 =
        "SAFE LOCKED";

    lcdLine2 =
        "SCAN RFID";
}

            vTaskDelay(
                100 /
                portTICK_PERIOD_MS
            );

            continue;
        }

        // =========================================
        // READ KEYPAD
        // =========================================
        uint8_t keys = 0;

        for (
            int i = 0;
            i < 8;
            i++
        )
        {
            digitalWrite(
                KEYPAD_SCL,
                LOW
            );

            delayMicroseconds(
                100
            );

            int bitValue =
                digitalRead(
                    KEYPAD_SDO
                );

            if (bitValue)
            {
                keys |=
                    (1 << i);
            }

            digitalWrite(
                KEYPAD_SCL,
                HIGH
            );

            delayMicroseconds(
                100
            );
        }

        // =========================================
        // NO KEY
        // =========================================
        if (keys == 0xFF)
        {
            keyPressed = false;

            vTaskDelay(
                30 /
                portTICK_PERIOD_MS
            );

            continue;
        }

        // =========================================
        // HOLD PROTECTION
        // =========================================
        if (keyPressed)
        {
            vTaskDelay(
                30 /
                portTICK_PERIOD_MS
            );

            continue;
        }

        keyPressed = true;

        // =========================================
        // DETECT KEY
        // =========================================
        for (
            int i = 0;
            i < 8;
            i++
        )
        {
            // LOW = PRESSED
            if (
                !(keys & (1 << i))
            )
            {
                int key =
                    i + 1;

                Serial.print(
                    "KEY: "
                );

                Serial.println(
                    key
                );

                buzzerBeep(
                    2200,
                    80
                );

                // =================================
// ADMIN MODE
// =================================
if (adminMode)
{
    // =============================
    // ADD RFID
    // =============================
    if (key == 1)
    {
        currentRFIDMode =
            RFID_MODE_ADD;

        // EXIT ADMIN SCREEN
        xEventGroupClearBits(
            systemEvents,
            BIT_ADMIN_MODE
        );

        Serial.println(
            "ADD RFID MODE"
        );

        break;
    }

    // =============================
    // DELETE RFID
    // =============================
    else if (key == 2)
    {
        currentRFIDMode =
            RFID_MODE_DELETE;

        // EXIT ADMIN SCREEN
        xEventGroupClearBits(
            systemEvents,
            BIT_ADMIN_MODE
        );

        Serial.println(
            "DELETE RFID MODE"
        );

        break;
    }

    // =============================
    // EXIT ADMIN
    // =============================
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

        lcdLine2 =
            "";

        lcdMessageTime =
            millis();

        Serial.println(
            "EXIT ADMIN"
        );

        break;
    }

    continue;
}

                // =================================
                // PASSWORD INPUT
                // =================================
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
                    j <
                    passwordInput.length();
                    j++
                )
                {
                    lcdLine2 += "*";
                }

                lcdMessageTime =
                    millis();

                break;
            }
        }

        // =========================================
        // CHECK PASSWORD
        // =========================================
        if (
            passwordInput.length()
            >= 4
        )
        {
            // =====================================
            // PASSWORD OK
            // =====================================
            if (
                passwordInput ==
                correctPassword
            )
            {
                Serial.println(
                    "PASSWORD OK"
                );

                authenticated =
                    true;

                failedAttempts = 0;

                // AUTH SUCCESS
                xEventGroupSetBits(
                    systemEvents,
                    BIT_AUTH_OK
                );

                // CLEAR ALARM
                xEventGroupClearBits(
                    systemEvents,
                    BIT_ALARM_ACTIVE
                );

                // CLEAR FLOW
                xEventGroupClearBits(
                    systemEvents,
                    BIT_RFID_OK
                );

                xEventGroupClearBits(
                    systemEvents,
                    BIT_FINGER_OK
                );

                // LCD
                lcdLine1 =
                    "ACCESS OK";

                lcdLine2 =
                    "SAFE OPEN";

                lcdMessageTime =
                    millis();

                // BUZZER
                buzzerBeep(
                    3000,
                    200
                );

                Serial.println(
                    "SAFE UNLOCKED"
                );
            }

            // =====================================
            // PASSWORD FAIL
            // =====================================
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

                Serial.print(
                    "FAILED: "
                );

                Serial.println(
                    failedAttempts
                );

                // TOO MANY FAILS
                if (
                    failedAttempts >= 3
                )
                {
                    Serial.println(
                        "ALARM ACTIVE"
                    );

                    xEventGroupSetBits(
                        systemEvents,
                        BIT_ALARM_ACTIVE
                    );
                }
            }

            // =====================================
            // SHOW RESULT
            // =====================================
            vTaskDelay(
                1500 /
                portTICK_PERIOD_MS
            );

            // =====================================
            // RESET
            // =====================================
            passwordInput = "";

            lcdLine1 = "";

            lcdLine2 = "";
        }

        // =========================================
        // TASK DELAY
        // =========================================
        vTaskDelay(
            100 /
            portTICK_PERIOD_MS
        );
    }
}