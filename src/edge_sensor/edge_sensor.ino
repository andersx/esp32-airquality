#include <WiFi.h>
#include <WiFiManager.h>

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>

#include <Wire.h>
#include <PMserial.h>
#include <time.h>
#include "auxiliary.h"

// IMPORTANT: TODO BEFORE FLASHING
// Set variables in auxiliary.h
// Need to set bucket and sensor name correctly to match with hardware.

// Upload time variables
unsigned long last_upload_time = 0;

// Loop every 5 seconds
const unsigned long read_interval = 5;
unsigned long last_read_time = 0;

// set the LCD number of columns and rows
const int LCD_COLUMNS = 16;
const int LCD_ROWS = 2;

// Just let these be global - ESP32 standard
LiquidCrystal_I2C lcd(0x27, LCD_COLUMNS, LCD_ROWS);

// Just let these be global - ESP32 standard
SerialPM pms(PMSx003, Serial2);  // Sensor type, hardware UART


void connect_wifimanager() {

  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi ");
  lcd.setCursor(0, 1);
  lcd.print("                ");

  WiFi.mode(WIFI_STA);

  WiFiManager wifi_manager;

  // Reset ESP32 WiFi netowrks ncomment this for testing
  // wifi_manager.resetSettings();

  // Reboot after 5 minutes if still no connection
  wifi_manager.setConfigPortalTimeout(300);

  bool success = wifi_manager.autoConnect("AirSensorSetup");  // anonymous ap

  if (!success) {
    Serial.println("❌ Failed to connect");
    lcd.setCursor(0, 1);  // bottom row
    lcd.print("NO WIFI - REBOOT");

    // Maybe reset wifi settings if WiFi fails.
    // Will require user to log in again,
    // Let's disable for now
    // wifi_manager.resetSettings();

    // Restart and start over if no wifi could be found
    ESP.restart();

  } else {
    // yeey!
    Serial.println("connected...yeey :)");
  }

  lcd.setCursor(0, 1);  // bottom row
  lcd.print("            ");
  lcd.setCursor(0, 1);  // bottom row
  lcd.print(WiFi.localIP());
}

// Get time from NPT server - time is needed for InfluxDB uploads
// TODO: Should I do this more often?? How fast is desync?
void set_time() {

  lcd.setCursor(0, 0);
  lcd.print("Sync timeserver ");
  lcd.setCursor(0, 1);
  lcd.print("                ");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for time");

  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Got time!");

  time_t ts = time(nullptr);
  // 16  chars are not enough for "YYYY-MM-DD HH:MM" for some reason?
  char time_buffer[17];
  struct tm* time_info = localtime(&ts);
  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M", time_info);

  lcd.setCursor(0, 1);
  lcd.print(time_buffer);
}

// Upload payload
void upload_to_influx(String payload) {

  lcd.setCursor(0, 0);
  lcd.print("Uploading values");
  lcd.setCursor(0, 1);
  lcd.print("to InfluxDB ... ");

  // Wifi should keep connecting automaticaclly,
  // so just skip this upload if temporarily out of wifi juice.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - will try again later!");
    return;
  }

  WiFiClientSecure* client = new WiFiClientSecure();
  // Skip certificate check for simplicity
  // TODO: Use HTTPS properly
  // client->setInsecure();
  client->setCACert(INFLUXDB_ROOT_CA);  // ✅ validate server cert

  HTTPClient http;

  Serial.println("Uploading test data to InfluxDB:");
  Serial.println("URL: " + INFLUXDB_URL);

  if (!http.begin(*client, INFLUXDB_URL)) {
    Serial.println("❌ http.begin() failed");
    delete client;
    return;
  }

  http.addHeader("Authorization", String("Token ") + INFLUXDB_TOKEN);
  http.addHeader("Content-Type", "text/plain");

  int response = http.POST(payload);
  Serial.print("📡 HTTP Response Code: ");
  Serial.println(response);

  if (response > 0) {
    Serial.print("✅ HTTP Response Code: ");
    Serial.println(response);

    // Only get the body if it's not 204
    if (response != 204) {
      String body = http.getString();
      Serial.println("📨 Server Response Body:");
      Serial.println(body);
    }
  } else {
    Serial.print("❌ Error: ");
    Serial.println(http.errorToString(response));
  }

  http.end();
  delete client;
}

void init_pmsx003_sensor() {
  lcd.setCursor(0, 0);
  lcd.print("Starting PMSx003");
  lcd.setCursor(0, 1);
  lcd.print("                ");

  // Start Serial2 on GPIO17 (RX), GPIO18 (TX)
  Serial2.begin(9600, SERIAL_8N1, 25, 26);
  pms.init();  // MUST be called after Serial2.begin()

  Serial.println("PMS sensor initialized on Serial2");
  lcd.setCursor(0, 1);
  lcd.print("Sensor ready!   ");
}

