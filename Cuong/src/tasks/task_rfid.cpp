// =====================================================
// task_rfid.cpp
// FINAL STABLE VERSION
// =====================================================

#include <Arduino.h>

#include <SPI.h>
#include <MFRC522.h>

#include "config/pins.h"

#include "core/globals.h"
#include "core/system_bits.h"

#include "core/buzzer.h"
#include "core/rfid_modes.h"
#include "core/rfid_storage.h"


// =====================================================
// RFID OBJECT
// =====================================================
MFRC522 rfid(
    RFID_SS,
    RFID_RST
);


// =====================================================
// LCD MESSAGE
// =====================================================
extern String lcdLine1;

extern String lcdLine2;

extern unsigned long
lcdMessageTime;


// =====================================================
// MASTER CARD
// =====================================================
String masterUID =
    "23de1907";


// =====================================================
// CHECK USER CARD
// =====================================================
bool isUserCard(
    String uid
)
{
    for (
        int i = 0;
        i < totalCards;
        i++
    )
    {
        if (
            uid == userCards[i]
        )
        {
            return true;
        }
    }

    return false;
}


// =====================================================
// SHOW LCD MESSAGE
// =====================================================
void showLCDMessage(
    String line1,
    String line2,
    int duration = 2000
)
{
    lcdLine1 = line1;

    lcdLine2 = line2;

    lcdMessageTime =
        millis();
}


// =====================================================
// ADD CARD
// =====================================================
void addCard(
    String uid
)
{
    // =============================================
    // CARD EXISTS
    // =============================================
    if (
        isUserCard(uid)
    )
    {
        showLCDMessage(
            "CARD EXISTS",
            ""
        );

        buzzerBeep(
            1000,
            300
        );

        return;
    }

    // =============================================
    // BLOCK MASTER
    // =============================================
    if (
        uid == masterUID
    )
    {
        showLCDMessage(
            "MASTER BLOCK",
            ""
        );

        buzzerBeep(
            1000,
            300
        );

        return;
    }

    // =============================================
    // MEMORY FULL
    // =============================================
    if (
        totalCards >= 20
    )
    {
        showLCDMessage(
            "MEMORY FULL",
            ""
        );

        buzzerBeep(
            1000,
            300
        );

        return;
    }

    // =============================================
    // SAVE CARD
    // =============================================
    saveRFIDCard(uid);

    Serial.println(
        "CARD ADDED"
    );

    showLCDMessage(
        "CARD ADDED",
        uid
    );

    buzzerBeep(
        3000,
        200
    );
}


// =====================================================
// DELETE CARD
// =====================================================
void deleteCard(
    String uid
)
{
    // =============================================
    // NOT FOUND
    // =============================================
    if (
        !isUserCard(uid)
    )
    {
        showLCDMessage(
            "NOT FOUND",
            ""
        );

        buzzerBeep(
            1000,
            500
        );

        return;
    }

    // =============================================
    // DELETE
    // =============================================
    deleteRFIDCard(uid);

    Serial.println(
        "CARD DELETED"
    );

    showLCDMessage(
        "CARD DELETE",
        uid
    );

    buzzerBeep(
        2500,
        200
    );
}


// =====================================================
// TASK RFID
// =====================================================
void taskRFID(void *pv)
{
    // =============================================
    // INIT SPI
    // =============================================
    SPI.begin(
        SPI_SCK,
        SPI_MISO,
        SPI_MOSI,
        RFID_SS
    );

    // =============================================
    // INIT RFID
    // =============================================
    rfid.PCD_Init();

    Serial.println(
        "RFID TASK STARTED"
    );

    // =============================================
    // MAIN LOOP
    // =============================================
    while (1)
    {
        // =========================================
        // WAIT CARD
        // =========================================
        if (
            !rfid.PICC_IsNewCardPresent()
        )
        {
            vTaskDelay(
                50 /
                portTICK_PERIOD_MS
            );

            continue;
        }

        // =========================================
        // READ CARD
        // =========================================
        if (
            !rfid.PICC_ReadCardSerial()
        )
        {
            vTaskDelay(
                50 /
                portTICK_PERIOD_MS
            );

            continue;
        }

        // =========================================
        // GET UID
        // =========================================
        String uid = "";

        for (
            byte i = 0;
            i < rfid.uid.size;
            i++
        )
        {
            if (
                rfid.uid.uidByte[i]
                < 0x10
            )
            {
                uid += "0";
            }

            uid += String(
                rfid.uid.uidByte[i],
                HEX
            );
        }

        uid.toLowerCase();

        Serial.print(
            "CARD UID: "
        );

        Serial.println(
            uid
        );

        // =========================================
        // ADD MODE
        // =========================================
        if (
            currentRFIDMode ==
            RFID_MODE_ADD
        )
        {
            addCard(uid);
            showLCDMessage(
            "ADD SUCCESS",
            "",
            2000
        );
            currentRFIDMode =
                RFID_MODE_NORMAL;

            adminMode = false;

            xEventGroupClearBits(
                systemEvents,
                BIT_ADMIN_MODE
            );

            continue;
        }

        // =========================================
        // DELETE MODE
        // =========================================
        if (
            currentRFIDMode ==
            RFID_MODE_DELETE
        )
        {
            deleteCard(uid);
            showLCDMessage(
    "DELETE OK",
    "",
    2000
);
            currentRFIDMode =
                RFID_MODE_NORMAL;

            adminMode = false;

            xEventGroupClearBits(
                systemEvents,
                BIT_ADMIN_MODE
            );

            continue;
        }

        // =========================================
        // MASTER CARD
        // =========================================
        if (
            uid == masterUID
        )
        {
            Serial.println(
                "MASTER CARD"
            );

            adminMode = true;

            xEventGroupSetBits(
                systemEvents,
                BIT_ADMIN_MODE
            );
            buzzerBeep(
                3000,
                200
            );
        }

        // =========================================
        // USER CARD
        // =========================================
        else if (
            isUserCard(uid)
        )
        {
            Serial.println(
                "USER CARD OK"
            );

            // =====================================
            // RESET FINGER
            // =====================================
            fingerReady = false;

            fingerAuthenticated =
                false;

            xEventGroupClearBits(
                systemEvents,
                BIT_FINGER_OK
            );

            // =====================================
            // RFID AUTH
            // =====================================
            rfidAuthenticated =
                true;

            xEventGroupSetBits(
                systemEvents,
                BIT_RFID_OK
            );

            // =====================================
            // CLEAR ALARM
            // =====================================
            xEventGroupClearBits(
                systemEvents,
                BIT_ALARM_ACTIVE
            );

            // =====================================
            // LCD
            // =====================================
            showLCDMessage(
                "CARD OK",
                "SCAN FINGER",
                2000
            );

            // =====================================
            // BUZZER
            // =====================================
            buzzerBeep(
                2500,
                150
            );
        }

        // =========================================
        // INVALID CARD
        // =========================================
        else
        {
            Serial.println(
                "INVALID CARD"
            );

            showLCDMessage(
                "INVALID CARD",
                "ACCESS DENIED"
            );

            buzzerBeep(
                1000,
                500
            );
        }

        // =========================================
        // STOP RFID
        // =========================================
        rfid.PICC_HaltA();

        rfid.PCD_StopCrypto1();

        vTaskDelay(
            1000 /
            portTICK_PERIOD_MS
        );
    }
}