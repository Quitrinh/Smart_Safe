#pragma once

// =========================
// RFID RC522 (SPI)
// =========================
#define RFID_SS        5
#define RFID_RST       13

#define SPI_MOSI       23
#define SPI_MISO       19
#define SPI_SCK        18


// =========================
// GPS NEO6M (UART1)
// =========================
// =========================
// SIM A7608C (UART2)
// =========================
#define SIM_RX         26
#define SIM_TX         25


// =========================
// Fingerprint AS608
// =========================

// =========================
#define GPS_RX         39
#define GPS_TX         -1


// =========================
// Fingerprint AS608
// =========================
#define FINGER_RX      16
#define FINGER_TX      17


// =========================
// TTP229 Keypad
// =========================
#define KEYPAD_SCL 22
#define KEYPAD_SDO 21


// =========================
// LCD I2C
// =========================
#define I2C_SDA 32
#define I2C_SCL 33


// =========================
// Sensors
// =========================

// SW420
#define VIBRATION_PIN  35

// // MQ2
// #define SMOKE_PIN      34

// Door Sensor
#define DOOR_PIN       27

// Flame Sensor
#define FLAME_PIN      36

// =========================
// Servo
// =========================
#define SERVO_PIN      14


// =========================
// Buzzer
// =========================
#define BUZZER_PIN     15

// =========================
// LED
// =========================
#define LED_RED_PIN    2
#define LED_GREEN_PIN  4

// =========================
// PUMP
// =========================
#define PUMP_RELAY_PIN   12