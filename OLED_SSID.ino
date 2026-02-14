#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===============================
// WIFI SETTINGS
// ===============================
const char* ssid = "Spectre";
const char* password = "Iamdashjei1996";

// ===============================
// SENSOR SETTINGS
// ===============================
#define WATER_SENSOR_PIN 34
#define PRESS_SENSOR_PIN 35

#define RESISTOR_VALUE 144.4
#define ADC_MAX 4095.0
#define ADC_REF 3.3
#define NUM_AVG 500

// Water sensor range (meters)
#define WATER_MAX_METERS 3.0

// Pressure range
#define PRESSURE_MAX_MPA 1.0
#define MPA_TO_PSI 145.038

// ===============================
// OLED SETTINGS
// ===============================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===============================
AsyncWebServer server(80);

// ===============================
// SENSOR FUNCTIONS
// ===============================
float readVoltage(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < NUM_AVG; i++) {
    sum += analogRead(pin);
  }
  float avg = (float)sum / NUM_AVG;
  return (avg / ADC_MAX) * ADC_REF;
}

float voltageToCurrent(float voltage) {
  return (voltage / RESISTOR_VALUE) * 1000.0;
}

float currentToPercent(float mA) {
  float percent = (mA - 4.0) * 6.25;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  return percent;
}

// ===============================
// JSON CREATOR (ArduinoJson v7)
// ===============================
String createJSON(float water_m, float press_psi) {

  JsonDocument doc;

  JsonObject water = doc["water_level"].to<JsonObject>();
  water["meters"] = water_m;

  JsonObject pressure = doc["pressure"].to<JsonObject>();
  pressure["psi"] = press_psi;

  String json;
  serializeJson(doc, json);
  Serial.println(json);
  return json;
}

// ===============================
// OLED DISPLAY
// ===============================
void updateOLED(String ip, float water_m, float press_psi) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Top line - IP
  display.setCursor(0, 0);
  display.print("IP: ");
  display.println(ip);

  // Water
  display.setCursor(0, 12);
  display.print("Water: ");
  display.print(water_m, 2);
  display.println(" m");

  // Pressure
  display.setCursor(0, 22);
  display.print("Press: ");
  display.print(press_psi, 1);
  display.println(" PSI");

  display.display();
}

// ===============================
void setup() {

  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(WATER_SENSOR_PIN, ADC_11db);
  analogSetPinAttenuation(PRESS_SENSOR_PIN, ADC_11db);

  // OLED init
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Flash screen while connecting
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("Connecting");
  display.display();

  WiFi.begin(ssid, password);

  bool blink = false;

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    blink = !blink;

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 10);
    if (blink) display.println("Connecting...");
    display.display();
  }

  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  // ===============================
  // ROUTES
  // ===============================
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/plain", "Go to /json");
  });

  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *req) {

    float wV = readVoltage(WATER_SENSOR_PIN);
    float pV = readVoltage(PRESS_SENSOR_PIN);

    float wI = voltageToCurrent(wV);
    float pI = voltageToCurrent(pV);

    float wPercent = currentToPercent(wI);
    float pPercent = currentToPercent(pI);

    // Convert to meters
    float water_m = (wPercent / 100.0) * WATER_MAX_METERS;

    // Convert to PSI
    float pressure_mpa = (pPercent / 100.0) * PRESSURE_MAX_MPA;
    float pressure_psi = pressure_mpa * MPA_TO_PSI;

    String json = createJSON(water_m, pressure_psi);

    req->send(200, "application/json", json);
  });

  server.begin();
}

// ===============================
void loop() {

  float wV = readVoltage(WATER_SENSOR_PIN);
  float pV = readVoltage(PRESS_SENSOR_PIN);

  float wI = voltageToCurrent(wV);
  float pI = voltageToCurrent(pV);

  float wPercent = currentToPercent(wI);
  float pPercent = currentToPercent(pI);

  float water_m = (wPercent / 100.0) * WATER_MAX_METERS;
  float pressure_psi = ((pPercent / 100.0) * PRESSURE_MAX_MPA) * MPA_TO_PSI;

  updateOLED(WiFi.localIP().toString(), water_m, pressure_psi);

  delay(2000);
}
