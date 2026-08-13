#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <WiFiManager.h>
#include "time.h"

// Extra 16-bit RGB565 Colors
#define ST77XX_GRAY     0x8410
#define ST77XX_DARKGRAY 0x4208
#define ST77XX_NAVY     0x000F

// ==================== PINS ====================
#define TFT_MOSI        35
#define TFT_SCLK        36
#define TFT_CS           7
#define TFT_DC          39
#define TFT_RST         40
#define TFT_BL          45

#define TOUCH_PIN       5            // TTP223 Touch Pad
#define RGB_LED_PIN     33           // RGB LED Pin
#define SDA_PIN         42           // BMP280 SDA
#define SCL_PIN         41           // BMP280 SCL
#define BAT_ADC_PIN      4           // Battery Sense Pin

// ==================== SETTINGS ====================
String openWeatherMapApiKey = "YOUR_OPENWEATHER_API_KEY";

// Talk to your AI assistant to figure out the city and country code settings here, or search on the internet
String city                 = "None";
String countryCode          = "None";

#define TEMP_OFFSET_C   26.5

// Timezone set: UTC+0. Talk to your AI assistant to determine the required offset valeus for your region.
const long  gmtOffset_sec      = 0; 
const int   daylightOffset_sec = 0;

// Default auto-sleep timeout (10 minutes = 600,000 ms)
const unsigned long AUTO_SLEEP_TIMEOUT_MS = 600000; 

// Default weather fetch interval (10 minutes)
const unsigned long WEATHER_FETCH_INTERVAL = 600000; 
// ==================================================



// ==================== OBJECTS ====================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Adafruit_BMP280 bmp;
Adafruit_NeoPixel rgbLed(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiManager wm;

// 3-Screen Navigation Cycle
enum DisplayMode { SCREEN_HOME,
                   SCREEN_SETUP,
                   SCREEN_FLASHLIGHT };
DisplayMode currentScreen = SCREEN_HOME;

bool flashlightOn = false;

bool bmpConnected = false;
bool isHotspotActive = false;
float outsideTemp = 0.0;
float bmpTemp = 0.0;
float pressurehPa = 0.0;
float altitudeMeters = 0.0;
String currentTimeStr = "--:--";

// Timers and touch tracking
unsigned long touchStartTime = 0;
unsigned long lastActivityTime = 0;  // Tracks inactivity for auto-sleep
unsigned long lastWeatherFetch = 0;  // Tracks weather API interval
bool isTouching = false;
const unsigned long HOLD_THRESHOLD_MS = 1500;

// ==================== HELPER FUNCTIONS ====================

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

void setBacklightBrightness(uint8_t brightness) {
  analogWrite(TFT_BL, brightness);
}

int readBatteryPercentage() {
  uint32_t raw = analogRead(BAT_ADC_PIN);
  float voltage = (raw / 4095.0) * 3.3 * 2.0;
  if (voltage >= 4.2) return 100;
  if (voltage <= 3.3) return 0;
  int pct = (int)((voltage - 3.3) / (4.2 - 3.3) * 100.0);
  return constrain(pct, 0, 100);
}

bool updateTimeAndBrightness() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100)) {
    char timeBuff[6];
    strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo);
    currentTimeStr = String(timeBuff);

    // Nighttime auto-dimming (8 PM to 6 AM)
    if (timeinfo.tm_hour >= 20 || timeinfo.tm_hour < 6) {
      setBacklightBrightness(30);
    } else {
      setBacklightBrightness(200);
    }
    return true;
  }
  return false;
}

void fetchOutsideWeather() {
  if (WiFi.status() == WL_CONNECTED && openWeatherMapApiKey != "YOUR_OPENWEATHER_API_KEY") {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&units=metric&appid=" + openWeatherMapApiKey;

    http.setTimeout(800);
    http.begin(url);
    if (http.GET() == HTTP_CODE_OK) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, http.getString());
      outsideTemp = doc["main"]["temp"];
    }
    http.end();
  }
}

void readBMP280() {
  if (bmpConnected) {
    bmp.takeForcedMeasurement();
    bmpTemp = bmp.readTemperature() - TEMP_OFFSET_C;
    pressurehPa = bmp.readPressure() / 100.0F;
    altitudeMeters = bmp.readAltitude(1013.25);
  }
}

// ==================== UI DRAWING ====================

void initHomeScreenLayout() {
  tft.fillScreen(ST77XX_BLACK);
}

