#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <heltec.h>

// Network credentials
const char* ssid = "2219";
const char* password = "48634009bd";

// Heater control pin and relay type
const int heaterPin = 5;
const bool relayNormallyOpen = true;  // Set to false for Normally Closed relay

// WorldTime API settings
const char* worldTimeApiHost = "worldtimeapi.org";
const char* timeZone = "America/New_York";  // Change this to your desired timezone

// Global variable to track last sync time
unsigned long lastSyncTime = 0;
const unsigned long syncInterval = 3600000; // 24 hours in milliseconds

// Time variables
int hour, minute, second, day, month, year, dayOfWeek;
bool timeSynced = false;
unsigned long lastSyncEpoch = 0;  // Last synchronized time as Unix timestamp

void setup() {
    Heltec.begin(true, false, true);  // Initialize Heltec module
    Serial.begin(115200);
    while (!Serial); // Wait for the serial port to connect
    Serial.println("Setup Started");

    Heltec.display->init();
    Heltec.display->flipScreenVertically();
    Heltec.display->setFont(ArialMT_Plain_10);

    pinMode(heaterPin, OUTPUT);  // Initialize heater control pin

    Serial.println("Attempting Time Sync");
    String syncStatus = syncTime() ? "Time Sync success" : "Time Sync unsuccessful";
    updateDisplay(syncStatus, "", "", syncStatus.startsWith("Time Sync unsuccessful") ? "TIME SYNC FAILED" : "");
    Serial.println(syncStatus);
}

void loop() {
    Serial.println("Entering Loop");

    // Check if it's time to sync
    if (millis() - lastSyncTime >= syncInterval) {
        Serial.println("Time for Sync");
        String syncStatus = syncTime() ? "Sync successful" : "Sync unsuccessful";
        updateDisplay(syncStatus, "", "", syncStatus.startsWith("Sync unsuccessful") ? "TIME SYNC FAILED" : "");
        lastSyncTime = millis();
    } else {
        Serial.println("Regular Update");
        updateTime();
        String currentTime = getFormattedDate();
        String heaterStatus = getHeaterStatus();

        if (isPeakTime()) {
            controlHeater(false);  // Turn off heater during peak time
        } else {
            controlHeater(true);   // Turn on heater during off-peak time
        }

        updateDisplay("", currentTime, heaterStatus, "");  // Empty sync status when not syncing
    }

    // Remove or adjust this delay as needed
    delay(10000);  // Delay for 10 seconds
}

void updateTime() {
    unsigned long currentMillis = millis();
    unsigned long timeElapsed = currentMillis - lastSyncTime; // Time since last sync or last update in milliseconds
    unsigned long timeElapsedInSeconds = timeElapsed / 1000; // Convert to seconds

    if (timeSynced) {
        unsigned long currentUnixTime = lastSyncEpoch + timeElapsedInSeconds;
        time_t rawtime = (time_t)currentUnixTime;
        struct tm * ptm = localtime(&rawtime);
        hour = ptm->tm_hour;
        minute = ptm->tm_min;
        second = ptm->tm_sec;
        day = ptm->tm_mday;
        month = ptm->tm_mon + 1;
        year = ptm->tm_year + 1900;
        dayOfWeek = ptm->tm_wday;
    } else {
        second += timeElapsedInSeconds;
        while (second >= 60) {
            minute++;
            second -= 60;
        }
        while (minute >= 60) {
            hour++;
            minute -= 60;
        }
        while (hour >= 24) {
            hour -= 24;
            // Increment day here if needed
        }
    }
    // Remove lastSyncTime update from here
    Serial.print("Updated Time: ");
    Serial.print(hour);
    Serial.print(":");
    Serial.print(minute);
    Serial.println(); // Don't print seconds
}


void updateDisplay(const String& syncStatus, const String& time, const String& heaterStatus, const String& bottomMessage) {
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->drawString(64, 0, syncStatus);
    Heltec.display->drawString(64, 12, time);
    Heltec.display->drawString(64, 24, heaterStatus);
    Heltec.display->drawString(64, 36, bottomMessage);
    Heltec.display->display();
}

bool syncTime() {
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(worldTimeApiHost, 443)) {
        WiFi.disconnect(true);
        return false;
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
            break;
        }
    }

    String payload = client.readString();
    Serial.println("Received payload from WorldTime API");
    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    String datetime = doc["datetime"].as<String>();
    sscanf(datetime.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    lastSyncEpoch = doc["unixtime"].as<unsigned long>() + doc["raw_offset"].as<long>() + doc["dst_offset"].as<long>();
    timeSynced = true;
    WiFi.disconnect(true);
    return true;
}

String getFormattedDate() {
    char dateBuffer[20];
    sprintf(dateBuffer, "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
    return String(dateBuffer);
}

void controlHeater(bool turnOn) {
    digitalWrite(heaterPin, relayNormallyOpen == turnOn ? HIGH : LOW);
}

String getHeaterStatus() {
    return digitalRead(heaterPin) == (relayNormallyOpen ? HIGH : LOW) ? "WATER HEATER ON" : "WATER HEATER OFF";
}

bool isPeakTime() {
    dayOfWeek = dayOfWeek == 0 ? 7 : dayOfWeek;
    if (dayOfWeek >= 1 && dayOfWeek <= 5) {
        if ((month >= 11 || month <= 3)) {
            if ((hour == 5 && minute >= 55) || (hour > 5 && hour < 10) || (hour == 10 && minute < 5)) {
                return true;
            }
        } else if (month >= 4 && month <= 10) {
            if ((hour == 12 && minute >= 55) || (hour > 12 && hour < 20) || (hour == 20 && minute < 5)) {
                return true;
            }
        }
    }
    return false;
}
