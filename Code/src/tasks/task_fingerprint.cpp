// =====================================================
// task_fingerprint.cpp
// AS608/R305 - CHECK BACKEND + BACKEND ENROLL VERSION
// =====================================================

#include <Arduino.h>
#include <Adafruit_Fingerprint.h>

#include "core/globals.h"
#include "core/buzzer.h"
#include "core/led.h"
#include "core/system_bits.h"
#include "config/pins.h"

// =====================================================
// OBJECT
// =====================================================
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// =====================================================
// BACKEND AUTH CHECK
// Hàm này nằm trong file backend command
// =====================================================
extern bool checkAuthFromBackend(String methodType, String methodValue);

// =====================================================
// LCD MESSAGE
// =====================================================
extern String lcdLine1;
extern String lcdLine2;
extern unsigned long lcdMessageTime;

// =====================================================
// BACKEND ENROLL STATE
// =====================================================
bool backendFingerBusy = false;

// =====================================================
// SETUP SENSOR
// =====================================================
void setupFingerprintSensor()
{
    mySerial.begin(
        57600,
        SERIAL_8N1,
        FINGER_RX,
        FINGER_TX
    );

    finger.begin(57600);

    if (finger.verifyPassword())
    {
        Serial.println("FINGERPRINT SENSOR OK");
        fingerReady = true;
    }
    else
    {
        Serial.println("FINGERPRINT SENSOR FAIL");
        fingerReady = false;
    }
}

// =====================================================
// CHECK FINGERPRINT LOCAL SENSOR
// Chỉ nhận dạng trong module AS608, chưa xác thực database
// =====================================================
int checkFingerprint()
{
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER)
    {
        return -1;
    }

    if (p != FINGERPRINT_OK)
    {
        Serial.print("FINGER IMAGE ERROR: ");
        Serial.println(p);
        return 0;
    }

    p = finger.image2Tz();

    if (p != FINGERPRINT_OK)
    {
        Serial.print("FINGER CONVERT ERROR: ");
        Serial.println(p);
        return 0;
    }

    p = finger.fingerSearch();

    if (p == FINGERPRINT_OK)
    {
        Serial.print("FINGER MATCH ID LOCAL: ");
        Serial.println(finger.fingerID);

        Serial.print("CONFIDENCE: ");
        Serial.println(finger.confidence);

        return finger.fingerID;
    }

    if (p == FINGERPRINT_NOTFOUND)
    {
        Serial.println("FINGER NOT FOUND LOCAL");
        return 0;
    }

    Serial.print("FINGER SEARCH ERROR: ");
    Serial.println(p);

    return 0;
}