void updateHomeScreenValues() {
  // 1. CENTER CLOCK (Text Size 5)
  tft.setTextSize(5);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(45, 48);
  tft.printf("%-5s", currentTimeStr.c_str());

  tft.setTextSize(2);

  // 2. TOP-LEFT: Outside Temperature
  tft.setCursor(5, 5);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  if (openWeatherMapApiKey == "YOUR_OPENWEATHER_API_KEY") {
    tft.print("OUT: NoKey");
  } else if (WiFi.status() != WL_CONNECTED) {
    tft.print("OUT: NoWiFi");
  } else {
    tft.printf("OUT:%.1fC ", outsideTemp);
  }

  // 3. TOP-RIGHT: BMP Indoor Temperature
  tft.setCursor(125, 5);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  if (!bmpConnected) {
    tft.print("IN: Err  ");
  } else {
    tft.printf("IN:%.1fC ", bmpTemp);
  }

  // 4. BOTTOM-LEFT: Pressure
  tft.setCursor(5, 115);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  if (!bmpConnected) {
    tft.print("N/A    ");
  } else {
    tft.printf("%.0fhPa ", pressurehPa);
  }

  // 5. BOTTOM-RIGHT: Altitude
  tft.setCursor(140, 115);
  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
  if (!bmpConnected) {
    tft.print("N/A   ");
  } else {
    tft.printf("ALT:%.0fm", altitudeMeters);
  }

  // 6. BATTERY PERCENTAGE
  tft.setCursor(78, 26);
  tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
  tft.printf("BAT:%d%% ", readBatteryPercentage());
}

void drawSetupScreen() {
  tft.fillScreen(ST77XX_NAVY);
  tft.setTextColor(ST77XX_WHITE, ST77XX_NAVY);

  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("WIFI & SETUP");

  tft.setTextSize(1);
  tft.setCursor(10, 40);

  if (isHotspotActive) {
    tft.setTextColor(ST77XX_GREEN, ST77XX_NAVY);
    tft.println("HOTSPOT ACTIVE!");
    tft.println("\n1. Connect phone to WiFi:");
    tft.setTextColor(ST77XX_YELLOW, ST77XX_NAVY);
    tft.println("   SSID: WeatherStation-AP");
    tft.setTextColor(ST77XX_GREEN, ST77XX_NAVY);
    tft.println("\n2. Open browser at:");
    tft.setTextColor(ST77XX_YELLOW, ST77XX_NAVY);
    tft.println("   IP:   192.168.4.1");

    tft.setTextColor(ST77XX_CYAN, ST77XX_NAVY);
    tft.setCursor(10, 115);
    tft.println("[TAP TOUCH] -> Exit Hotspot");
  } else {
    tft.setTextColor(ST77XX_CYAN, ST77XX_NAVY);
    if (WiFi.status() == WL_CONNECTED) {
      tft.printf("Connected: %s\n", WiFi.SSID().c_str());
      tft.printf("IP:        %s\n", WiFi.localIP().toString().c_str());
      tft.printf("Signal:    %d dBm\n", WiFi.RSSI());
    } else {
      tft.setTextColor(ST77XX_RED, ST77XX_NAVY);
      tft.println("Status: Disconnected");
    }

    tft.setTextColor(ST77XX_YELLOW, ST77XX_NAVY);
    tft.setCursor(10, 105);
    tft.println("[HOLD TOUCH 1.5s] -> Launch AP");
  }
}

void drawFlashlightScreen() {
  tft.fillScreen(ST77XX_BLACK);

  // 1. Center Title: FLASHLIGHT
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(30, 30);
  tft.print("FLASHLIGHT");

  // 2. Status Text: ON / OFF
  if (flashlightOn) {
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setCursor(102, 75);
    tft.print("ON ");
  } else {
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setCursor(93, 75);
    tft.print("OFF");
  }

  // 3. Instruction Hint
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
  tft.setCursor(25, 115);
  tft.print("[HOLD TOUCH] -> Power ON/OFF");
}

void toggleFlashlight() {
  flashlightOn = !flashlightOn;

  if (flashlightOn) {
    rgbLed.setBrightness(204);                             // 80% Brightness
    rgbLed.setPixelColor(0, rgbLed.Color(255, 255, 255));  // Bright White
    rgbLed.show();
  } else {
    setLedColor(0, 0, 0);       // Turn OFF
    rgbLed.setBrightness(128);  // Reset to default
  }

  // Refresh display immediately if currently on Flashlight menu
  if (currentScreen == SCREEN_FLASHLIGHT) {
    drawFlashlightScreen();
  }
}

// ==================== DEEP SLEEP & PORTAL ====================

void enterDeepSleep() {
  setLedColor(255, 0, 0);  // Flash Red on sleep
  setBacklightBrightness(0);
  tft.fillScreen(ST77XX_BLACK);

  // Wait until finger is lifted off touch pad
  unsigned long timeout = millis();
  while (digitalRead(TOUCH_PIN) == HIGH) {
    delay(10);
    if (millis() - timeout > 3000) break;
  }

  setLedColor(0, 0, 0);
  delay(50);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_PIN, 1);
  esp_deep_sleep_start();
}

void startSetupPortal() {
  isHotspotActive = true;
  setLedColor(0, 0, 255);  // Blue LED during AP Hotspot mode

  wm.setConfigPortalTimeout(180);
  wm.startConfigPortal("WeatherStation-AP");  // Starts non-blocking hotspot
  drawSetupScreen();
}

void stopSetupPortal() {
  if (isHotspotActive) {
    wm.stopConfigPortal();
    isHotspotActive = false;
  }
  setLedColor(0, 0, 0);
  currentScreen = SCREEN_HOME;
  initHomeScreenLayout();
  updateHomeScreenValues();
  lastActivityTime = millis();
}

