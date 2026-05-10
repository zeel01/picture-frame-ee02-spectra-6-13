# Picture Frame - XIAO EE02 Spectra 6 13.3" Color E-Paper Display

This project is a simple offline, low-power picture frame created using the Seed Studio XIAO EE02 display board with a 13.3" Spectra E-Paper display. 

## Converting Images

Use `convert_images.py` to convert images to binary files. Use images that are in portrait orientation with a 3:4 aspect ratio. The screen resolution is 1200x1600, so prefer images of that size or larger. Images that are bright, high-contrast, and vibrant will look best.

```
python convert_images.py <input_folder> <output_folder>
```

## Behavior

The device will wake up, read a binary file, draw it to the screen, write a log file, then go to sleep. A counter will increment after each image, and the next file will be shown. Image files will be named `000.bin`, `001.bin`, `002.bin`, etc. When the counter exceeds the number of files, it will wrap back to the beginning.

There is a 1 hour wakeup timer, so a new image will display every hour.

## Power Consumption

The device tries to use minimum power, and uses deep sleep so that the micro controller is not wasting power between updates. The nature of the E-Paper display means that the display does not require power to maintain the image, and if the battery dies the image will simply remain.

From a small sample of measurements, I roughly estimate that the battery will last around 1000 refresh cycles before it needs charged. This is a little over a month of estimated runtime.