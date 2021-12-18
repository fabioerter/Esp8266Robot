
////////////// WiFi, Time and Date Functions /////////////////
#include <arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <fs.h>
#include <ESP8266WebServer.h>
WiFiUDP ntpUDP; //** NTP client class
NTPClient timeClient(ntpUDP);
#define TZone 0 // or set you your requirements e.g. -5 for EST
String webpage, time_now, log_time, lastcall, time_str, DataFile = "datalog.txt";

int StartWiFi(const char *ssid, const char *password)
{
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to: "));
  Serial.println(String(ssid));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (connAttempts > 20)
    {
      Serial.println("\nFailed to connect to a Wi-Fi network");
      return -5;
    }
    connAttempts++;
  }
  Serial.print(F("WiFi connected at: "));
  Serial.println(WiFi.localIP());
  return 1;
}

#ifdef ESP8266
void StartTime()
{
  // Note: The ESP8266 Time Zone does not function e.g. ,0,"time.nist.gov"
  configTime(TZone * 3600, 0, "pool.ntp.org", "time.nist.gov");
  // Change this line to suit your time zone, e.g. USA EST configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  // Change this line to suit your time zone, e.g. AUS configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println(F("\nWaiting for time"));
  while (!time(nullptr))
  {
    delay(500);
  }
  Serial.println("Time set");
  timeClient.begin();
}

String GetTime()
{
  time_t now = time(nullptr);
  struct tm *now_tm;
  int hour, min, second, day, month, year, dow;
  now = time(NULL);
  now_tm = localtime(&now);
  hour = now_tm->tm_hour;
  min = now_tm->tm_min;
  second = now_tm->tm_sec;
  day = now_tm->tm_mday;
  month = now_tm->tm_mon + 1;
  year = now_tm->tm_year - 100; // To get just YY information
  String days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  dow = ((timeClient.getEpochTime() / 86400L) + 4) % 7;
  time_str = (day < 10 ? "0" + String(day) : String(day)) + "/" +
             (month < 10 ? "0" + String(month) : String(month)) + "/" +
             (year < 10 ? "0" + String(year) : String(year)) + " ";
  time_str = (hour < 10 ? "0" + String(hour) : String(hour)) + ":" + (min < 10 ? "0" + String(min) : String(min)) + ":" + (second < 10 ? "0" + String(second) : String(second));
  Serial.println(time_str);
  return time_str; // returns date-time formatted as "11/12/17 22:01:00"
}

byte calc_dow(int y, int m, int d)
{
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
#else
void StartTime()
{
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov");
  setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1); // Change for your location
  UpdateLocalTime();
}

void UpdateLocalTime()
{
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  char output[50];
  strftime(output, 50, "%a %d-%b-%y  (%H:%M:%S)", &timeinfo);
  time_str = output;
}

String GetTime()
{
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time - trying again");
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  Serial.println(&timeinfo, "%a %b %d %Y %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  char output[50];
  strftime(output, 50, "%d/%m/%y %H:%M:%S", &timeinfo); //Use %m/%d/%y for USA format
  time_str = output;
  Serial.println(time_str);
  return time_str; // returns date-time formatted like this "11/12/17 22:01:00"
}
#endif
