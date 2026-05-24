// =====================================================
// task_fingerprint.cpp
// Task xử lý cảm biến vân tay AS608/R305
// =====================================================

#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include "core/globals.h"
#include "core/buzzer.h"
#include "core/system_bits.h"

#define FINGER_RX 16
#define FINGER_TX 17

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setupFingerprintSensor() {
	mySerial.begin(57600, SERIAL_8N1, FINGER_RX, FINGER_TX);
	finger.begin(57600);
	if (finger.verifyPassword()) {
		Serial.println("FINGERPRINT SENSOR OK");
		fingerReady = true;
	} else {
		Serial.println("FINGERPRINT SENSOR FAIL");
		fingerReady = false;
	}
}

uint8_t checkFingerprint() {
	uint8_t p = finger.getImage();
	if (p != FINGERPRINT_OK) return 0;
	p = finger.image2Tz();
	if (p != FINGERPRINT_OK) return 0;
	p = finger.fingerSearch();
	if (p == FINGERPRINT_OK) {
		return finger.fingerID;
	}
	return 0;
}

void taskFingerprint(void *pv) {
	setupFingerprintSensor();
	while (1) {
		if (fingerReady) {
			int id = checkFingerprint();
			if (id > 0) {
				fingerAuthenticated = true;
				authenticated = true;
				Serial.print("FINGERPRINT OK, ID: ");
				Serial.println(id);
				xEventGroupSetBits(systemEvents, BIT_FINGER_OK);
				buzzerBeep(2000, 100);
				lcdLine1 = "FINGER OK";
				lcdLine2 = "SAFE OPEN";
				lcdMessageTime = millis();
				vTaskDelay(2000 / portTICK_PERIOD_MS);
			} else {
				fingerAuthenticated = false;
			}
		}
		vTaskDelay(300 / portTICK_PERIOD_MS);
	}
}
