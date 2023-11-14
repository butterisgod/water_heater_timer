#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <heltec.h>

// Network credentials
const char* ssid = "xx";
const char* password = "xx";

// Heater control pin and relay type
const int heaterPin = 5;
const bool relayNormallyOpen = true;  // Set to false for Normally Closed relay

// WorldTime API settings
const char* worldTimeApiHost = "worldtimeapi.org";
const char* timeZone = "America/New_York";  // Change this to your desired timezone

// Global variable to track last sync time
unsigned long lastSyncTime = 0;
const unsigned long syncInterval = 86400000; // 24 hours in milliseconds

// Time variables
int hour, minute, second, day, month, year, dayOfWeek;
bool timeSynced = false;
unsigned long lastSyncEpoch = 0;  // Last synchronized time as Unix timestamp

void setup() {
    Heltec.begin(true, false, true);  // Initialize Heltec module
    Serial.begin(115200);
    Heltec.display->init();
    Heltec.display->flipScreenVertically();
    Heltec.display->setFont(ArialMT_Plain_10);

    pinMode(heaterPin, OUTPUT);  // Initialize heater control pin

    String syncStatus = syncTime() ? "Time Sync success" : "Time Sync unsuccessful";
    updateDisplay(syncStatus, "", "");
}

void loop() {
    // Check if it's time to sync
    if (millis() - lastSyncTime >= syncInterval) {
        String syncStatus = syncTime() ? "Sync successful" : "Sync unsuccessful";
        updateDisplay(syncStatus, "", "");
        lastSyncTime = millis();
    } else {
        // Update time and display regularly
        updateTime();
        String currentTime = getFormattedDate();
        String heaterStatus = getHeaterStatus();

        if (isPeakTime()) {
            controlHeater(false);  // Turn off heater during peak time
        } else {
            controlHeater(true);   // Turn on heater during off-peak time
        }

        updateDisplay("", currentTime, heaterStatus);  // Empty sync status when not syncing
    }
    
    delay(10000);  // Delay for 10 seconds
}

void updateTime() {
    if (timeSynced) {
        unsigned long currentMillis = millis();
        unsigned long timeSinceLastSync = currentMillis - lastSyncTime; // Time since last sync in milliseconds
        unsigned long secondsSinceLastSync = timeSinceLastSync / 1000;   // Convert to seconds

        // Calculate current epoch time
        unsigned long currentEpoch = lastSyncEpoch + secondsSinceLastSync;

        // Convert epoch time to struct tm
        struct tm *currentTime = gmtime((time_t *)&currentEpoch);

        // Update global time variables
        hour = currentTime->tm_hour;
        minute = currentTime->tm_min;
        second = currentTime->tm_sec;
        day = currentTime->tm_mday;
        month = currentTime->tm_mon + 1;
        year = currentTime->tm_year + 1900;
    }
}



void updateDisplay(const String& syncStatus, const String& time, const String& heaterStatus) {
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->drawString(64, 0, syncStatus);
    Heltec.display->drawString(64, 12, time);
    Heltec.display->drawString(64, 24, heaterStatus);
    Heltec.display->display();
}

bool syncTime() {
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        return false; // WiFi connection failed
    }

    WiFiClientSecure client;
    client.setInsecure(); // Disable SSL certificate verification

    if (!client.connect(worldTimeApiHost, 443)) {
        WiFi.disconnect(true);
        return false; // Connection to API failed
    }

    client.print("GET /api/timezone/");
    client.print(timeZone);
    client.println(" HTTP/1.0");
    client.println("Host: worldtimeapi.org");
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
            break; // Headers are finished
        }
    }

    String payload = client.readString();

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    const char* datetime = doc["datetime"]; // "2023-11-14T15:19:30.123456-05:00"
    sscanf(datetime, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    // Extract Unix timestamp from the API response
    lastSyncEpoch = doc["unixtime"];  // Unix timestamp (epoch time)

    timeSynced = true;

    WiFi.disconnect(true);

    return true;
}


String getFormattedDate() {
    char dateBuffer[20];
    sprintf(dateBuffer, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    return String(dateBuffer);
}

void controlHeater(bool turnOn) {
    digitalWrite(heaterPin, relayNormallyOpen == turnOn ? HIGH : LOW);
}

String getHeaterStatus() {
    return digitalRead(heaterPin) == (relayNormallyOpen ? HIGH : LOW) ? "WATER HEATER ON" : "WATER HEATER OFF";
}

bool isPeakTime() {
    // Adjust dayOfWeek to be 1 for Monday, 7 for Sunday
    dayOfWeek = dayOfWeek == 0 ? 7 : dayOfWeek;

    if (dayOfWeek >= 1 && dayOfWeek <= 5) {  // Weekday check
        if ((month >= 11 || month <= 3)) {
            if ((hour == 6 && minute >= 55) || // 5 minutes before peak time starts in winter
                (hour > 6 && hour < 9) || // peak time
                (hour == 9 && minute < 5)) { // 5 minutes after peak time ends in winter
                return true;
            }
        } else if (month >= 4 && month <= 10) {
            if ((hour == 13 && minute >= 55) || // 5 minutes before peak time starts in summer
                (hour > 13 && hour < 19) || // peak time
                (hour == 19 && minute < 5)) { // 5 minutes after peak time ends in summer
                return true;
            }
        }
    }
    return false;  // Non-peak time
}
