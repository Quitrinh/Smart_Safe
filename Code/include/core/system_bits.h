// =====================================================
// system_bits.h
// SMART SAFE EVENT BITS
// =====================================================
#pragma once
// AUTH
#define BIT_AUTH_OK (1 << 0)
#define BIT_FINGER_OK (1 << 1)
#define BIT_RFID_OK (1 << 2)
#define BIT_KEYPAD_OK (1 << 3)
// SENSOR / SECURITY
#define BIT_DOOR_OPEN (1 << 4)
#define BIT_GAS_ALARM (1 << 5)
#define BIT_VIB_ALARM (1 << 6)
#define BIT_ALARM_ACTIVE (1 << 7)
// MODE
#define BIT_ADMIN_MODE (1 << 8)
// SERVICE WAKEUP
#define BIT_NEED_GPS (1 << 9)
#define BIT_GPS_READY (1 << 10)
#define BIT_TRACKING_MODE  (1 << 11)
#define BIT_NEED_SIM (1 << 12)
#define BIT_NEED_API (1 << 13)
#define BIT_FLAME_ACTIVE   (1 << 14)