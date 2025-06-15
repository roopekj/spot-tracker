#include "time.h"
#include "string.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <LiquidCrystal.h>

// WiFi credentials
const char* ssid = "ssid";
const char* password = "password";

// NTP domain
const char* ntpServer = "fi.pool.ntp.org";

// Price API
const String url = "https://www.porssisahkoa.fi/api/Prices/GetPrices?mode=1";
unsigned long lastTime = 0;
unsigned long timerDelay = NULL;   // This will get updated after the first successful update

// Initialize the library by associating any needed LCD interface pin with the ESP32 pin number it is connected to
const int rs = 13, en = 12, d4 = 26, d5 = 25, d6 = 33, d7 = 32;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setTimezone(String timezone){
  Serial.printf("Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);
  tzset();
}

void initTime(String timezone){
  struct tm timeinfo;

  configTime(0, 0, ntpServer);
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  setTimezone(timezone);
}

int getHour(){
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    return timeinfo.tm_hour;
  } else {
    Serial.println("Failed to obtain hour");
    return -1;
  }
}

int getMinute(){
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    return timeinfo.tm_min;
  } else {
    Serial.println("Failed to obtain minute");
    return -1;
  }
}

void setup() {
  Serial.begin(115200); 

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Starting...");

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    WiFi.begin(ssid, password);
    delay(5000);
  }
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  initTime("EET-2EEST,M3.5.0/3,M10.5.0/4");
}

// We want to perform the following things after boot:
//  * Right after the boot, we want to make a request no matter what.
//  * After the first successful request, we want to wait for as many minutes as there are before the next hour.
// This way, we refresh our data at the start of every hour while also not making excessive calls.
// We don't assume the delay to be exactly an hour after a successful request due to the possibility of WiFi problems and API downtime.
void loop() {
  if (lastTime == NULL || (millis() - lastTime) > timerDelay) {
    if(WiFi.status()== WL_CONNECTED) {
      HTTPClient http;
      
      http.begin(url.c_str());
      
      int httpResponseCode = http.GET();
      
      if (httpResponseCode == 200) {
        String payload = http.getString();
        JSONVar parsed = JSON.parse(payload);
 
        if (JSON.typeof(parsed) == "undefined") {
          Serial.println("Parsing API response failed!");
          delay(60000);
          return;
        }

        const double currentPrice = (double) parsed[getHour()]["value"];

        lcd.setCursor(0, 0);
        char first_buffer[16];
        snprintf(first_buffer, sizeof(first_buffer), "%d-%d: %0.2lf", getHour(), getHour() + 1, currentPrice);
        lcd.print(first_buffer);

        if (getHour() < 23) {
          const double nextPrice = (double) parsed[getHour() + 1]["value"];

          lcd.setCursor(0, 1);
          char second_buffer[16];
          snprintf(second_buffer, sizeof(second_buffer), "%d-%d: %0.2lf", getHour() + 1, getHour() + 2, nextPrice);
          lcd.print(second_buffer);
        }

        // This iteration of the loop was a success, so we update variables accordingly
        timerDelay = 60 * 1000 * (60 - getMinute());
        lastTime = millis();
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected, attempting reconnect");
      WiFi.begin(ssid, password);
      while(WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        WiFi.begin(ssid, password);
        delay(5000);
      }
    }
  }
}
