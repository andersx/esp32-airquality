#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <WebServer.h>
#include "esp_wifi.h"
#include <LiquidCrystal_I2C.h>
#include "arduino_secrets.h"

#define SEALEVELPRESSURE_HPA (1013.25)
#define LED_INDICATOR_PIN 2
#define LCD_UPDATE_INTERVAL 5000  // ms

WebServer server(80);

 // Bosch BME680 - I2C interface
Adafruit_BME680 bme;

// PMA0003 sensor - serial interface
HardwareSerial pmsSerial(2);

// AliExpress 16x2 LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // 16x2 LCD


unsigned long lastLcdUpdate = 0;
const unsigned long lcdUpdateInterval = 5000; // 5 seconds
uint16_t pm1_0 = 0, pm2_5 = 0, pm10 = 0;

float currentTemperature = NAN;
float currentHumidity = NAN;
float currentPressure = NAN;
float currentGas = NAN;
float currentAltitude = NAN;

// === Function Declarations ===
void initializeWiFi();
void initializeSensor();
void initializeLCD();
void updateLCD();
bool readSensor();
void handleRoot();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(LED_INDICATOR_PIN, OUTPUT);
  digitalWrite(LED_INDICATOR_PIN, LOW);

  initializeLCD();
  initializeWiFi();
  initializeSensor();

  pmsSerial.begin(9600, SERIAL_8N1, 16, 17);
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");

  // Initial reading for LCD display
  readSensor();
  updateLCD();
}

void loop() {
  server.handleClient();

  if (millis() - lastLcdUpdate >= LCD_UPDATE_INTERVAL) {
    lastLcdUpdate = millis();
    updateLCD();
  }
}

// === Initialize WiFi ===
void initializeWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.begin(SECRET_SSID, SECRET_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(2000);
}

// === Initialize BME680 Sensor ===
void initializeSensor() {
  if (!bme.begin(0x77)) {
    Serial.println("BME680 not found!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    while (true);
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // Temp (°C), duration (ms)
}

// === Initialize LCD ===
void initializeLCD() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Booting...");
}

// === Read All Sensor Values ===
bool readSensor() {
  unsigned long endTime = bme.beginReading();
  if (endTime == 0) {
    Serial.println("Failed to begin BME680 reading");
    return false;
  }

  delay(50); // Simulate async work

  if (!bme.endReading()) {
    Serial.println("Failed to complete BME680 reading");
    return false;
  }

  currentTemperature = bme.temperature;
  currentHumidity = bme.humidity;
  currentPressure = bme.pressure / 100.0; // Pa to hPa
  currentGas = bme.gas_resistance / 1000.0; // Ohms to KOhms
  currentAltitude = bme.readAltitude(SEALEVELPRESSURE_HPA);

  Serial.printf("T: %.2f °C, H: %.2f %%\n", currentTemperature, currentHumidity);
  Serial.printf("P: %.2f hPa, G: %.2f kOhms, Alt: %.2f m\n", currentPressure, currentGas, currentAltitude);

  return true;
}

void readPMSData() {
  while (pmsSerial.available() >= 32) {
    if (pmsSerial.read() == 0x42 && pmsSerial.read() == 0x4D) {
      uint8_t buffer[30];
      buffer[0] = 0x42;
      buffer[1] = 0x4D;
      for (int i = 2; i < 32; i++) {
        buffer[i] = pmsSerial.read();
      }

      pm1_0 = (buffer[10] << 8) | buffer[11];
      pm2_5 = (buffer[12] << 8) | buffer[13];
      pm10  = (buffer[14] << 8) | buffer[15];
    }
  }
}

// === Update LCD Display ===
void updateLCD2() {
  lcd.clear();

  if (!isnan(currentTemperature) && !isnan(currentHumidity)) {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(currentTemperature, 1);
    lcd.print(" C");

    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(currentHumidity, 0);
    lcd.print(" %");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("No sensor data");
  }
}

void updateLCD3() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(bme.temperature, 1);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("PM2.5: ");
  lcd.print(pm2_5);
  lcd.print(" ug/m3");
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PM2.5:");
  
  
  // Pad so output is left-adjusted,
  // overflow will cause units to not be dislpayed
  if (pm2_5 < 10) {
    lcd.print("   "); 
  } else if (pm2_5 < 100) {
    lcd.print("  "); 
  } else if (pm2_5 < 1000) {
    lcd.print(" ");
  } 
  lcd.print(pm2_5);
  lcd.print(" ");
  lcd.print((char) 0b11100100);
  lcd.print("g/m3");

  lcd.setCursor(0, 1);
  if (pm2_5 < 12.5) {
    lcd.print("Good"); 
  } else if (pm2_5 < 25) {
    lcd.print("Moderate"); 
  } else if (pm2_5 < 50) {
    lcd.print("Unhealthy*");
  } else if (pm2_5 < 150) {
    lcd.print("Unhealthy*");
  } else if (pm2_5 >= 150) {
    lcd.print("Hazardous");
  }

}

// === Web Server Handler (JSON Response) ===
void handleRoot() {
  digitalWrite(LED_INDICATOR_PIN, HIGH);

  if (!readSensor()) {
    server.send(500, "application/json", "{\"error\":\"Sensor read failed\"}");
    digitalWrite(LED_INDICATOR_PIN, LOW);
    return;
  }

  String json = "{";
  json += "\"temperature\":" + String(bme.temperature, 2) + ",";
  json += "\"pressure\":" + String(bme.pressure / 100.0, 2) + ",";
  json += "\"humidity\":" + String(bme.humidity, 2) + ",";
  json += "\"gas\":" + String(bme.gas_resistance / 1000.0, 2) + ",";
  json += "\"altitude\":" + String(bme.readAltitude(SEALEVELPRESSURE_HPA), 2) + ",";
  json += "\"pm1_0\":" + String(pm1_0) + ",";
  json += "\"pm2_5\":" + String(pm2_5) + ",";
  json += "\"pm10\":"  + String(pm10);
  json += "}";

  server.send(200, "application/json", json);
  digitalWrite(LED_INDICATOR_PIN, LOW);
}
