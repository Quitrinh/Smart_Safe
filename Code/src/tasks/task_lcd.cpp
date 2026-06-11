// =====================================================
// task_lcd.cpp
// =====================================================

#include <Arduino.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "config/pins.h"

#include "core/globals.h"
#include "core/system_bits.h"
#include "core/rfid_modes.h"

// =====================================================
// LCD CONFIG
// =====================================================
LiquidCrystal_I2C lcd(
    0x27,
    16,
    2
);

// =====================================================
// LCD CACHE
// =====================================================
String lastLine1 = "";
String lastLine2 = "";

// =====================================================
// LCD MESSAGE TIMER
// =====================================================
extern unsigned long lcdMessageTime;

// =====================================================
// EXTERNAL LCD MESSAGE
// =====================================================
extern String lcdLine1;
extern String lcdLine2;

// =====================================================
// CENTER TEXT
// =====================================================
String centerText(String text)
{
    int len = text.length();

    if (len > 16)
    {
        text = text.substring(0, 16);
        len = 16;
    }

    int leftPadding = (16 - len) / 2;

    String result = "";

    for (int i = 0; i < leftPadding; i++)
    {
        result += " ";
    }

    result += text;

    return result;
}

// =====================================================
// CLEAR LCD
// =====================================================
void clearLCD()
{
    lcd.setCursor(0, 0);
    lcd.print("                ");

    lcd.setCursor(0, 1);
    lcd.print("                ");
}

// =====================================================
// UPDATE LCD
// =====================================================
void updateLCD(
    String line1,
    String line2)
{
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate < 200)
    {
        return;
    }

    lastUpdate = millis();

    if (
        line1 == lastLine1 &&
        line2 == lastLine2)
    {
        return;
    }

    lastLine1 = line1;
    lastLine2 = line2;

    clearLCD();

    lcd.setCursor(0, 0);
    lcd.print(centerText(line1));

    lcd.setCursor(0, 1);
    lcd.print(centerText(line2));
}

// =====================================================
// TASK LCD
// =====================================================
void taskLCD(void *pv)
{
    Wire.begin(
        I2C_SDA,
        I2C_SCL
    );

    Wire.setClock(100000);

    lcd.init();
    lcd.backlight();

    Serial.println("LCD TASK STARTED");

    updateLCD(
        "SMART SAFE",
        "STARTING..."
    );

    vTaskDelay(
        pdMS_TO_TICKS(2000)
    );

    while (1)
    {
        EventBits_t bits =
            xEventGroupGetBits(
                systemEvents
            );

        // =========================================
        // ALARM
        // =========================================
        if (bits & BIT_ALARM_ACTIVE)
        {
            updateLCD(
                "!!! ALERT !!!",
                "SECURITY"
            );
        }

        // =========================================
        // ADMIN MODE
        // =========================================
        else if (bits & BIT_ADMIN_MODE)
        {
            updateLCD(
                "ADMIN MODE",
                "1ADD 2DEL 3EXIT"
            );
        }

        // =========================================
        // CUSTOM MESSAGE
        // Ưu tiên hiển thị các thông báo:
        // RFID ADDED
        // RFID FAILED
        // FINGER ADDED
        // FINGER DELETED
        // ACCESS DENIED
        // =========================================
        else if (
            lcdLine1 != "" &&
            millis() - lcdMessageTime < 3000
        )
        {
            updateLCD(
                lcdLine1,
                lcdLine2
            );
        }

        // =========================================
        // ADD MODE
        // =========================================
        else if (
            currentRFIDMode ==
            RFID_MODE_ADD
        )
        {
            updateLCD(
                "ADD MODE",
                "SCAN CARD"
            );
        }

        // =========================================
        // DELETE MODE
        // =========================================
        else if (
            currentRFIDMode ==
            RFID_MODE_DELETE
        )
        {
            updateLCD(
                "DELETE MODE",
                "SCAN CARD"
            );
        }

        // =========================================
        // AUTH OK
        // =========================================
        else if (
            bits & BIT_AUTH_OK
        )
        {
            updateLCD(
                "ACCESS OK",
                "SAFE OPEN"
            );
        }

        // =========================================
        // FINGER OK
        // =========================================
        else if (
            bits & BIT_FINGER_OK
        )
        {
            updateLCD(
                "FINGER OK",
                "ENTER PASS"
            );
        }

        // =========================================
        // RFID OK
        // =========================================
        else if (
            bits & BIT_RFID_OK
        )
        {
            updateLCD(
                "CARD OK",
                "SCAN FINGER"
            );
        }

        // =========================================
        // DEFAULT
        // =========================================
        else
        {
            updateLCD(
                "SAFE LOCKED",
                "SCAN RFID"
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(300)
        );
    }
}