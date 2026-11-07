/**
 * ==============================================================================
 * PROJECT: IoT-Based Smart Agriculture Environmental Monitoring System
 * AUTHOR : Bhagyamma K (USN: BMTU2004)
 * PLATFORM: PlatformIO / ESP32 Dev Module
 * BAUD RATE: 9600
 * CLOUD: ThingSpeak IoT (API Key: 4QTXKK7SFW9UUFH9)
 * 
 * HARDWARE CONNECTIONS:
 *   - DHT11 (Temp & Humidity)     : Signal -> GPIO 4  | VCC -> 3.3V | GND -> GND
 *   - Capacitive Soil Moisture    : AOUT   -> GPIO 34 | VCC -> 3.3V | GND -> GND
 *   - MQ-135 / MQ-2 Gas Sensor    : AO     -> GPIO 35 | VCC -> 5V   | GND -> GND
 *   - 0.96" SSD1306 OLED (128x64) : SDA    -> GPIO 21 | SCL -> GPIO 22 | VCC -> 3.3V
 * ==============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ======================== 1. Wi-Fi & Cloud Configuration ===========
const char* ssid         = "BHAGYAMMA K";     // Configured Wi-Fi Name
const char* password     = "BMTU2004";         // Configured Wi-Fi Password
const char* apiKey       = "4QTXKK7SFW9UUFH9"; // ThingSpeak Write API Key
const char* serverName   = "http://api.thingspeak.com/update";

// ======================== 2. Pin Assignments =======================
#define DHTPIN              4       // Data pin for DHT11
#define DHTTYPE             DHT11   // Sensor type
#define SOIL_MOISTURE_PIN   34      // Analog ADC1_CH6
#define GAS_SENSOR_PIN      35      // Analog ADC1_CH7

// OLED Display parameters (128x64 I2C)
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1
#define OLED_I2C_ADDRESS    0x3C

// ======================== 3. Calibration Limits ====================
const int   SOIL_DRY_RAW    = 3400; // Sensor in open air
const int   SOIL_WET_RAW    = 1450; // Sensor submerged in water
const float MOISTURE_DRY    = 35.0; // Moisture < 35% is DRY
const float MOISTURE_WET    = 70.0; // Moisture > 70% is WET
const int   GAS_ALERT_LIMIT = 1800; // Baseline threshold for gas hazard

// ======================== 4. Global Objects & State ================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

float  temperature         = 0.0;
float  humidity            = 0.0;
int    soilRaw             = 0;
float  soilMoisturePercent = 0.0;
String soilStatus          = "OPTIMAL";
int    gasRaw              = 0;
String gasStatus           = "NORMAL";

// Timers for non-blocking execution
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 2000;   // Update OLED & sensors every 2s

unsigned long lastCloudPush  = 0;
const unsigned long CLOUD_INTERVAL  = 20000;  // Push to ThingSpeak every 20s

// ======================== 5. Forward Declarations ==================
float calculateMoisture(int raw);
void readAllSensors();
void updateOLED();
void handleRootTable();
void handleDataApi();
void sendDataToThingSpeak();

// ======================== 6. Sensor Helper Functions ===============
float calculateMoisture(int raw) {
  int clamped = constrain(raw, SOIL_WET_RAW, SOIL_DRY_RAW);
  float pct = map(clamped, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100);
  return constrain(pct, 0.0f, 100.0f);
}

void readAllSensors() {
  // 1. Read DHT11
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
  }

  // 2. Read Soil Moisture
  soilRaw = analogRead(SOIL_MOISTURE_PIN);
  soilMoisturePercent = calculateMoisture(soilRaw);
  if (soilMoisturePercent < MOISTURE_DRY) {
    soilStatus = "DRY";
  } else if (soilMoisturePercent > MOISTURE_WET) {
    soilStatus = "WET";
  } else {
    soilStatus = "OPTIMAL";
  }

  // 3. Read MQ Gas Sensor
  gasRaw = analogRead(GAS_SENSOR_PIN);
  if (gasRaw > GAS_ALERT_LIMIT) {
    gasStatus = "ALERT";
  } else {
    gasStatus = "NORMAL";
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Top Status Bar
  display.setCursor(0, 0);
  display.print(F("AGRI CLOUD NODE"));
  display.setCursor(96, 0);
  display.print(WiFi.status() == WL_CONNECTED ? F("[OK]") : F("[--]"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Line 1: Temperature & Humidity
  display.setCursor(0, 14);
  display.printf("Temp: %.1f C  Hum: %d%%", temperature, (int)humidity);

  // Line 2: Soil Moisture & Status
  display.setCursor(0, 27);
  display.printf("Soil: %.1f%% [%s]", soilMoisturePercent, soilStatus.c_str());

  // Line 3: Gas Concentration
  display.setCursor(0, 40);
  display.printf("Gas:  %d  [%s]", gasRaw, gasStatus.c_str());
  display.drawLine(0, 51, 128, 51, SSD1306_WHITE);

  // Bottom Line: IP Address or Cloud Status
  display.setCursor(0, 55);
  if (WiFi.status() == WL_CONNECTED) {
    display.print(WiFi.localIP().toString());
  } else {
    display.print(F("Local Mode"));
  }

  display.display();
}

// ======================== 7. Web Server Handlers ===================

/**
 * Serves live structured telemetry table to any browser at http://<ESP32_IP>/
 */
