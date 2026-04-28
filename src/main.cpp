// #include <Arduino.h>

// #include "driver.h"
#include <cstdint>
#include "TFT_eSPI.h"
#include "image.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
// #include "image.h"

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

void setup()
{
#ifdef EPAPER_ENABLE
  Serial.begin(115200);
  while(!Serial);
  Serial.println("13.3\" Colorful E-Paper Bitmap Display Example");
  
  if(!SD.begin(21)){
      Serial.println("Card Mount Failed");
      return;
  }

  File file = SD.open("/img.bin");
  if(!file){
      Serial.println("Failed to open file for reading");
      return;
  }


  uint8_t *imgData = (uint8_t *)malloc(file.size());
  if(!imgData){
      Serial.println("Failed to allocate memory for image");
      return;
  }

  file.read(imgData, file.size());
  file.close();

  // Display color bitmap image using pushImage API

  epaper.begin();

  // Clear screen to white
  // epaper.fillScreen(TFT_WHITE);
  // epaper.update();
  // delay(100);
  
  // Display color bitmap image using pushImage API
  // pushImage(x, y, width, height, image_data)
  epaper.pushImage(0, 0, 1200, 1600, (uint16_t *)imgData);
  // epaper.pushImage(0, 0, 1200, 1600, (uint16_t *)gImage_13inch3);

  epaper.update();
  
  // Put display to sleep to save power
  epaper.sleep();
#else
  Serial.begin(115200);
  Serial.println("EPAPER_ENABLE not defined. Please select the correct setup file.");
#endif
}

void loop()
{
  // Nothing to do here
}