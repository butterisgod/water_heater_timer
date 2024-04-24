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
const unsigned long syncInterval = 86400000; // 24 hours in milliseconds

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
    updateDisplay(syncStatus, "", "");
    Serial.println(syncStatus);
}

void loop() {
    Serial.println("Entering Loop");

    // Check if it's time to sync
    if (millis() - lastSyncTime >= syncInterval) {
        Serial.println("Time for Sync");
        String syncStatus = syncTime() ? "Sync successful" : "Sync unsuccessful";
        updateDisplay(syncStatus, "", "");
        Serial.println(syncStatus);
        lastSyncTime = millis();
    } else {
        Serial.println("Regular Update");
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

    // Remove or adjust this delay as needed
     delay(10000);  // Delay for 10 seconds
}

void updateTime() {
    if (timeSynced) {
        unsigned long currentMillis = millis();
        unsigned long timeElapsed = currentMillis - lastSyncTime; // Time since last sync in milliseconds
        unsigned long timeElapsedInSeconds = timeElapsed / 1000; // Convert to seconds

        // Calculate current Unix timestamp
        unsigned long currentUnixTime = lastSyncEpoch + timeElapsedInSeconds;

        // Convert current Unix timestamp to struct tm
        time_t rawtime = (time_t)currentUnixTime;
        struct tm * ptm = localtime(&rawtime);

        // Update global time variables
        hour = ptm->tm_hour;
        minute = ptm->tm_min;
        second = ptm->tm_sec;
        day = ptm->tm_mday;
        month = ptm->tm_mon + 1; // tm_mon is months since January (0-11)
        year = ptm->tm_year + 1900; // tm_year is years since 1900
        dayOfWeek = ptm->tm_wday; // Sunday = 0, Monday = 1, ...

        Serial.print("Updated Time: ");
        Serial.print(hour);
        Serial.print(":");
        Serial.print(minute);
        Serial.println(); // Don't print seconds
    } else {
        Serial.println("Time not synced yet.");
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
    Serial.println("Received payload from WorldTime API");
    Serial.println(payload); // Print the payload for debugging

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    String datetime = doc["datetime"].as<String>(); // Extract datetime field

    // Parse the datetime string, ignoring milliseconds and timezone offset
    // Format: YYYY-MM-DDTHH:MM:SS (ISO 8601 format)
    sscanf(datetime.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    // Set lastSyncEpoch to the current Unix time for future calculations
    // Assuming that the API returns the unixtime in UTC, convert it to local time
    lastSyncEpoch = doc["unixtime"].as<unsigned long>() + doc["raw_offset"].as<long>() + doc["dst_offset"].as<long>();

    timeSynced = true;

    WiFi.disconnect(true);

    return true;
}





String getFormattedDate() {
    char dateBuffer[20];
    // Format the date and time without seconds
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
    // Adjust dayOfWeek to be 1 for Monday, 7 for Sunday
    dayOfWeek = dayOfWeek == 0 ? 7 : dayOfWeek;

    if (dayOfWeek >= 1 && dayOfWeek <= 5) {  // Weekday check
        if ((month >= 11 || month <= 3)) {  // Winter months
            if ((hour == 5 && minute >= 55) || // 5 minutes before peak time starts in winter
                (hour > 5 && hour < 10) || // peak time
                (hour == 10 && minute < 5)) { // 5 minutes after peak time ends in winter
                return true;
            }
        } else if (month >= 4 && month <= 10) {  // Summer months
            if ((hour == 12 && minute >= 55) || // 5 minutes before peak time starts in summer
                (hour > 12 && hour < 20) || // peak time
                (hour == 20 && minute < 5)) { // 5 minutes after peak time ends in summer
                return true;
            }
        }
    }
    return false;  // Non-peak time
}