// ==================== SETUP & LOOP ====================

void setup() {
  Serial.begin(115200);

  // 1. Hardware Init
  rgbLed.begin();
  rgbLed.setBrightness(128);
  setLedColor(0, 0, 0);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(TFT_BL, OUTPUT);
  setBacklightBrightness(200);

  // 2. Initialize Display
  tft.init(135, 240);
  tft.setRotation(1);

  // 3. SET TIMEZONE IMMEDIATELY (Fixes UTC+0 after Deep Sleep wakeup)
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.google.com", "time.nist.gov");

  // 4. BMP280 Sensor Check
  Wire.begin(SDA_PIN, SCL_PIN);
  bmpConnected = bmp.begin(0x76) || bmp.begin(0x77);

  if (bmpConnected) {
    bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                    Adafruit_BMP280::SAMPLING_X1,
                    Adafruit_BMP280::SAMPLING_X1,
                    Adafruit_BMP280::FILTER_OFF,
                    Adafruit_BMP280::STANDBY_MS_4000);
  }

  // 5. DRAW SCREEN IMMEDIATELY (Reads internal RTC with UTC+5 instantly)
  readBMP280();
  updateTimeAndBrightness();
  initHomeScreenLayout();
  updateHomeScreenValues();

  // 6. Connect Wi-Fi (Non-blocking portal mode)
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  wm.setEnableConfigPortal(false);
  wm.setConnectTimeout(5);

  if (wm.autoConnect("WeatherStation-AP")) {
    fetchOutsideWeather();
    lastWeatherFetch = millis();
  }

  lastActivityTime = millis();
}

void loop() {
  // Process non-blocking WiFiManager background tasks
  wm.process();

  // If AP Portal timed out on its own, return to Home Screen
  if (isHotspotActive && !wm.getConfigPortalActive()) {
    stopSetupPortal();
  }

  static unsigned long lastSensorUpdate = 0;
  static unsigned long lastTimeCheck = 0;
  unsigned long now = millis();

  // 1. FAST TIME CHECK (Every 500ms, non-blocking)
  if (now - lastTimeCheck >= 500) {
    lastTimeCheck = now;
    if (updateTimeAndBrightness() && currentScreen == SCREEN_HOME) {
      tft.setTextSize(5);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(45, 48);
      tft.printf("%-5s", currentTimeStr.c_str());
    }
  }

  // 2. SENSOR & WEATHER REFRESH (Indoor every 5s, Outdoor every 10 min)
  if (now - lastSensorUpdate >= 5000) {
    lastSensorUpdate = now;
    readBMP280();

    if (now - lastWeatherFetch >= WEATHER_FETCH_INTERVAL || lastWeatherFetch == 0) {
      if (WiFi.status() == WL_CONNECTED) {
        fetchOutsideWeather();
        lastWeatherFetch = now;
      }
    }

    if (currentScreen == SCREEN_HOME) {
      updateHomeScreenValues();
    }
  }

  // 3. AUTO-SLEEP CHECK
  if (now - lastActivityTime >= AUTO_SLEEP_TIMEOUT_MS) {
    enterDeepSleep();
  }

  // 4. скщзROLS
  bool rawTouch = (digitalRead(TOUCH_PIN) == HIGH);

  if (rawTouch && !isTouching) {
    isTouching = true;
    touchStartTime = now;
    lastActivityTime = now;
  } else if (rawTouch && isTouching) {
    // 1.5s Hold Check
    if (now - touchStartTime >= HOLD_THRESHOLD_MS) {
      isTouching = false;

      if (currentScreen == SCREEN_HOME) {
        enterDeepSleep();
      } else if (currentScreen == SCREEN_SETUP) {
        if (isHotspotActive) {
          stopSetupPortal();
        } else {
          startSetupPortal();
        }
      } else if (currentScreen == SCREEN_FLASHLIGHT) {
        toggleFlashlight();  // HOLD TOGGLES LIGHT ON/OFF
      }
    }
  } else if (!rawTouch && isTouching) {
    // Touch Released
    unsigned long pressDuration = now - touchStartTime;
    isTouching = false;
    lastActivityTime = now;

    // Short Tap (<1.5s): Cycle 1 -> 2 -> 3 -> 1
    if (pressDuration >= 20 && pressDuration < HOLD_THRESHOLD_MS) {
      if (isHotspotActive) {
        stopSetupPortal();
      } else if (currentScreen == SCREEN_HOME) {
        currentScreen = SCREEN_SETUP;
        drawSetupScreen();
      } else if (currentScreen == SCREEN_SETUP) {
        currentScreen = SCREEN_FLASHLIGHT;
        drawFlashlightScreen();
      } else if (currentScreen == SCREEN_FLASHLIGHT) {
        currentScreen = SCREEN_HOME;
        initHomeScreenLayout();
        updateHomeScreenValues();
      }
    }
  }

  delay(10);
}