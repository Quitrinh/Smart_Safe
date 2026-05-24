// =====================================================
// main.cpp
// FINAL GPS + TELEGRAM VERSION
// =====================================================

#include <Arduino.h>

#include "core/globals.h"
#include "core/events.h"
#include "core/rfid_storage.h"
#include "core/buzzer.h"


// =====================================================
// GLOBAL QUEUE
// =====================================================
QueueHandle_t systemQueue;

// TELEGRAM QUEUE
QueueHandle_t telegramQueue;

// LCD QUEUE
QueueHandle_t lcdQueue;


// =====================================================
// SIM MUTEX
// =====================================================
SemaphoreHandle_t simMutex;


// =====================================================
// EVENT GROUP
// =====================================================
EventGroupHandle_t systemEvents;


// =====================================================
// SYSTEM STATE
// =====================================================
SystemState currentState =
    STATE_LOCKED;

SafeState safeState =
    SAFE_LOCKED;


// =====================================================
// RFID MODE
// =====================================================
RFIDMode currentRFIDMode =
    RFID_MODE_NORMAL;


// =====================================================
// FAILED PASSWORD
// =====================================================
int failedAttempts = 0;


// =====================================================
// AUTH STATES
// =====================================================
bool authenticated = false;

bool rfidAuthenticated = false;

bool fingerAuthenticated = false;

bool adminMode = false;


// =====================================================
// GPS GLOBAL
// =====================================================
double gpsLat = 0;

double gpsLng = 0;

bool gpsValid = false;


// =====================================================
// FINGER STATE
// =====================================================
bool fingerReady = false;


// =====================================================
// LCD
// =====================================================
String lcdLine1 = "";

String lcdLine2 = "";

unsigned long
lcdMessageTime = 0;


// =====================================================
// TELEGRAM TIMER
// =====================================================
unsigned long
lastTelegramTime = 0;


// =====================================================
// RFID STORAGE
// =====================================================
String userCards[20];

int totalCards = 0;


// =====================================================
// TASK DECLARE
// =====================================================
void taskAlarm(void *pv);

void taskSensor(void *pv);

void taskKeypad(void *pv);

void taskServo(void *pv);

void taskLCD(void *pv);

void taskRFID(void *pv);

void taskSIM(void *pv);

void taskTelegram(void *pv);

void taskGPS(void *pv);

void taskFingerprint(void *pv);


// =====================================================
// SETUP
// =====================================================
void setup()
{
    // =============================================
    // SERIAL
    // =============================================
    Serial.begin(115200); 

    delay(1000);

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "SMART SAFE SYSTEM START"
    );

    Serial.println(
        "================================="
    );

    // =============================================
    // SYSTEM QUEUE
    // =============================================
    systemQueue =
        xQueueCreate(
            20,
            sizeof(SystemEvent)
        );

    // =============================================
    // TELEGRAM QUEUE
    // =============================================
    telegramQueue =
        xQueueCreate(
            20,
            sizeof(SystemEvent)
        );

    // =============================================
    // LCD QUEUE
    // =============================================
    lcdQueue =
        xQueueCreate(
            10,
            sizeof(LCDMessage)
        );

    // =============================================
    // SIM MUTEX
    // =============================================
    simMutex =
        xSemaphoreCreateMutex();

    // =============================================
    // EVENT GROUP
    // =============================================
    systemEvents =
        xEventGroupCreate();

    // =============================================
    // CHECK CREATE
    // =============================================
    if (
        systemQueue == NULL
    )
    {
        Serial.println(
            "SYSTEM QUEUE FAIL"
        );
    }

    if (
        telegramQueue == NULL
    )
    {
        Serial.println(
            "TELEGRAM QUEUE FAIL"
        );
    }

    if (
        lcdQueue == NULL
    )
    {
        Serial.println(
            "LCD QUEUE FAIL"
        );
    }

    if (
        simMutex == NULL
    )
    {
        Serial.println(
            "SIM MUTEX FAIL"
        );
    }

    // =============================================
    // LOAD RFID
    // =============================================
    loadRFIDCards();

    Serial.print(
        "TOTAL RFID CARD: "
    );

    Serial.println(
        totalCards
    );

    // =============================================
    // SENSOR TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskSensor,
        "Sensor Task",
        4096,
        NULL,
        1,
        NULL,
        0
    );

    // =============================================
    // ALARM TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskAlarm,
        "Alarm Task",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    // =============================================
    // RFID TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskRFID,
        "RFID Task",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    // =============================================
    // KEYPAD TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskKeypad,
        "Keypad Task",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    // =============================================
    // SERVO TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskServo,
        "Servo Task",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    // =============================================
    // LCD TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskLCD,
        "LCD Task",
        6144,
        NULL,
        1,
        NULL,
        0
    );

    // =============================================
    // SIM TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskSIM,
        "SIM Task",
        8192,
        NULL,
        1,
        NULL,
        0
    );

    // =============================================
    // TELEGRAM TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskTelegram,
        "Telegram Task",
        8192,
        NULL,
        1,
        NULL,
        0
    );


    // =============================================
    // FINGERPRINT TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskFingerprint,
        "Fingerprint Task",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    // =============================================
    // GPS TASK
    // =============================================
    xTaskCreatePinnedToCore(
        taskGPS,
        "GPS Task",
        4096,
        NULL,
        1,
        NULL,
        0
    );

    Serial.println();

    Serial.println(
        "SYSTEM READY"
    );

    Serial.println(
        "WAIT RFID..."
    );

    Serial.print(
        "FREE HEAP: "
    );

    Serial.println(
        ESP.getFreeHeap()
    );
}


// =====================================================
// LOOP
// =====================================================
void loop()
{
    while (1)
    {
        // Kiểm tra xác thực 3 lớp: RFID, vân tay, mật khẩu
        if (rfidAuthenticated && fingerAuthenticated && authenticated) {
            // Mở két, reset trạng thái xác thực
            safeState = SAFE_OPEN;
            lcdLine1 = "UNLOCKED";
            lcdLine2 = "SAFE OPEN";
            lcdMessageTime = millis();
            buzzerBeep(2000, 100);
            // Reset trạng thái xác thực để tránh mở lại
            rfidAuthenticated = false;
            fingerAuthenticated = false;
            authenticated = false;
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}