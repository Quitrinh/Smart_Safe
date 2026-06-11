// =====================================================
// task_rfid.cpp
// RFID CHECK BACKEND + WAIT REMOVE AFTER ENROLL
// =====================================================

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#include "config/pins.h"
#include "core/globals.h"
#include "core/system_bits.h"
#include "core/led.h"
#include "core/buzzer.h"
#include "core/rfid_modes.h"
#include "core/rfid_storage.h"

MFRC522 rfid(RFID_SS, RFID_RST);

extern String lcdLine1;
extern String lcdLine2;
extern unsigned long lcdMessageTime;

extern bool checkAuthFromBackend(String methodType, String methodValue);

String masterUID = "23de1907";

bool backendRFIDBusy = false;
bool waitRemoveAfterEnroll = false;

String lastEnrollUID = "";
unsigned long lastEnrollTime = 0;

bool isUserCard(String uid)
{
    for (int i = 0; i < totalCards; i++)
    {
        if (uid == userCards[i])
        {
            return true;
        }
    }
    return false;
}

void showLCDMessage(String line1, String line2, int duration = 2000)
{
    lcdLine1 = line1;
    lcdLine2 = line2;
    lcdMessageTime = millis();
}

void resetAuthState()
{
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

void addCard(String uid)
{
    if (isUserCard(uid))
    {
        showLCDMessage("CARD EXISTS", "");
        buzzerBeep(1000, 300);
        return;
    }

    if (uid == masterUID)
    {
        showLCDMessage("MASTER BLOCK", "");
        buzzerBeep(1000, 300);
        return;
    }

    if (totalCards >= 20)
    {
        showLCDMessage("MEMORY FULL", "");
        buzzerBeep(1000, 300);
        return;
    }

    saveRFIDCard(uid);

    Serial.println("CARD ADDED LOCAL");

    showLCDMessage("CARD ADDED", uid);
    buzzerBeep(3000, 200);
}

void deleteCard(String uid)
{
    if (!isUserCard(uid))
    {
        showLCDMessage("NOT FOUND", "");
        buzzerBeep(1000, 500);
        return;
    }

    deleteRFIDCard(uid);

    Serial.println("CARD DELETED LOCAL");

    showLCDMessage("CARD DELETE", uid);
    buzzerBeep(2500, 200);
}

String readRFIDUID()
{
    String uid = "";

    for (byte i = 0; i < rfid.uid.size; i++)
    {
        if (rfid.uid.uidByte[i] < 0x10)
        {
            uid += "0";
        }

        uid += String(rfid.uid.uidByte[i], HEX);
    }

    uid.toLowerCase();
    return uid;
}

// =====================================================
// APP ADD RFID
// =====================================================
String waitRFIDCardFromRC522(uint32_t timeoutMs)
{
    backendRFIDBusy = true;
    resetAuthState();

    unsigned long start = millis();

    Serial.println("[RFID BACKEND] WAIT NEW CARD");

    showLCDMessage("ADD RFID", "SCAN CARD");

    while (millis() - start < timeoutMs)
    {
        if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
        {
            String uid = readRFIDUID();

            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();

            backendRFIDBusy = false;

            Serial.print("[RFID BACKEND] UID = ");
            Serial.println(uid);

            if (uid == masterUID)
            {
                Serial.println("[RFID BACKEND] MASTER BLOCK");
                showLCDMessage("MASTER BLOCK", "");
                return "";
            }

            lastEnrollUID = uid;
            lastEnrollTime = millis();
            waitRemoveAfterEnroll = true;

            resetAuthState();

            showLCDMessage("RFID ADDED", "REMOVE CARD");

            buzzerBeep(3000, 200);
            ledPulse(LED_MODE_GREEN, 300);

            return uid;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    backendRFIDBusy = false;

    Serial.println("[RFID BACKEND] TIMEOUT");

    showLCDMessage("RFID TIMEOUT", "");

    return "";
}

// =====================================================
// TASK RFID
// =====================================================
void taskRFID(void *pv)
{
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, RFID_SS);
    rfid.PCD_Init();

    Serial.println("RFID TASK STARTED");

    while (1)
    {
        // Khi đang ADD_RFID từ app thì task thường không xử lý login
        if (backendRFIDBusy)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Sau khi ADD_RFID xong, bắt buộc nhấc thẻ ra
        if (waitRemoveAfterEnroll)
        {
            resetAuthState();

            if (!rfid.PICC_IsNewCardPresent())
            {
                waitRemoveAfterEnroll = false;

                lastEnrollUID = "";
                lastEnrollTime = 0;

                showLCDMessage("SAFE LOCKED", "SCAN RFID");

                Serial.println("[RFID] CARD REMOVED, BACK TO NORMAL");
            }
            else
            {
                showLCDMessage("RFID ADDED", "REMOVE CARD");
            }

            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }

        if (!rfid.PICC_IsNewCardPresent())
        {
            vTaskDelay(pdMS_TO_TICKS(30));
            continue;
        }

        if (!rfid.PICC_ReadCardSerial())
        {
            vTaskDelay(pdMS_TO_TICKS(30));
            continue;
        }

        String uid = readRFIDUID();

        Serial.print("CARD UID: ");
        Serial.println(uid);

        // ADD MODE LOCAL
        if (currentRFIDMode == RFID_MODE_ADD)
        {
            addCard(uid);

            currentRFIDMode = RFID_MODE_NORMAL;
            adminMode = false;

            xEventGroupClearBits(systemEvents, BIT_ADMIN_MODE);

            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();

            continue;
        }

        // DELETE MODE LOCAL
        if (currentRFIDMode == RFID_MODE_DELETE)
        {
            deleteCard(uid);

            currentRFIDMode = RFID_MODE_NORMAL;
            adminMode = false;

            xEventGroupClearBits(systemEvents, BIT_ADMIN_MODE);

            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();

            continue;
        }

        // MASTER CARD
        if (uid == masterUID)
        {
            Serial.println("MASTER CARD");

            adminMode = true;

            xEventGroupSetBits(systemEvents, BIT_ADMIN_MODE);

            showLCDMessage("ADMIN MODE", "1ADD 2DEL 3EXIT");

            buzzerBeep(3000, 200);
        }

        // USER CARD - CHECK BACKEND
        else if (checkAuthFromBackend("RFID", uid))
        {
            Serial.println("USER CARD OK FROM BACKEND");

            rfidAuthenticated = true;
            fingerAuthenticated = false;
            authenticated = false;

            xEventGroupSetBits(systemEvents, BIT_RFID_OK);

            xEventGroupClearBits(
                systemEvents,
                BIT_FINGER_OK |
                BIT_KEYPAD_OK |
                BIT_AUTH_OK
            );

            xEventGroupClearBits(
                systemEvents,
                BIT_ALARM_ACTIVE |
                BIT_NEED_GPS |
                BIT_TRACKING_MODE
            );

            showLCDMessage("CARD OK", "SCAN FINGER");

            buzzerBeep(2500, 150);
            ledPulse(LED_MODE_GREEN, 300);
        }

        // INVALID CARD
        else
        {
            Serial.println("INVALID CARD FROM BACKEND");

            resetAuthState();

            showLCDMessage("INVALID CARD", "ACCESS DENIED");

            buzzerBeep(1000, 500);
            ledPulse(LED_MODE_RED, 500);
        }

        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}