#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <heltec.h>
#include <time.h>

// Network credentials
const char* ssid = "xxxx";
const char* password = "xxxx";

// Initialize NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -5 * 3600, 60000); // Adjust for your timezone where -5 is US Eastern, change to yours

// Heater control pin
const int heaterPin = 5;

void setup() {
    Heltec.begin(true, false, true);  // Initialize Heltec module
    Serial.begin(115200);
    Heltec.display->init();
    Heltec.display->flipScreenVertically();
    Heltec.display->setFont(ArialMT_Plain_10);

    WiFi.begin(ssid, password);  // Connect to WiFi
    while (WiFi.status() != WL_CONNECTED) {
        delay(5000);
        updateDisplay("Connecting to WiFi...", "", "");
    }

    timeClient.begin();  // Start NTP client
    pinMode(heaterPin, OUTPUT);  // Initialize heater control pin
}


void loop() {
    String currentTime = getFormattedDate();
    String wifiStatus = WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected";
    String heaterStatus = digitalRead(heaterPin) == HIGH ? "WATER HEATER ON" : "WATER HEATER OFF";

    if (isPeakTime()) {
        digitalWrite(heaterPin, LOW);  // Turn off heater during peak time
    } else {
        digitalWrite(heaterPin, HIGH);  // Turn on heater during off-peak time
    }

    updateDisplay(currentTime, wifiStatus, heaterStatus);
    delay(10000);  // Delay for 10 seconds
}

void updateDisplay(const String& time, const String& wifiStatus, const String& heaterStatus) {
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->drawString(64, 0, time);
    Heltec.display->drawString(64, 12, wifiStatus);
    Heltec.display->drawString(64, 24, heaterStatus);
    Heltec.display->display();
}

bool isDaylightSavingTime(struct tm *timeinfo) {
    int month = timeinfo->tm_mon + 1;  // tm_mon: months since January [0-11]
    int mday = timeinfo->tm_mday;
    int wday = timeinfo->tm_wday;

    if (month < 3 || month > 11) {
        return false; // DST is not in effect in Jan, Feb, and Dec
    }
    if (month > 3 && month < 11) {
        return true; // DST is in effect from April to October
    }

    // In March and November, check if we are past the second Sunday
    if ((month == 3 && (mday - wday) >= 8) || (month == 11 && (mday - wday) < 1)) {
        return true;
    }

    return false;
}

String getFormattedDate() {
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime); 

    // Adjust for DST if necessary
    if (isDaylightSavingTime(ptm)) {
        epochTime += 3600; // Add one hour
        ptm = gmtime((time_t *)&epochTime);
    }

    char dateBuffer[20];
    sprintf(dateBuffer, "%04d-%02d-%02d %02d:%02d:%02d", 
            ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    return String(dateBuffer);
}

bool isPeakTime() {
    struct tm currentTime = getCurrentTime();
    int dayOfWeek = currentTime.tm_wday; // tm_wday: days since Sunday [0-6]
    int month = currentTime.tm_mon + 1;  // tm_mon: months since January [0-11]
    int hour = currentTime.tm_hour;
    int minute = currentTime.tm_min;

    // Adjust dayOfWeek to be 1 for Monday, 7 for Sunday
    dayOfWeek = dayOfWeek == 0 ? 7 : dayOfWeek;

    if (dayOfWeek >= 1 && dayOfWeek <= 5) {  // Weekday check
        if ((month >= 11 || month <= 3) && (hour > 6 || (hour == 6 && minute > 0)) && hour < 10) {
            return true;  // Winter peak time
        } else if (month >= 4 && month <= 10 && (hour > 13 || (hour == 13 && minute > 0)) && hour < 20) {
            return true;  // Summer peak time
        }
    }
    return false;  // Non-peak time
}


struct tm getCurrentTime() {
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    return *gmtime((time_t *)&epochTime); 
}