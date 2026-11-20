#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "web_page.h"

// ==================== LED INDICATOR PIN ====================
// GPIO 2 has the built-in blue LED on ESP32 DevKit boards,
// and you can also connect your external LED to GPIO 2 + 200 ohm resistor to GND!
constexpr int PIN_LED = 2;

// ==================== CAPTIVE PORTAL & WIFI AP ====================
const char* AP_SSID = "UAV-Energy-Monitor";
const char* AP_PASS = "12345678";

const byte DNS_PORT = 53;
DNSServer dnsServer;

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ==================== OLED CONFIGURATION ====================
constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 64;
constexpr int OLED_RESET = -1;
constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr int I2C_SDA_PIN = 21;
constexpr int I2C_SCL_PIN = 22;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// ==================== ADC PIN ASSIGNMENTS (ADC1 Only) ====================
constexpr int PIN_CELL1   = 34; // Tap 1 (Cell 1: 0 - 4.2V)
constexpr int PIN_CELL2   = 35; // Tap 2 (Cell 1+2: 0 - 8.4V)
constexpr int PIN_CELL3   = 36; // Tap 3 (Total pack: 0 - 12.6V) -> VP
constexpr int PIN_CHARGER = 33; // Harvesting Input (0.2V - 6.0V+)

// ==================== RESISTOR DIVIDERS (OHMS) ====================
constexpr float R1_TOP    = 12000.0f;
constexpr float R1_BOTTOM = 12000.0f; // Ratio: 2.000

constexpr float R2_TOP    = 50000.0f;
constexpr float R2_BOTTOM = 12000.0f; // Ratio: 5.167

constexpr float R3_TOP    = 100000.0f;
constexpr float R3_BOTTOM = 12000.0f; // Ratio: 9.333

constexpr float R_CHG_TOP    = 50000.0f;
constexpr float R_CHG_BOTTOM = 12000.0f; // Ratio: 5.167

constexpr float RATIO_1   = (R1_TOP + R1_BOTTOM) / R1_BOTTOM;
constexpr float RATIO_2   = (R2_TOP + R2_BOTTOM) / R2_BOTTOM;
constexpr float RATIO_3   = (R3_TOP + R3_BOTTOM) / R3_BOTTOM;
constexpr float RATIO_CHG = (R_CHG_TOP + R_CHG_BOTTOM) / R_CHG_BOTTOM;

// ==================== HYSTERESIS & DEBOUNCE BUFFER ====================
// Starts charging at 0.20V (Level 1x) and drops off below 0.12V
constexpr float CHG_ON_THRESHOLD_V  = 0.20f; // Must rise above 0.20V to start charging
constexpr float CHG_OFF_THRESHOLD_V = 0.12f; // Must fall below 0.12V to exit charging
constexpr unsigned long DWELL_MIN_MS = 1500;  // Must hold state for at least 1.5s

float vChgSmoothed = 0.0f;
bool isChargingStable = false;
unsigned long lastStateChangeMs = 0;

// Global telemetry variables
float g_cell1 = 0.0f;
float g_cell2 = 0.0f;
float g_cell3 = 0.0f;
float g_total = 0.0f;
float g_vChg  = 0.0f;
bool g_batteryConnected = false;
bool g_tap3Connected    = false;
const char* g_multStr   = "x0";
const char* g_labelStr  = "IDLE";
int g_strengthPercent   = 0;
char g_chargeTimeStr[16] = "--";

int animTick = 0;

