#include <Arduino.h>

#include <Preferences.h>

#include "core/globals.h"

Preferences rfidPrefs;

// =====================================================
// EXTERNAL RFID
// =====================================================
extern String userCards[20];

extern int totalCards;


// =====================================================
// LOAD CARDS
// =====================================================
void loadRFIDCards() {

    rfidPrefs.begin(
        "rfid",
        false
    );

    totalCards =
        rfidPrefs.getInt(
            "count",
            0
        );

    Serial.print(
        "LOAD RFID COUNT: "
    );

    Serial.println(
        totalCards
    );

    for (int i = 0;
         i < totalCards;
         i++) {

        String key =
            "card" + String(i);

        userCards[i] =
            rfidPrefs.getString(
                key.c_str(),
                ""
            );

        Serial.print(
            "LOAD CARD: "
        );

        Serial.println(
            userCards[i]
        );
    }

    rfidPrefs.end();
}


// =====================================================
// SAVE CARD
// =====================================================
void saveRFIDCard(String uid) {

    rfidPrefs.begin(
        "rfid",
        false
    );

    userCards[totalCards] =
        uid;

    String key =
        "card" + String(totalCards);

    rfidPrefs.putString(
        key.c_str(),
        uid
    );

    totalCards++;

    rfidPrefs.putInt(
        "count",
        totalCards
    );

    rfidPrefs.end();

    Serial.println(
        "RFID SAVED FLASH"
    );
}


// =====================================================
// DELETE CARD
// =====================================================
void deleteRFIDCard(String uid) {

    rfidPrefs.begin(
        "rfid",
        false
    );

    int foundIndex = -1;

    for (int i = 0;
         i < totalCards;
         i++) {

        if (userCards[i] == uid) {

            foundIndex = i;

            break;
        }
    }

    // not found
    if (foundIndex == -1) {

        rfidPrefs.end();

        return;
    }

    // shift RAM
    for (int i = foundIndex;
         i < totalCards - 1;
         i++) {

        userCards[i] =
            userCards[i + 1];
    }

    totalCards--;

    // rewrite flash
    rfidPrefs.clear();

    rfidPrefs.putInt(
        "count",
        totalCards
    );

    for (int i = 0;
         i < totalCards;
         i++) {

        String key =
            "card" + String(i);

        rfidPrefs.putString(
            key.c_str(),
            userCards[i]
        );
    }

    rfidPrefs.end();

    Serial.println(
        "RFID DELETED FLASH"
    );
}