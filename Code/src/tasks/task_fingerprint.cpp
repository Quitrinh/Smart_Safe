// =====================================================
// task_fingerprint.cpp
// AS608/R305 - CHECK BACKEND + BACKEND ENROLL VERSION
// FIX:
// - Không quét lặp lần 2 sau khi quét đúng
// - Sau OK/FAIL đều chờ nhấc tay ra
// - Add vân tay tìm ID trống thật sự, không dùng templateCount + 1
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
// SCAN CONTROL
// =====================================================
unsigned long lastFingerScanTime = 0;
const uint32_t FINGER_SCAN_COOLDOWN_MS = 1200;

// =====================================================
// WAIT FINGER REMOVED
// Tránh quét lại cùng 1 ngón liên tục
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
// Không dùng templateCount + 1 nữa
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
// CHECK FINGERPRINT LOCAL SENSOR
// Chỉ nhận dạng trong module AS608, chưa xác thực database
// Return:
// -1 = không có ngón tay
//  0 = có ngón nhưng fail
// >0 = ID vân tay nhận dạng được
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

    // =========================
    // LAN 1
    // =========================
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

    waitFingerRemoved(8000);

    vTaskDelay(pdMS_TO_TICKS(500));

    // =========================
    // LAN 2
    // =========================
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

    // Chờ nhấc tay ra để taskFingerprint không quét lại ngay
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
        // Đang add vân tay từ backend thì không scan mở két
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

        // Nếu đã xác thực vân tay OK rồi thì không quét nữa
        // Tránh tình trạng OK xong lại FAIL do quét lại lần 2
        if (fingerAuthenticated)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Chỉ cho quét vân tay sau khi RFID đã hợp lệ
        if (!rfidAuthenticated)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Cooldown tránh quét quá nhanh
        if (millis() - lastFingerScanTime < FINGER_SCAN_COOLDOWN_MS)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        int id = checkFingerprint();

        // =================================================
        // KHÔNG CÓ NGÓN TAY
        // =================================================
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

                // Quan trọng: chờ nhấc tay ra
                waitFingerRemoved(8000);

                // Sau khi OK, không quét nữa cho tới khi hệ thống reset cờ
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

                lcdLine1 = "FINGER DENIED";
                lcdLine2 = "ACCESS DENIED";
                lcdMessageTime = millis();

                buzzerBeep(1000, 500);
                ledPulse(LED_MODE_RED, 500);

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

            lcdLine1 = "FINGER FAIL";
            lcdLine2 = "TRY AGAIN";
            lcdMessageTime = millis();

            buzzerBeep(1000, 300);
            ledPulse(LED_MODE_RED, 500);

            // Quan trọng: chờ nhấc tay ra để không fail liên tục
            waitFingerRemoved(8000);

            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}