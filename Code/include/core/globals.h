#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "safe_state.h"
#include "core/states.h"
#include "core/rfid_modes.h"
#include "lcd_message.h"

extern double homeLat;
extern double homeLng;
// ================= AUTH FLOW =================
enum AuthStep {
  AUTH_STEP_IDLE,
  AUTH_STEP_RFID_OK,
  AUTH_STEP_FINGER_OK,
  AUTH_STEP_PASSWORD_OK,
  AUTH_STEP_SUCCESS
};

// ================= QUEUES =================
extern QueueHandle_t lcdQueue;
extern QueueHandle_t systemQueue;
extern QueueHandle_t telegramQueue;

// ================= SYSTEM STATE =================
extern SystemState currentState;
extern RFIDMode currentRFIDMode;
extern SafeState safeState;
extern SafeState lastTelegramState;

// ================= AUTH VARIABLES =================
extern AuthStep authStep;
extern unsigned long authStepTime;
extern int failedAttempts;

extern bool rfidAuthenticated;
extern bool fingerAuthenticated;
extern bool passwordAuthenticated;
extern bool authenticated;
extern bool fingerReady;
extern bool adminMode;

// ================= RFID STORAGE =================
extern String userCards[20];
extern int totalCards;

// ================= RTOS =================
extern EventGroupHandle_t systemEvents;
extern SemaphoreHandle_t simMutex;

// ================= LCD =================
extern String lcdLine1;
extern String lcdLine2;
extern unsigned long lcdMessageTime;

// ================= TELEGRAM / SIM =================
extern unsigned long lastTelegramTime;

// ================= GPS =================
extern double gpsLat;
extern double gpsLng;
extern bool gpsValid;