// ==================== ESTIMATED TIME TO FULL CHARGE (1h to 3h) ====================
void calculateChargeTime(int pct) {
  if (!isChargingStable || !g_batteryConnected) {
    strcpy(g_chargeTimeStr, "--");
    return;
  }

  float remainingPct = 100.0f - pct;
  if (remainingPct < 4.0f) remainingPct = 4.0f;

  // Speed factor based on multiplier (x1: ~2.8h, x4: ~2.0h, x10: ~1.1h)
  float speedFactor = 1.0f;
  if (strcmp(g_multStr, "x1") == 0) speedFactor = 0.85f;
  else if (strcmp(g_multStr, "x2") == 0) speedFactor = 1.05f;
  else if (strcmp(g_multStr, "x4") == 0) speedFactor = 1.40f;
  else if (strcmp(g_multStr, "x6") == 0) speedFactor = 1.80f;
  else if (strcmp(g_multStr, "x8") == 0) speedFactor = 2.25f;
  else if (strcmp(g_multStr, "x10") == 0 || strcmp(g_multStr, "x12") == 0) speedFactor = 2.85f;

  // Estimate strictly between 1.0h and 3.0h
  float estHours = (remainingPct / 100.0f) * (2.8f / speedFactor) + 0.9f;
  if (estHours < 1.0f) estHours = 1.0f;
  if (estHours > 3.0f) estHours = 3.0f;

  int h = static_cast<int>(estHours);
  int m = static_cast<int>((estHours - h) * 60.0f);
  snprintf(g_chargeTimeStr, sizeof(g_chargeTimeStr), "%dh %02dm", h, m);
}

// ==================== WEB SERVER HANDLERS ====================
void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleData() {
  int pct = (g_cell1 > 2.0f) ? (static_cast<int>((g_cell1 - 3.2f) / 1.0f * 100.0f)) : 0;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  calculateChargeTime(pct);

  String json = "{";
  json += "\"battConnected\":" + String(g_batteryConnected ? "true" : "false") + ",";
  json += "\"tap3Ok\":" + String(g_tap3Connected ? "true" : "false") + ",";
  json += "\"c1\":" + String(g_cell1, 2) + ",";
  json += "\"c2\":" + String(g_cell2, 2) + ",";
  json += "\"c3\":" + String(g_cell3, 2) + ",";
  json += "\"total\":" + String(g_total, 2) + ",";
  json += "\"pct\":" + String(pct) + ",";
  json += "\"isCharging\":" + String(isChargingStable ? "true" : "false") + ",";
  json += "\"vChg\":" + String(g_vChg, 2) + ",";
  json += "\"multStr\":\"" + String(g_multStr) + "\",";
  json += "\"labelStr\":\"" + String(g_labelStr) + "\",";
  json += "\"strengthPct\":" + String(g_strengthPercent) + ",";
  json += "\"chargeTime\":\"" + String(g_chargeTimeStr) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

float readVoltage(int pin, float ratio, uint32_t &rawMvOut, uint32_t noiseFloorMv = 120) {
  constexpr int SAMPLES = 32;
  uint32_t sumMv = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sumMv += analogReadMilliVolts(pin);
    delayMicroseconds(100);
  }
  rawMvOut = sumMv / SAMPLES;
  if (rawMvOut <= noiseFloorMv) return 0.0f;
  return (static_cast<float>(rawMvOut) / 1000.0f) * ratio;
}

int voltageToPercent(float v) {
  if (v <= 3.20f) return 0;
  if (v >= 4.20f) return 100;
  return static_cast<int>((v - 3.20f) / (4.20f - 3.20f) * 100.0f);
}

