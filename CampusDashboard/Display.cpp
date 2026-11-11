#include "Display.h"
#include <WiFi.h>
#include <time.h>

namespace {
constexpr uint16_t Background = 0x0842;
constexpr uint16_t Card = 0x1927;
constexpr uint16_t Accent = 0x5EFF;

// 用 GFX 图形绘制图标，无需额外图片、字体或 SD 卡。
void drawWeatherIcon(Adafruit_ILI9341& screen, int x, int y, int code) {
  screen.fillRect(x - 22, y - 17, 44, 34, Card);
  if (code < 0) return;

  if (code <= 2) screen.fillCircle(x - 5, y - 5, 10, ILI9341_YELLOW);
  if (code == 0) return;
  if (code == 45 || code == 48) {
    for (int offset = -8; offset <= 8; offset += 8) {
      screen.fillRoundRect(x - 17, y + offset, 34, 3, 1, ILI9341_LIGHTGREY);
    }
    return;
  }

  screen.fillCircle(x - 10, y, 7, ILI9341_LIGHTGREY);
  screen.fillCircle(x, y - 5, 9, ILI9341_LIGHTGREY);
  screen.fillCircle(x + 10, y, 7, ILI9341_LIGHTGREY);
  screen.fillRoundRect(x - 15, y, 30, 7, 3, ILI9341_LIGHTGREY);
  if (code >= 95) {
    screen.fillTriangle(x, y + 5, x - 5, y + 12, x + 1, y + 12, ILI9341_YELLOW);
    screen.fillTriangle(x - 1, y + 10, x + 5, y + 10, x - 1, y + 16, ILI9341_YELLOW);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    for (int dx = -8; dx <= 8; dx += 8) {
      screen.fillCircle(x + dx, y + 12, 2, ILI9341_WHITE);
    }
  } else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    for (int dx = -8; dx <= 8; dx += 8) {
      screen.drawLine(x + dx, y + 9, x + dx - 3, y + 15, Accent);
    }
  }
}
String number(float value, unsigned digits = 0) {
  return isnan(value) ? String("--") : String(value, digits);
}

constexpr int PanelX = 54;
constexpr int PanelY = 16;
constexpr int PanelWidth = 212;
constexpr int PanelHeight = 208;
constexpr uint16_t Muted = 0xA69A;

String timeSpan(uint32_t seconds) {
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu",
           static_cast<unsigned long>(seconds / 3600),
           static_cast<unsigned long>(seconds / 60 % 60),
           static_cast<unsigned long>(seconds % 60));
  return buffer;
}
}

void Display::begin() {
  SPI.begin(Config::sck, -1, Config::mosi, Config::cs);
  screen.begin(10000000);
  screen.setRotation(Config::rotation == 3 ? 3 : 1);
  screen.setTextWrap(false);
}

void Display::frame(uint8_t page) {
  struct Tab {
    const char* label;
    int x, y;
  };
  const Tab tabs[] = {
    {"< CLOCK", 2, 56}, {"< NOW", 2, 156},
    {"3-DAY >", 269, 56}, {"NET >", 269, 156}
  };

  screen.fillScreen(Background);
  screen.fillRoundRect(PanelX, PanelY, PanelWidth, PanelHeight, 10, Card);
  screen.drawRoundRect(PanelX, PanelY, PanelWidth, PanelHeight, 10, Accent);
  screen.setTextSize(1);
  for (uint8_t i = 0; i < pageCount; ++i) {
    const auto& tab = tabs[i];
    const bool active = i == page;
    screen.fillRoundRect(tab.x, tab.y, 49, 23, 5, active ? Accent : Card);
    if (!active) screen.drawRoundRect(tab.x, tab.y, 49, 23, 5, Muted);
    screen.setTextColor(active ? Background : ILI9341_WHITE);
    screen.setCursor(tab.x + (49 - strlen(tab.label) * 6) / 2, tab.y + 8);
    screen.print(tab.label);
  }

  // 换页后清空绘制缓存。平时保留卡片与导航，只刷新变化的字段。
  for (auto& text : previous) text = "";
  for (auto& code : previousIcons) code = -2;
}

void Display::field(uint8_t slot, int x, int y, uint8_t size, uint8_t columns,
                    const String& text, uint16_t color, bool centered) {
  String visible = text.substring(0, columns);
  const unsigned width = visible.length();
  if (centered) {
    const unsigned left = (columns - width) / 2;
    for (unsigned i = 0; i < left; ++i) visible = " " + visible;
  }
  while (visible.length() < columns) visible += ' ';
  if (previous[slot] == visible) return;
  previous[slot] = visible;
  screen.setCursor(x, y);
  screen.setTextSize(size);
  screen.setTextColor(color, Card);
  screen.print(visible);
}

