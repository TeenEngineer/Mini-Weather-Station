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
String openWeatherMapApiKey = "0e352cd48dca08dc9f3bc11f5fe08bd2"; 
String city                 = "Tashkent";
String countryCode          = "UZ";

#define TEMP_OFFSET_C   15.5          

// Timezone: Philadelphia / US Eastern Time (Automatic DST)
const char* ntpServer = "pool.ntp.org";
const char* TZ_INFO   = "EST5EDT,M3.2.0,M11.1.0";

// Auto-sleep timeout (10 minutes = 600,000 ms)
const unsigned long AUTO_SLEEP_TIMEOUT_MS = 600000; 

// ==================== OBJECTS ====================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Adafruit_BMP280 bmp;
Adafruit_NeoPixel rgbLed(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiManager wm;

enum DisplayMode { SCREEN_HOME, SCREEN_SETUP };
DisplayMode currentScreen = SCREEN_HOME;

bool bmpConnected     = false;
bool isHotspotActive  = false;
float outsideTemp     = 0.0;
float bmpTemp         = 0.0;
float pressurehPa     = 0.0;
float altitudeMeters  = 0.0;
String currentTimeStr = "--:--";

// Timers and touch tracking
unsigned long touchStartTime    = 0;
unsigned long lastActivityTime  = 0;  // Tracks inactivity for auto-sleep
bool isTouching                 = false;
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
  float voltage = (raw / 4095.0) * 3.3 * 2.0; // Resistor divider compensation
  if (voltage >= 4.2) return 100;
  if (voltage <= 3.3) return 0;
  int pct = (int)((voltage - 3.3) / (4.2 - 3.3) * 100.0);
  return constrain(pct, 0, 100);
}

void updateTimeAndBrightness() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeBuff[6];
    strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo);
    currentTimeStr = String(timeBuff);

    // Nighttime auto-dimming (8 PM to 6 AM)
    if (timeinfo.tm_hour >= 20 || timeinfo.tm_hour < 6) {
      setBacklightBrightness(30);  
    } else {
      setBacklightBrightness(200); 
    }
  }
}

void fetchOutsideWeather() {
  if (WiFi.status() == WL_CONNECTED && openWeatherMapApiKey != "YOUR_OPENWEATHER_API_KEY") {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&units=metric&appid=" + openWeatherMapApiKey;
    
    http.setTimeout(1500); 
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
    bmp.takeForcedMeasurement(); // Take a quick single reading, then sleep
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

  // 5. BOTTOM-RIGHT: Battery Percentage
  tft.setCursor(140, 115);
  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
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

// ==================== DEEP SLEEP & PORTAL ====================

void enterDeepSleep() {
  setLedColor(255, 0, 0); // Flash Red on sleep
  setBacklightBrightness(0);
  tft.fillScreen(ST77XX_BLACK);

  // Wait until finger is lifted off touch pad
  unsigned long timeout = millis();
  while (digitalRead(TOUCH_PIN) == HIGH) { 
    delay(10); 
    if (millis() - timeout > 3000) break;
  }

  setLedColor(0, 0, 0); // LED off
  delay(50);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_PIN, 1);
  esp_deep_sleep_start();
}

void startSetupPortal() {
  isHotspotActive = true;
  drawSetupScreen();
  setLedColor(0, 0, 255); // Blue LED during AP Hotspot mode

  wm.setConfigPortalTimeout(180); 
  wm.startConfigPortal("WeatherStation-AP");

  isHotspotActive = false;
  setLedColor(0, 0, 0); // Turn off LED after leaving AP portal
  currentScreen = SCREEN_HOME;
  initHomeScreenLayout();
  lastActivityTime = millis(); // Reset auto-sleep timer
}

// ==================== SETUP & LOOP ====================

void setup() {
  Serial.begin(115200);
  delay(300);

  // Init RGB LED (OFF by default)
  rgbLed.begin();
  rgbLed.setBrightness(128); 
  setLedColor(0, 0, 0); 

  pinMode(TOUCH_PIN, INPUT); 
  pinMode(TFT_BL, OUTPUT);
  setBacklightBrightness(200); 

  tft.init(135, 240);
  tft.setRotation(1); 
  
  // BMP280 Sensor Check (Forced Mode to prevent internal heat)
  Wire.begin(SDA_PIN, SCL_PIN);
  bmpConnected = bmp.begin(0x76) || bmp.begin(0x77);

  if (bmpConnected) {
    bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                    Adafruit_BMP280::SAMPLING_X1,
                    Adafruit_BMP280::SAMPLING_X1,
                    Adafruit_BMP280::FILTER_OFF,
                    Adafruit_BMP280::STANDBY_MS_4000);
  }

  // WiFi Connection
  WiFi.mode(WIFI_STA);
  wm.setConnectTimeout(10);
  wm.autoConnect("WeatherStation-AP");

  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(TZ_INFO, ntpServer); // Philadelphia timezone
  }

  initHomeScreenLayout();
  lastActivityTime = millis(); // Start auto-sleep countdown
}

void loop() {
  static unsigned long lastUpdate = 0;

  // Refresh readings every 5 seconds
  if (millis() - lastUpdate > 5000 || lastUpdate == 0) {
    lastUpdate = millis();
    readBMP280();
    updateTimeAndBrightness();
    fetchOutsideWeather();

    if (currentScreen == SCREEN_HOME) {
      updateHomeScreenValues();
    }
  }

  // ================= AUTO-SLEEP CHECK =================
  if (millis() - lastActivityTime >= AUTO_SLEEP_TIMEOUT_MS) {
    enterDeepSleep();
  }

  // ================= TOUCH CONTROLS =================
  bool rawTouch = (digitalRead(TOUCH_PIN) == HIGH);

  if (rawTouch && !isTouching) {
    isTouching = true;
    touchStartTime = millis();
    lastActivityTime = millis(); // Reset 10-min timer on touch
  } 
  else if (rawTouch && isTouching) {
    // 1.5s Hold Check
    if (millis() - touchStartTime >= HOLD_THRESHOLD_MS) {
      isTouching = false; 
      
      if (currentScreen == SCREEN_HOME) {
        enterDeepSleep(); 
      } else if (currentScreen == SCREEN_SETUP) {
        startSetupPortal(); 
      }
    }
  } 
  else if (!rawTouch && isTouching) {
    // Touch Released
    unsigned long pressDuration = millis() - touchStartTime;
    isTouching = false;
    lastActivityTime = millis(); // Reset 10-min timer on release

    // Short Tap (<1.5s): Toggle Screen Mode
    if (pressDuration >= 20 && pressDuration < HOLD_THRESHOLD_MS) {
      if (currentScreen == SCREEN_HOME) {
        currentScreen = SCREEN_SETUP;
        drawSetupScreen();
      } else {
        currentScreen = SCREEN_HOME;
        initHomeScreenLayout();
        updateHomeScreenValues();
      }
    }
  }

  delay(10);
}