const char* classify_pm2_5(int pm2_5) {
  // TODO: Need resource for these classifications
  // https://www.who.int/news-room/feature-stories/detail/what-are-the-who-air-quality-guidelines
  if (pm2_5 < 0) {
    return "N/A";
  } else if (pm2_5 < 5) {
    return "Excellent";
  } else if (pm2_5 < 10) {
    return "Good";
  } else if (pm2_5 < 15) {
    return "Moderate";
  } else if (pm2_5 < 25) {
    return "Poor";
  } else if (pm2_5 < 50) {
    return "Very Poor";
  } else {
    return "Hazardous";
  }
}

void init_lcd_with_greeting() {
  // Turn on LCD backlight
  lcd.init();
  lcd.backlight();

  // Display greetings!
  lcd.setCursor(0, 0);
  lcd.print("Booting ESP32   ");
  lcd.setCursor(0, 1);
  lcd.print("Code by ANDERSX ");
}

// Painful lesson, need to pad almost everything 1 sec of waiting,
// Probably not all delays are needed, but it works
void setup() {

  // Setup for SDA SCL (default ESP32 pins)
  Wire.begin(21, 22);

  init_lcd_with_greeting();
  delay(1000);

  Serial.begin(115200);
  delay(1000);

  init_pmsx003_sensor();
  delay(1000);

  connect_wifimanager();
  delay(1000);

  set_time();
  delay(1000);
}


void update_display(int pm2_5) {

  lcd.setCursor(0, 0);

  if (pm2_5 > -1) {
    lcd.printf("PM2.5:%4d ", int(pms.pm25));
  } else {
    lcd.printf("PM2.5: N/A ");
  }

  lcd.print((char)0b11100100);
  lcd.print("g/m3");

  lcd.setCursor(0, 1);
  lcd.print("Air:            ");
  lcd.setCursor(5, 1);
  lcd.print(classify_pm2_5(pm2_5));
}


void loop() {

  // Current time in seconds
  unsigned long current_time_seconds = millis() / 1000;

  // Exit loop gracefully if it's too early for next sensor read
  // Don't simply use delay(5000) - this is the (ESP32) way!
  if (current_time_seconds - last_read_time < read_interval) {
    return;
  }

  last_read_time = current_time_seconds;


  Serial.print("Reading PMSx003 from serial2: ");
  Serial.println(SENSOR_NAME);

  //  Set these to negative to indicate they  have not been read
  // They should be zero or positive integers if read cocrrectly
  int pm1_0 = -1;
  int pm2_5 = -1;
  int pm10 = -1;

  // Read and parse PMS data
  pms.read();

  // Need time_stamp as a time_object - millis()/1000 is not the same?
  // Make the time_stamp here to sync up with pms.read() as well
  time_t time_stamp = time(nullptr);

  if (pms.status == OK) {
    Serial.printf(
      "PM1.0: %d, PM2.5: %d, PM10: %d [µg/m3]\n",
      pms.pm01, pms.pm25, pms.pm10);

    pm1_0 = pms.pm01;
    pm2_5 = pms.pm25;
    pm10 = pms.pm10;

  } else {
    Serial.print("Error: ");
    Serial.println(pms.status);
  }

  Serial.print("Done reading PMSx003 from serial2: ");
  Serial.println(SENSOR_NAME);

  // Show PM 2.5 levels and subjective rating
  update_display(pm2_5);

  // Check that readings are valid, if any are negative then bail out
  if ((pm1_0 > -1) || (pm2_5 > -1) || (pm10 > -1)) {

    // Assemble payload
    if (current_time_seconds - last_upload_time >= INFLUXDB_UPLOAD_INTERVAL) {

      String payload = SENSOR_NAME + " pm1_0=" + String(pm1_0) + ",pm2_5=" + String(pm2_5) + ",pm10=" + String(pm10) + " " + String(time_stamp);
      Serial.println("Payload: " + payload);
      Serial.println("Uploading to InfluxDB:");

      upload_to_influx(payload);
      last_upload_time = current_time_seconds;
      Serial.println("Done uploading to InfluxDB.");
    }

  } else {
    Serial.println("Invalid PMSx005 reading");
  }

  Serial.print("Next upload in ");
  Serial.print(INFLUXDB_UPLOAD_INTERVAL - (current_time_seconds - last_upload_time));
  Serial.println(" seconds");
}
