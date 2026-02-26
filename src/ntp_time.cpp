#include "ntp_time.h"
#include <HardwareSerial.h>


const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

struct tm timeinfo;


void setup_time() {
  configTime(gmtOffset_sec, daylightOffset_sec, 
             "pool.ntp.org", "time.nist.gov", "time.google.com");
  
  struct tm t;
  Serial.print("Waiting for NTP sync");
  int retry = 0;
  while (!getLocalTime(&t, 1000) && retry < 15) {
    Serial.print(".");
    retry++;
  }
  Serial.println(retry < 15 ? "Setup time OK" : "Setup time FAILED");
}


struct tm* getDateTime() {
  
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Error: Failed to obtain time");
    return &timeinfo;
  }

  /* 
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");   // Friday, December 12 2025 05:47:17
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
 */

  return &timeinfo;
}
