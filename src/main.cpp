#include <Arduino.h>

#include <cstdint>
#include <sys/time.h>
#include "TFT_eSPI.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

bool USB = false; // Set to true if USB is connected, false otherwise

constexpr uint8_t BATTERY_ADC_PIN = A0; // GPIO1
constexpr uint8_t BATTERY_ADC_ENABLE_PIN = 6; // D5 / GPIO6
constexpr float BATTERY_VOLTAGE_DIVIDER_RATIO = 2.0f;
constexpr float BATTERY_EMPTY_VOLTAGE = 3.47f;
constexpr float BATTERY_FULL_VOLTAGE = 4.2f;

RTC_DATA_ATTR uint16_t imageIndex = 0;
RTC_DATA_ATTR uint32_t lastRtcSeedEpoch = 0;

#ifndef BUILD_UNIX_EPOCH
#define BUILD_UNIX_EPOCH 0
#endif

static bool seedRtcFromBuildIfNeeded() {
	constexpr uint32_t buildEpoch = BUILD_UNIX_EPOCH;
	if (buildEpoch < 1700000000UL) return false;
	if (lastRtcSeedEpoch == buildEpoch) return false;

	struct timeval tv = {};
	tv.tv_sec = static_cast<time_t>(buildEpoch);
	tv.tv_usec = 0;
	if (settimeofday(&tv, nullptr) != 0) return false;

	lastRtcSeedEpoch = buildEpoch;
	return true;
}

static float readBatteryVoltage() {
	digitalWrite(BATTERY_ADC_ENABLE_PIN, HIGH);
	delay(2);

	uint32_t sum = 0;
	for (int i = 0; i < 10; i++) {
		sum += analogRead(BATTERY_ADC_PIN);
		delay(2);
	}

	digitalWrite(BATTERY_ADC_ENABLE_PIN, LOW);

	int adcValue = sum / 10;
	return (adcValue / 4095.0f) * 3.3f * BATTERY_VOLTAGE_DIVIDER_RATIO;
}

static uint8_t batteryVoltageToPercent(float voltage) {
	if (voltage <= BATTERY_EMPTY_VOLTAGE) return 0;
	if (voltage >= BATTERY_FULL_VOLTAGE) return 100;

	float ratio = (voltage - BATTERY_EMPTY_VOLTAGE) / (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE);
	return static_cast<uint8_t>(ratio * 100.0f + 0.5f);
}

static void prepareForDeepSleep() {
	// Keep SD card deselected and park SPI lines to reduce idle draw.
	pinMode(21, OUTPUT);
	digitalWrite(21, HIGH);
	pinMode(7, OUTPUT);
	digitalWrite(7, LOW);
	pinMode(8, INPUT);
	pinMode(9, OUTPUT);
	digitalWrite(9, LOW);

	#ifdef USE_XIAO_EPAPER_DISPLAY_BOARD_EE02
	// Drop EE02 ePaper board enable before deep sleep.
	pinMode(43, OUTPUT);
	digitalWrite(43, LOW);
	#endif
}

uint16_t countImages() {
	uint16_t fileCount = 0;
	File root = SD.open("/");
	File file = root.openNextFile();
	while (file) {
		if (!file.isDirectory()) {
			String filename = file.name();
			if (filename.endsWith(".bin")) {
				fileCount++;
			}
		}
		file = root.openNextFile();
	}
	root.close();
	return fileCount;
}

