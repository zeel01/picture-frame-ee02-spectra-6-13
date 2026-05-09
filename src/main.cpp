// #include <Arduino.h>

#include <cstdint>
#include "TFT_eSPI.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

bool USB = false; // Set to true if USB is connected, false otherwise

RTC_DATA_ATTR uint16_t imageIndex = 0;

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

void setup() {
	if (digitalRead(19) == LOW) USB = true;

	if (USB) Serial.begin(115200);
	if (USB) delay(4000);
	if (USB) Serial.println("13.3\" Colorful E-Paper Bitmap Display Example");

	if(!SD.begin(21)){
		if (USB) Serial.println("Card Mount Failed");
		return;
	}

	int imageCount = countImages();
	if (USB) Serial.printf("Number of images on SD card: %d\n", imageCount);

	char filename[20];
	snprintf(filename, sizeof(filename), "/%03d.bin", imageIndex);

	File file = SD.open(filename);
	if(!file){
		if (USB) Serial.println("Failed to open file for reading");
		return;
	}

	uint8_t *imgData = (uint8_t *)malloc(file.size());
	if(!imgData){
		if (USB) Serial.println("Failed to allocate memory for image");
		return;
	}

	file.read(imgData, file.size());
	file.close();
	SD.end(); // Unmount SD card

	epaper.begin();

	if (imageIndex == 0) {
		// Clear the display on the first image to ensure a clean slate
		epaper.fillScreen(TFT_WHITE);
		epaper.update();
		delay(100);
	}

	// Draw the image on the e-paper display
	epaper.pushImage(0, 0, 1200, 1600, (uint16_t *)imgData);

	epaper.update();

	// Put display to sleep to save power
	epaper.sleep();
	
	imageIndex = (imageIndex + 1) % imageCount; // Increment index and wrap around if it exceeds file count

	// Put ESP32 into deep sleep with 10 second wakeup timer
	if (USB) Serial.println("Entering deep sleep for 10 minutes...");
	esp_sleep_enable_timer_wakeup(10 * 60 * 1000000); // Microseconds
	esp_deep_sleep_start();
}

void loop() {
	// Nothing to do here
}