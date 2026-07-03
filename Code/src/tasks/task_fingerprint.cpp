// =====================================================
// task_fingerprint.cpp
// AS608/R305 - CHECK BACKEND + BACKEND ENROLL VERSION
// FIX:
// - Sai vân tay đủ maxWrongPassword lần -> ALARM
// - Không quét lặp sau khi OK/FAIL
// - Add vân tay tìm ID trống thật
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
// EXTERN
// =====================================================
extern bool checkAuthFromBackend(String methodType, String methodValue);

extern String lcdLine1;
extern String lcdLine2;
extern unsigned long lcdMessageTime;

extern int maxWrongPassword;

// =====================================================
// BACKEND ENROLL STATE
// =====================================================
bool backendFingerBusy = false;

// =====================================================
// SCAN CONTROL
// =====================================================
unsigned long lastFingerScanTime = 0;
const uint32_t FINGER_SCAN_COOLDOWN_MS = 1200;

// =====================================================
// FAIL HANDLER
// =====================================================
void handleFingerFailAlarm()
{
    failedAttempts++;

    Serial.print("[FINGER] FAILED ATTEMPTS = ");
    Serial.println(failedAttempts);

    Serial.print("[FINGER] MAX WRONG = ");
    Serial.println(maxWrongPassword);

    lcdLine1 = "FINGER FAIL";
    lcdLine2 =
        "FAIL " +
        String(failedAttempts) +
        "/" +
        String(maxWrongPassword);

    lcdMessageTime = millis();

    buzzerBeep(1000, 300);
    ledPulse(LED_MODE_RED, 500);

    if (failedAttempts >= maxWrongPassword)
    {
        Serial.println("[FINGER] TOO MANY FAIL -> ALARM");

        xEventGroupSetBits(
            systemEvents,
            BIT_ALARM_ACTIVE |
            BIT_NEED_GPS |
            BIT_TRACKING_MODE
        );

        lcdLine1 = "ALARM ACTIVE";
        lcdLine2 = "FINGER FAIL";
        lcdMessageTime = millis();

        rfidAuthenticated = false;
        fingerAuthenticated = false;
        authenticated = false;

        xEventGroupClearBits(
            systemEvents,
            BIT_RFID_OK |
            BIT_FINGER_OK |
            BIT_KEYPAD_OK |
            BIT_AUTH_OK
        );
    }
}

// =====================================================
// WAIT FINGER REMOVED
// =====================================================
void waitFingerRemoved(uint32_t timeoutMs)
{
    unsigned long start = millis();

    while (millis() - start < timeoutMs)
    {
        uint8_t p = finger.getImage();

        if (p == FINGERPRINT_NOFINGER)
        {
            Serial.println("[FINGER] FINGER REMOVED");
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    Serial.println("[FINGER] WAIT REMOVE TIMEOUT");
}

// =====================================================
// FIND FREE FINGERPRINT ID
// =====================================================
int findFreeFingerprintId()
{
    for (int id = 1; id <= 127; id++)
    {
        uint8_t p = finger.loadModel(id);

        if (p != FINGERPRINT_OK)
        {
            Serial.print("[FINGER ENROLL] FREE ID = ");
            Serial.println(id);
            return id;
        }
    }

    Serial.println("[FINGER ENROLL] NO FREE ID");
    return -1;
}

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
// CHECK FINGERPRINT LOCAL
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

    p = finger.image2Tz(1);

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

    int id = findFreeFingerprintId();

    if (id <= 0)
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
        lcdLine1 = "PLACE FINGER";
        lcdLine2 = "LAN 1";
        lcdMessageTime = millis();

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

    waitFingerRemoved(8000);

    vTaskDelay(pdMS_TO_TICKS(500));

    lcdLine1 = "PLACE AGAIN";
    lcdLine2 = "LAN 2";
    lcdMessageTime = millis();

    start = millis();
    p = -1;

    while (millis() - start < timeoutMs)
    {
        lcdLine1 = "PLACE AGAIN";
        lcdLine2 = "LAN 2";
        lcdMessageTime = millis();

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

    waitFingerRemoved(8000);

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

        if (fingerAuthenticated)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (!rfidAuthenticated)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (millis() - lastFingerScanTime < FINGER_SCAN_COOLDOWN_MS)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        int id = checkFingerprint();

        if (id == -1)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        lastFingerScanTime = millis();

        // =================================================
        // LOCAL MATCH OK
        // =================================================
        if (id > 0)
        {
            Serial.print("FINGERPRINT LOCAL OK, ID: ");
            Serial.println(id);

            bool valid = checkAuthFromBackend(
                "FINGERPRINT",
                String(id)
            );

            if (valid)
            {
                fingerAuthenticated = true;
                failedAttempts = 0;

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

                waitFingerRemoved(8000);

                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
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

                handleFingerFailAlarm();

                waitFingerRemoved(8000);

                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
        }

        // =================================================
        // LOCAL FAIL
        // =================================================
        if (id == 0)
        {
            fingerAuthenticated = false;

            xEventGroupClearBits(
                systemEvents,
                BIT_FINGER_OK | BIT_AUTH_OK
            );

            Serial.println("FINGERPRINT FAIL");

            handleFingerFailAlarm();

            waitFingerRemoved(8000);

            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}