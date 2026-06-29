#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// WiFi
const char* ssid = "iPhone";
const char* password = "poop1234";

WebServer server(80);

// TFT pins
#define TFT_CS   15
#define TFT_DC   12
#define TFT_RST  0
#define TFT_SCLK 14
#define TFT_MOSI 13

#define SCAN_BUTTON 32

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define FLASH_LED 2
#define BATTERY_ADC 33

// Freenove ESP32 WROVER CAM pins
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM       5
#define Y2_GPIO_NUM       4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

String lastScan = "No scan yet";

float readBatteryVoltage() {
  int raw = analogRead(BATTERY_ADC);
  float adcVoltage = (raw / 4095.0) * 3.3;
  return adcVoltage * 2.0; // 100k + 100k divider
}

int batteryPercent() {
  float v = readBatteryVoltage();
  if (v >= 4.20) return 100;
  if (v <= 3.30) return 0;
  return (int)((v - 3.30) * 100.0 / (4.20 - 3.30));
}

void showHome() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("POKEDEX");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 45);
  tft.print("Battery: ");
  tft.print(batteryPercent());
  tft.println("%");

  tft.setCursor(10, 70);
  tft.println("Use IoT page");
  tft.setCursor(10, 85);
  tft.println("to scan card.");
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QQVGA; // 160x120
  config.fb_count = 1;

  return esp_camera_init(&config) == ESP_OK;
}

void scanCard() {
  digitalWrite(FLASH_LED, HIGH);

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.println("Scanning...");

  delay(500);

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    digitalWrite(FLASH_LED, LOW);
    lastScan = "Camera failed";

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 40);
    tft.println("Camera fail");
    return;
  }

  uint16_t *pixels = (uint16_t *)fb->buf;
  int pixelCount = fb->len / 2;

  for (int i = 0; i < pixelCount; i++) {
    pixels[i] = (pixels[i] >> 8) | (pixels[i] << 8); // swap bytes
  }

  tft.drawRGBBitmap(0, 0, pixels, 160, 120);

  tft.fillScreen(ST77XX_BLACK);
  tft.drawRGBBitmap(0, 0, (uint16_t*)fb->buf, 160, 120);

  esp_camera_fb_return(fb);

  digitalWrite(FLASH_LED, LOW);
  lastScan = "Photo captured";
}

void dashboard() {
  String html = "";
  html += "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial;background:#111;color:white;text-align:center;}button{font-size:24px;padding:15px;margin:15px;}</style>";
  html += "</head><body>";
  html += "<h1>Pokedex Demo</h1>";
  html += "<h2>Battery: " + String(batteryPercent()) + "%</h2>";
  html += "<h2>Last Scan: " + lastScan + "</h2>";
  html += "<a href='/scan'><button>Scan Card</button></a>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleScan() {
  scanCard();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  pinMode(SCAN_BUTTON, INPUT_PULLUP);

  pinMode(FLASH_LED, OUTPUT);
  digitalWrite(FLASH_LED, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC, ADC_11db);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  showHome();

  if (!initCamera()) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 40);
    tft.println("Camera Failed");
    Serial.println("Camera Failed");
  } else {
    Serial.println("Camera OK");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Dashboard IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", dashboard);
  server.on("/scan", handleScan);
  server.begin();

  showHome();
}

void loop() {
  server.handleClient();

  static bool lastButtonState = HIGH;

  bool currentState = digitalRead(SCAN_BUTTON);

  if (lastButtonState == HIGH && currentState == LOW) {
    Serial.println("Button Pressed - Scanning");
    scanCard();

    delay(300); // debounce
  }

  lastButtonState = currentState;
}