void writeLogs() {
	// Open info.log for appending
	File logFile = SD.open("/info.log", FILE_APPEND);
	if (logFile) {
		time_t now = time(nullptr);
		struct tm* timeinfo = localtime(&now);
		char timeStr[30];
		strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
		
		float batteryVoltage = readBatteryVoltage();
		uint8_t batteryPercent = batteryVoltageToPercent(batteryVoltage);
		char batteryStr[32];
		snprintf(batteryStr, sizeof(batteryStr), "Battery: %.2f V (%u%%)", batteryVoltage, batteryPercent);
		if (USB) Serial.println(batteryStr);
		
		logFile.printf("[%s] Displayed image: %03d.bin | %s\n", timeStr, imageIndex, batteryStr);

		if (batteryVoltage < BATTERY_EMPTY_VOLTAGE) {
			logFile.printf("[%s] Battery voltage critical: %.2f V, shutting down. Please recharge the battery.\n", timeStr, batteryVoltage);
		}

		logFile.flush();
		logFile.close();
	} else {
		if (USB) Serial.println("Failed to open log file for writing");
	}
}

void setup() {
	// if (digitalRead(19) == LOW) USB = true; // For debugging

	if (USB) Serial.begin(115200);
	if (USB) delay(4000);
	if (USB) Serial.println("13.3\" Colorful E-Paper Bitmap Display Example");
	if (seedRtcFromBuildIfNeeded() && USB) Serial.println("RTC set from build/upload host time");

	analogReadResolution(12);
	pinMode(BATTERY_ADC_PIN, INPUT);
	pinMode(BATTERY_ADC_ENABLE_PIN, OUTPUT);
	digitalWrite(BATTERY_ADC_ENABLE_PIN, LOW);

	if (readBatteryVoltage() < BATTERY_EMPTY_VOLTAGE) {
		if (USB) Serial.println("Battery voltage too low, skipping display update");
		
		if(SD.begin(21)){
			writeLogs();
			SD.end();
		}
		
		prepareForDeepSleep();
		esp_deep_sleep_start(); // Enter deep sleep immediately and don't schedule a wakeup timer
		
	}

	if(!SD.begin(21)){
		if (USB) Serial.println("Card Mount Failed");
		return;
	}

	int imageCount = countImages();
	if (USB) Serial.printf("Number of images on SD card: %d\n", imageCount);

	if (imageCount == 0) {
		if (USB) Serial.println("No .bin image files found on SD card");
		return;
	}

	char filename[20];
	snprintf(filename, sizeof(filename), "/%03d.bin", imageIndex);

	File file = SD.open(filename);
	if(!file){
		if (USB) Serial.println("Failed to open file for reading");
		return;
	}
	size_t imageSize = file.size();

	uint8_t *imgData = (uint8_t *)malloc(imageSize);
	if(!imgData){
		if (USB) Serial.println("Failed to allocate memory for image");
		file.close();
		return;
	}

	file.read(imgData, imageSize);
	file.close();

	if (USB) Serial.printf("Loaded image: %s (%u bytes)\n", filename, static_cast<unsigned>(imageSize));
	
	if (USB) Serial.println("Writing logs...");
	writeLogs();

	SD.end();

	epaper.begin();

	if (imageIndex == 0) {
		// Clear the display on the first image to ensure a clean slate
		if (USB) Serial.println("Clearing display...");
		epaper.fillScreen(TFT_WHITE);
		// epaper.update();
		delay(100);
	}

	if (USB) Serial.println("Updating display...");

	// Draw the image on the e-paper display
	epaper.pushImage(0, 0, 1200, 1600, (uint16_t *)imgData);
	epaper.update();
	epaper.sleep(); // Put display to sleep to save power

	if (USB) Serial.println("Display update complete");
	
	imageIndex = (imageIndex + 1) % imageCount; // Increment index and wrap around if it exceeds file count

	if (USB) Serial.printf("Next image index: %d\n", imageIndex);

	// Put ESP32 into deep sleep with 1 hour wakeup timer
	if (USB) Serial.println("Entering deep sleep for 1 hour...");
	
	prepareForDeepSleep();
	esp_sleep_enable_timer_wakeup(60 * 60 * 1000000);
	// esp_sleep_enable_timer_wakeup(100000); // Debugging: 100ms
	esp_deep_sleep_start();
}

void loop() {
	// Nothing to do here
}