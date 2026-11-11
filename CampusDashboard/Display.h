#pragma once
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include "Config.h"
#include "Weather.h"

class Display {
 public:
  static constexpr uint8_t pageCount = 4;
  void begin();
  void show(uint8_t page, const WeatherData& weather, bool newPage);

 private:
  void frame(uint8_t page);
  void field(uint8_t slot, int x, int y, uint8_t size, uint8_t columns,
             const String& text, uint16_t color = ILI9341_WHITE, bool centered = false);
  void icon(uint8_t slot, int x, int y, int code);
  Adafruit_ILI9341 screen{&SPI, Config::dc, Config::cs, Config::reset};
  String previous[12];
  int previousIcons[3] = {-2, -2, -2};
};
