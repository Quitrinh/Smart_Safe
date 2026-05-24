#pragma once

typedef enum
{
    // =====================================
    // AUTH
    // =====================================
    EVENT_PASSWORD_OK,
    EVENT_PASSWORD_FAIL,

    EVENT_RFID_OK,
    EVENT_RFID_FAIL,

    EVENT_FINGER_OK,
    EVENT_FINGER_FAIL,

    // =====================================
    // SECURITY
    // =====================================
    EVENT_VIBRATION,
    EVENT_DOOR_OPEN,
    EVENT_SMOKE,

    EVENT_UNAUTHORIZED,

    // =====================================
    // SYSTEM
    // =====================================
    EVENT_UNLOCK,
    EVENT_LOCK,

    // =====================================
    // ALARM
    // =====================================
    EVENT_ALARM_ON,
    EVENT_ALARM_OFF

} EventType;


// =========================================
// SYSTEM EVENT
// =========================================
typedef struct
{
    EventType type;

    char message[64];

} SystemEvent;