void handleRootTable() {
  String sBg = (soilStatus == "DRY") ? "#ea580c" : ((soilStatus == "WET") ? "#0284c7" : "#16a34a");
  String gBg = (gasStatus == "ALERT") ? "#dc2626" : "#16a34a";

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><meta http-equiv='refresh' content='2'>";
  html += "<title>Smart Agriculture Dashboard - Bhagyamma K</title>";
  html += "<style>";
  html += "body{font-family:'Segoe UI',Arial,sans-serif;background:#f8fafc;padding:24px 16px;margin:0;text-align:center;}";
  html += ".container{max-width:680px;margin:auto;background:#ffffff;padding:28px;border-radius:16px;box-shadow:0 4px 16px rgba(0,0,0,0.06);}";
  html += "h2{color:#15803d;margin:0 0 6px 0;font-size:22px;}";
  html += ".sub{color:#64748b;font-size:13px;margin:0 0 20px 0;}";
  html += "table{width:100%;border-collapse:collapse;text-align:left;margin-top:10px;}";
  html += "th,td{padding:12px 16px;border-bottom:1px solid #e2e8f0;font-size:14px;}";
  html += "th{background:#f1f5f9;color:#334155;font-weight:600;}";
  html += ".badge{padding:4px 10px;border-radius:8px;color:#ffffff;font-weight:bold;font-size:12px;display:inline-block;}";
  html += ".footer{margin-top:22px;font-size:12px;color:#94a3b8;border-top:1px solid #f1f5f9;padding-top:14px;}";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h2>Smart Agriculture Telemetry Dashboard</h2>";
  html += "<p class='sub'><b>Author:</b> Bhagyamma K (USN: BMTU2004) &bull; Auto-refreshes every 2s</p>";
  
  html += "<table>";
  html += "<thead><tr><th>Parameter</th><th>Sensor Reading</th><th>Standard Range</th><th>Live Status</th></tr></thead>";
  html += "<tbody>";
  html += "<tr><td><b>Soil Moisture</b></td><td>" + String(soilMoisturePercent, 1) + " %</td><td>35% - 70%</td><td><span class='badge' style='background:" + sBg + ";'>" + soilStatus + "</span></td></tr>";
  html += "<tr><td><b>Temperature</b></td><td>" + String(temperature, 1) + " &deg;C</td><td>20&deg;C - 32&deg;C</td><td><span class='badge' style='background:#16a34a;'>NORMAL</span></td></tr>";
  html += "<tr><td><b>Humidity</b></td><td>" + String(humidity, 1) + " %</td><td>50% - 75%</td><td><span class='badge' style='background:#16a34a;'>NORMAL</span></td></tr>";
  html += "<tr><td><b>Air / Gas</b></td><td>" + String(gasRaw) + " ADC</td><td>&lt; 1800 ADC</td><td><span class='badge' style='background:" + gBg + ";'>" + gasStatus + "</span></td></tr>";
  html += "</tbody></table>";

  html += "<div class='footer'>Raw Soil ADC: <b>" + String(soilRaw) + "</b> &bull; Cloud: <b>ThingSpeak Active</b> &bull; Baud: <b>9600</b></div>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

/**
 * REST API Endpoint returning JSON data
 */
void handleDataApi() {
  String json = "{";
  json += "\"author\":\"Bhagyamma K\",";
  json += "\"usn\":\"BMTU2004\",";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"soilMoisture\":" + String(soilMoisturePercent, 1) + ",";
  json += "\"soilRaw\":" + String(soilRaw) + ",";
  json += "\"soilStatus\":\"" + soilStatus + "\",";
  json += "\"gasRaw\":" + String(gasRaw) + ",";
  json += "\"gasStatus\":\"" + gasStatus + "\"";
  json += "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

/**
 * Uploads live sensor fields to ThingSpeak Cloud
 */
void sendDataToThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[CLOUD] Wi-Fi offline. Telemetry skipped.");
    return;
  }

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String httpRequestData = "api_key=" + String(apiKey) +
                           "&field1=" + String(soilMoisturePercent, 1) +
                           "&field2=" + String(temperature, 1) +
                           "&field3=" + String(humidity, 1) +
                           "&field4=" + String(gasRaw);

  Serial.println("[CLOUD] Uploading data to ThingSpeak...");
  int httpResponseCode = http.POST(httpRequestData);

  if (httpResponseCode > 0) {
    Serial.printf("[CLOUD] Success! HTTP Response code: %d\n", httpResponseCode);
  } else {
    Serial.printf("[CLOUD] Error sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// ======================== 8. Setup & Main Loop =====================
void setup() {
  Serial.begin(9600);

  // Configure 12-bit ADC (0 - 4095)
  analogReadResolution(12);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(GAS_SENSOR_PIN, INPUT);

  // Initialize Sensors
  dht.begin();
  Wire.begin(21, 22);

  // Initialize OLED Display with Student Splash Screen
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16, 22);
    display.println(F("Bhagyamma K"));
    display.setCursor(26, 36);
    display.println(F("BMTU2004"));
    display.display();
    delay(1500);
  }

  // Connect to Wi-Fi
  Serial.printf("\nConnecting to %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[HTTP] Dashboard table live at: http://%s/\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Connection timeout. Operating in local mode.");
  }

  // Register Web Routes
  server.on("/", handleRootTable);
  server.on("/data", handleDataApi);
  server.begin();

  readAllSensors();
  updateOLED();
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  // Refresh OLED and read sensors every 2 seconds
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;
    readAllSensors();
    updateOLED();

    Serial.printf("[DATA] Temp: %.1f C | Hum: %.1f %% | Soil: %.1f %% (%s) | Gas: %d (%s)\n",
                  temperature, humidity, soilMoisturePercent, soilStatus.c_str(), gasRaw, gasStatus.c_str());
  }

  // Push data to ThingSpeak Cloud every 20 seconds
  if (currentMillis - lastCloudPush >= CLOUD_INTERVAL) {
    lastCloudPush = currentMillis;
    sendDataToThingSpeak();
  }
}