// Multipliers: x1 (for 0.2V - 1.0V), then clean multiples of 2: x2, x4, x6, x8, x10, x12
void updateChargingMetrics(float vChg, bool chargingActive) {
  if (!chargingActive) {
    g_multStr = "x0";
    g_labelStr = "NO INPUT";
    g_strengthPercent = 0;
    return;
  }

  if (vChg < 1.00f) {
    // 0.2V to 1.0V: x1 Low Energy Harvest
    g_multStr = "x1";
    g_labelStr = "HARVEST x1";
    g_strengthPercent = 20;
  } else if (vChg < 1.80f) {
    g_multStr = "x2";
    g_labelStr = "HARVEST x2";
    g_strengthPercent = 38;
  } else if (vChg < 2.60f) {
    g_multStr = "x4";
    g_labelStr = "HARVEST x4";
    g_strengthPercent = 55;
  } else if (vChg < 3.50f) {
    g_multStr = "x6";
    g_labelStr = "HARVEST x6";
    g_strengthPercent = 75;
  } else if (vChg < 4.40f) {
    g_multStr = "x8";
    g_labelStr = "HARVEST x8";
    g_strengthPercent = 88;
  } else if (vChg < 5.20f) {
    g_multStr = "x10";
    g_labelStr = "HARVEST x10";
    g_strengthPercent = 95;
  } else {
    g_multStr = "x12";
    g_labelStr = "TURBO x12";
    g_strengthPercent = 100;
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(200);

  // Setup LED Pin
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_CHARGER, INPUT_PULLDOWN);

  analogSetPinAttenuation(PIN_CELL1, ADC_11db);
  analogSetPinAttenuation(PIN_CELL2, ADC_11db);
  analogSetPinAttenuation(PIN_CELL3, ADC_11db);
  analogSetPinAttenuation(PIN_CHARGER, ADC_11db);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS);

  dnsServer.start(DNS_PORT, "*", local_IP);

  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.on("/generate_204", handleRoot);
  server.on("/gen_204", handleRoot);
  server.on("/hotspot-detect.html", handleRoot);
  server.on("/ncsi.txt", handleRoot);
  server.on("/canonical.html", handleRoot);

  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();

  Serial.println("\n==================================");
  Serial.println("UAV Energy Harvesting System");
  Serial.println("Brownout Protection: ACTIVE");
  Serial.println("Harvesting Input: 0.2V - 6.0V+");
  Serial.printf("Wi-Fi SSID: %s\n", AP_SSID);
  Serial.printf("Wi-Fi Pass: %s\n", AP_PASS);
  Serial.println("Open Phone Browser: http://192.168.4.1");
  Serial.println("==================================\n");

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("CAPTIVE PORTAL ON");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 15);
  display.printf("Wi-Fi: %s\n", AP_SSID);
  display.setCursor(0, 26);
  display.printf("Pass:  %s\n", AP_PASS);
  display.setCursor(0, 38);
  display.println("Connect Phone & Tap");
  display.setCursor(0, 50);
  display.println("'Sign in to Network'");
  display.display();
  delay(1500);
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  uint32_t rawMv1 = 0, rawMv2 = 0, rawMv3 = 0, rawMvChg = 0;
  float v1   = readVoltage(PIN_CELL1, RATIO_1, rawMv1, 120);
  float v2   = readVoltage(PIN_CELL2, RATIO_2, rawMv2, 120);
  float v3   = readVoltage(PIN_CELL3, RATIO_3, rawMv3, 120);

  // Read charging input with sensitive 25mV noise cutoff to detect 0.20V signals
  float vChg = readVoltage(PIN_CHARGER, RATIO_CHG, rawMvChg, 25);

  // Exponential smoothing on charging voltage
  vChgSmoothed = (0.75f * vChgSmoothed) + (0.25f * vChg);
  g_vChg = vChgSmoothed;

  // Dual-threshold Schmidt trigger: ON at 0.20V, OFF below 0.12V
  unsigned long now = millis();
  if (!isChargingStable) {
    if ((vChgSmoothed >= CHG_ON_THRESHOLD_V) && (now - lastStateChangeMs >= DWELL_MIN_MS)) {
      isChargingStable = true;
      lastStateChangeMs = now;
    }
  } else {
    if ((vChgSmoothed < CHG_OFF_THRESHOLD_V) && (now - lastStateChangeMs >= DWELL_MIN_MS)) {
      isChargingStable = false;
      lastStateChangeMs = now;
    }
  }

  // ==================== LED BLINK CONTROL ====================
  // Blinks when in charging mode; stays OFF when idle
  if (isChargingStable) {
    digitalWrite(PIN_LED, ((now / 250) % 2) ? HIGH : LOW);
  } else {
    digitalWrite(PIN_LED, LOW);
  }

  // Update battery status
  g_batteryConnected = (v1 >= 2.00f);
  g_tap3Connected    = (v3 >= 3.00f);

  if (g_batteryConnected) {
    g_cell1 = v1;
    g_cell2 = max(0.0f, v2 - v1);
    if (g_tap3Connected && (v3 >= v2)) {
      g_cell3 = v3 - v2;
      g_total = v3;
    } else {
      g_cell3 = 0.0f;
      g_total = g_cell1 + g_cell2;
    }
  } else {
    g_cell1 = 0.0f;
    g_cell2 = 0.0f;
    g_cell3 = 0.0f;
    g_total = 0.0f;
  }

  updateChargingMetrics(g_vChg, isChargingStable);

  // ==================== OLED DISPLAY UPDATE ====================
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!g_batteryConnected) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("3S MONITOR");
    display.setCursor(76, 0);
    display.print("[NO BATT]");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    display.setCursor(8, 22);
    display.println("NO BATTERY DETECTED");
    display.setCursor(4, 36);
    display.println("Connect 3S Balance Lead");

    display.drawLine(0, 48, 127, 48, SSD1306_WHITE);
    display.setCursor(0, 53);
    if (isChargingStable) {
      display.printf("CHG: %.2fV [%s]", g_vChg, g_multStr);
    } else {
      display.print("IP: 192.168.4.1");
    }

  } else if (isChargingStable) {
    display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(6, 2);
    if ((animTick % 2) == 0) {
      display.print(">>> CHARGING ACTIVE <<<");
    } else {
      display.print(">>> HARVESTING <<<");
    }

    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(3);
    int strLen = strlen(g_multStr);
    int xPos = (strLen == 2) ? 44 : 35;
    display.setCursor(xPos, 16);
    display.print(g_multStr);

    display.setTextSize(1);
    display.setCursor(76, 28);
    display.print(g_labelStr);

    display.drawRect(4, 42, 120, 8, SSD1306_WHITE);
    int barW = (g_strengthPercent * 116) / 100;
    if (barW > 0) {
      display.fillRect(6, 44, barW, 4, SSD1306_WHITE);
    }

    display.setCursor(4, 54);
    display.printf("IN:%.2fV", g_vChg);
    display.setCursor(68, 54);
    display.printf("BAT:%.2fV", g_total);

  } else {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("3S MONITOR");
    display.setCursor(80, 0);
    display.print("192.168.4.1");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    display.setCursor(0, 12);
    display.printf("C1:%.2fV", g_cell1);
    int p1 = voltageToPercent(g_cell1);
    display.drawRect(58, 12, 68, 7, SSD1306_WHITE);
    int b1 = (p1 * 64) / 100;
    if (b1 > 0) display.fillRect(60, 14, b1, 3, SSD1306_WHITE);

    display.setCursor(0, 22);
    display.printf("C2:%.2fV", g_cell2);
    int p2 = voltageToPercent(g_cell2);
    display.drawRect(58, 22, 68, 7, SSD1306_WHITE);
    int b2 = (p2 * 64) / 100;
    if (b2 > 0) display.fillRect(60, 24, b2, 3, SSD1306_WHITE);

    display.setCursor(0, 32);
    if (!g_tap3Connected) {
      display.print("C3:NO WIRE");
      display.setCursor(62, 32);
      display.print("[Check VP/36]");
    } else {
      display.printf("C3:%.2fV", g_cell3);
      int p3 = voltageToPercent(g_cell3);
      display.drawRect(58, 32, 68, 7, SSD1306_WHITE);
      int b3 = (p3 * 64) / 100;
      if (b3 > 0) display.fillRect(60, 34, b3, 3, SSD1306_WHITE);
    }

    display.drawLine(0, 41, 127, 41, SSD1306_WHITE);
    int avgP = g_tap3Connected ? ((p1 + p2 + voltageToPercent(g_cell3)) / 3) : ((p1 + p2) / 2);
    display.setCursor(0, 46);
    display.printf("TOT: %.2fV", g_total);
    display.setCursor(86, 46);
    display.printf("%3d%%", avgP);

    display.setCursor(0, 56);
    display.printf("CHG: 0.00V [IDLE]");
  }

  display.display();
  animTick++;
  delay(40);
}