void Display::icon(uint8_t slot, int x, int y, int code) {
  if (previousIcons[slot] == code) return;
  previousIcons[slot] = code;
  drawWeatherIcon(screen, x, y, code);
}

void Display::show(uint8_t page, const WeatherData& w, bool newPage) {
  if (newPage) frame(page);

  switch (page) {
    case 0: {
      tm local;
      char clock[9] = "--:--:--";
      char date[24] = "Waiting for NTP";
      if (getLocalTime(&local, 10)) {
        strftime(clock, sizeof(clock), "%H:%M:%S", &local);
        strftime(date, sizeof(date), "%a %d %b %Y", &local);
      }
      field(0, 64, 65, 4, 8, clock);
      if (newPage) screen.drawFastHLine(77, 111, 166, Accent);
      field(1, 64, 126, 2, 16, date, ILI9341_WHITE, true);
      field(2, 64, 153, 2, 16, "MYT UTC+8", Accent, true);
      field(3, 64, 182, 2, 16, Config::location, Muted, true);
      break;
    }
    case 1:
      if (!w.available) {
        field(0, 64, 36, 1, 32, "Fetching weather...", Muted, true);
        break;
      }
      field(0, 64, 36, 1, 32, Config::location, Muted, true);
      icon(0, 160, 64, w.code);
      field(1, 70, 84, 3, 10, number(w.temp, 1) + " C", ILI9341_WHITE, true);
      field(2, 64, 110, 2, 16, "Feels " + number(w.feels, 1) + " C", Accent, true);
      field(3, 70, 137, 2, 15, "Humidity " + number(w.humidity) + "%");
      field(4, 70, 155, 2, 15, "Wind " + number(w.wind) + " km/h");
      field(5, 70, 173, 2, 15, "Press " + number(w.pressure) + " hPa");
      field(6, 64, 194, 1, 32, "Gust " + number(w.gust) + "km/h  Rain " + number(w.rain, 1) + "mm", Muted);
      field(7, 64, 207, 1, 32, String("Open-Meteo ") + w.updated + (w.stale ? " cached" : ""), Muted);
      break;
    case 2:
      if (!w.available) {
        field(0, 64, 36, 1, 32, "Fetching weather...", Muted, true);
        break;
      }
      field(0, 64, 36, 1, 32, "DATE          HI/LO C   RAIN", Accent);
      for (uint8_t i = 0; i < 3; ++i) {
        const int y = 51 + i * 41;
        if (previousIcons[i] == -2) screen.drawRoundRect(62, y, 196, 39, 5, Muted);
        icon(i, 118, y + 19, w.dailyCode[i]);
        field(1 + i * 3, 67, y + 15, 1, 5, i == 0 ? String("Today") : String(w.dates[i]).substring(5));
        field(2 + i * 3, 145, y + 13, 2, 6, number(w.high[i]) + "/" + number(w.low[i]));
        field(3 + i * 3, 225, y + 15, 1, 5, number(w.chance[i]) + "%", Accent);
      }
      field(10, 64, 183, 1, 32, String("Sunrise ") + w.sunrise + "   Sunset " + w.sunset, ILI9341_WHITE, true);
      field(11, 64, 204, 1, 32, w.stale ? "Open-Meteo | cached" : "Open-Meteo", Muted, true);
      break;
    case 3: {
      const bool online = WiFi.status() == WL_CONNECTED;
      field(0, 64, 43, 2, 16, online ? "WiFi Connected" : "WiFi Offline", Accent, true);
      if (newPage) screen.drawFastHLine(72, 69, 176, Accent);
      field(1, 64, 82, 1, 32, online ? String("SSID: ") + WiFi.SSID() : "SSID: --", Muted);
      field(2, 64, 101, 1, 32, online ? String("IP: ") + WiFi.localIP().toString() : "IP: --");
      field(3, 64, 124, 2, 16, online ? String("RSSI ") + WiFi.RSSI() + " dBm" : "RSSI --");
      field(4, 64, 152, 2, 16, "UP " + timeSpan(millis() / 1000));
      field(5, 64, 180, 2, 16, String("Heap ") + ESP.getFreeHeap() / 1024 + " KiB");
      break;
    }
  }
}