// =====================================================
// ENROLL FINGERPRINT FROM BACKEND COMMAND
// Dùng khi app gửi ADD_FINGER
// Lưu template vào AS608, trả ID về backend
// =====================================================
int enrollFingerprintFromAS608(uint32_t timeoutMs)
{
    backendFingerBusy = true;

    if (!fingerReady)
    {
        Serial.println("[FINGER ENROLL] SENSOR NOT READY");

        backendFingerBusy = false;
        return -1;
    }

    finger.getTemplateCount();

    int id = finger.templateCount + 1;

    if (id < 1)
    {
        id = 1;
    }

    if (id > 127)
    {
        Serial.println("[FINGER ENROLL] MEMORY FULL");

        backendFingerBusy = false;
        return -1;
    }

    Serial.print("[FINGER ENROLL] ID = ");
    Serial.println(id);

    lcdLine1 = "PLACE FINGER";
    lcdLine2 = "LAN 1";
    lcdMessageTime = millis();

    unsigned long start = millis();

    int p = -1;

    while (millis() - start < timeoutMs)
    {
        p = finger.getImage();

        if (p == FINGERPRINT_OK)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (p != FINGERPRINT_OK)
    {
        Serial.println("[FINGER ENROLL] NO FINGER 1");

        backendFingerBusy = false;
        return -1;
    }

    p = finger.image2Tz(1);

    if (p != FINGERPRINT_OK)
    {
        Serial.println("[FINGER ENROLL] IMAGE 1 FAIL");

        backendFingerBusy = false;
        return -1;
    }

    lcdLine1 = "REMOVE FINGER";
    lcdLine2 = "";
    lcdMessageTime = millis();

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (finger.getImage() != FINGERPRINT_NOFINGER)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    lcdLine1 = "PLACE AGAIN";
    lcdLine2 = "LAN 2";
    lcdMessageTime = millis();

    start = millis();
    p = -1;

    while (millis() - start < timeoutMs)
    {
        p = finger.getImage();

        if (p == FINGERPRINT_OK)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (p != FINGERPRINT_OK)
    {
        Serial.println("[FINGER ENROLL] NO FINGER 2");

        backendFingerBusy = false;
        return -1;
    }

    p = finger.image2Tz(2);

    if (p != FINGERPRINT_OK)
    {
        Serial.println("[FINGER ENROLL] IMAGE 2 FAIL");

        backendFingerBusy = false;
        return -1;
    }

    p = finger.createModel();

    if (p != FINGERPRINT_OK)
    {
        Serial.println("[FINGER ENROLL] CREATE MODEL FAIL");

        backendFingerBusy = false;
        return -1;
    }

    p = finger.storeModel(id);

    if (p != FINGERPRINT_OK)
    {
        Serial.println("[FINGER ENROLL] STORE FAIL");

        backendFingerBusy = false;
        return -1;
    }

    Serial.println("[FINGER ENROLL] SUCCESS");

    lcdLine1 = "FINGER ADDED";
    lcdLine2 = "ID: " + String(id);
    lcdMessageTime = millis();

    buzzerBeep(3000, 200);
    ledPulse(LED_MODE_GREEN, 500);

    backendFingerBusy = false;

    return id;
}

// =====================================================
// TASK FINGERPRINT
// =====================================================
void taskFingerprint(void *pv)
{
    setupFingerprintSensor();

    while (1)
    {
        if (backendFingerBusy)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!fingerReady)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Chỉ cho quét vân tay sau khi RFID đã hợp lệ
        if (!rfidAuthenticated)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int id = checkFingerprint();

        if (id > 0)
        {
            Serial.print("FINGERPRINT LOCAL OK, ID: ");
            Serial.println(id);

            // =================================================
            // CHECK DATABASE/BACKEND
            // Nếu vân tay đã bị xoá trên app/database thì FAIL
            // =================================================
            bool valid = checkAuthFromBackend(
                "FINGERPRINT",
                String(id)
            );

            if (valid)
            {
                fingerAuthenticated = true;

                xEventGroupSetBits(
                    systemEvents,
                    BIT_FINGER_OK
                );

                Serial.print("FINGERPRINT OK FROM BACKEND, ID: ");
                Serial.println(id);

                lcdLine1 = "FINGER OK";
                lcdLine2 = "ENTER PASS";
                lcdMessageTime = millis();

                buzzerBeep(2000, 100);
                ledPulse(LED_MODE_BLUE, 300);
            }
            else
            {
                fingerAuthenticated = false;

                xEventGroupClearBits(
                    systemEvents,
                    BIT_FINGER_OK | BIT_AUTH_OK
                );

                Serial.print("FINGERPRINT DENIED FROM BACKEND, ID: ");
                Serial.println(id);

                lcdLine1 = "FINGER DELETED";
                lcdLine2 = "ACCESS DENIED";
                lcdMessageTime = millis();

                buzzerBeep(1000, 500);
                ledPulse(LED_MODE_RED, 500);
            }
        }
        else if (id == 0)
        {
            fingerAuthenticated = false;

            xEventGroupClearBits(
                systemEvents,
                BIT_FINGER_OK | BIT_AUTH_OK
            );

            Serial.println("FINGERPRINT FAIL");

            lcdLine1 = "FINGER FAIL";
            lcdLine2 = "TRY AGAIN";
            lcdMessageTime = millis();

            buzzerBeep(1000, 300);
            ledPulse(LED_MODE_RED, 500);
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}