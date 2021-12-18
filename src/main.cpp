/* ESP8266 plus WEMOS SHT30-D Sensor with a Temperature and Humidity Web Server
 Automous display of sensor results on a line-chart, gauge view and the ability to export the data via copy/paste for direct input to MS-Excel
 The 'MIT License (MIT) Copyright (c) 2016 by David Bird'. Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
 following conditions:
   The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
   software use is visible to an end-user.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
See more at http://www.dsbird.org.uk
*/

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#define TZone 0 // or set you your requirements e.g. -5 for EST
//#include <Server.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP; //** NTP client class
NTPClient timeClient(ntpUDP);
#include <fs.h>
#else
#include <WiFi.h>
#include <ESP32WebServer.h> //https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <WiFiClient.h>
#include "FS.h"
#include "SPIFFS.h"
#endif
#include <SPI.h>
#include <time.h>
#include <wifi/wifi.h>
#include <FileLogger/FileLogger.h>
#include "credentials.h"

String buf;
String version = "v1.0"; // Version of this program
String site_width = "1023";
WiFiClient client;
ESP8266WebServer server(80); // Start server on port 80 (default for a web-browser, change to your requirements, e.g. 8080 if your Router uses port 80
                             // To access server from the outside of a WiFi network e.g. ESP8266WebServer server(8266); and then add a rule on your Router that forwards a
                             // connection request to http://your_network_ip_address:8266 to port 8266 and view your ESP server from anywhere.
                             // Example http://g6ejd.uk.to:8266 will be directed to http://192.168.0.40:8266 or whatever IP address your router gives to this server

int timer_cnt, max_temp, min_temp;
String webpage, time_now, log_time, lastcall, time_str;
bool AScale, AUpdate, log_delete_approved;
float temp, humi;
typedef signed short sint16;

void append_page_header()
{
  webpage = "<!DOCTYPE html><head>";
  if (AUpdate)
    webpage += F("<meta http-equiv='refresh' content='30'>"); // 30-sec refresh time, test needed to prevent auto updates repeating some commands
  webpage += F("<title>Logger</title>");
  webpage += F("<style>ul{list-style-type:none;margin:0;padding:0;overflow:hidden;background-color:#31c1f9;font-size:14px;}");
  webpage += F("li{float:left;}");
  webpage += F("li a{display:block;text-align:center;padding:5px 25px;text-decoration:none;}");
  webpage += F("li a:hover{background-color:#FFFFFF;}");
  webpage += F("h1{background-color:#31c1f9;}");
  webpage += F("body{width:");
  webpage += site_width;
  webpage += F("px;margin:0 auto;font-family:arial;font-size:14px;text-align:center;color:#ed6495;background-color:#F7F2Fd;}");
  webpage += F("</style></head><body><h1>Data Logger ");
  webpage += version + "</h1>";
}

void append_page_footer()
{ // Saves repeating many lines of code for HTML page footers
  webpage += F("<ul>");
  webpage += F("<li><a href='/TH'>Temp&deg;/Humi</a></li>");
  webpage += F("<li><a href='/TD'>Temp&deg;/DewP</a></li>");
  webpage += F("<li><a href='/DV'>Dial</a></li>");
  webpage += F("<li><a href='/LgTU'>Records&dArr;</a></li>");
  webpage += F("<li><a href='/LgTD'>Records&uArr;</a></li>");
  webpage += F("<li><a href='/AS'>AutoScale(");
  webpage += String((AScale ? "ON" : "OFF")) + ")</a></li>";
  webpage += F("<li><a href='/AU'>Refresh(");
  webpage += String((AUpdate ? "ON" : "OFF")) + ")</a></li>";
  webpage += F("<li><a href='/Setup'>Setup</a></li>");
  webpage += F("<li><a href='/Help'>Help</a></li>");
  webpage += F("<li><a href='/LogS'>Log Size</a></li>");
  webpage += F("<li><a href='/LogV'>Log View</a></li>");
  webpage += F("<li><a href='/LogE'>Log Erase</a></li>");
  webpage += F("</ul><footer><p ");
  webpage += F("style='background-color:#a3b2f7'>&copy;");
  char HTML[15] = {0x40, 0x88, 0x5c, 0x98, 0x5C, 0x84, 0xD2, 0xe4, 0xC8, 0x40, 0x64, 0x60, 0x62, 0x70, 0x00};
  for (byte c = 0; c < 15; c++)
  {
    HTML[c] >>= 1;
  }
  webpage += String(HTML) + F("</p>\n");
  webpage += F("</footer></body></html>");
}

String calcDateTime(int epoch)
{ // From UNIX time becuase google charts can use UNIX time
  int seconds, minutes, hours, dayOfWeek, current_day, current_month, current_year;
  seconds = epoch;
  minutes = seconds / 60;  // calculate minutes
  seconds -= minutes * 60; // calculate seconds
  hours = minutes / 60;    // calculate hours
  minutes -= hours * 60;
  current_day = hours / 24; // calculate days
  hours -= current_day * 24;
  current_year = 1970; // Unix time starts in 1970
  dayOfWeek = 4;       // on a Thursday
  while (1)
  {
    bool leapYear = (current_year % 4 == 0 && (current_year % 100 != 0 || current_year % 400 == 0));
    uint16_t daysInYear = leapYear ? 366 : 365;
    if (current_day >= daysInYear)
    {
      dayOfWeek += leapYear ? 2 : 1;
      current_day -= daysInYear;
      if (dayOfWeek >= 7)
        dayOfWeek -= 7;
      ++current_year;
    }
    else
    {
      dayOfWeek += current_day;
      dayOfWeek %= 7;
      /* calculate the month and day */
      static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for (current_month = 0; current_month < 12; ++current_month)
      {
        uint8_t dim = daysInMonth[current_month];
        if (current_month == 1 && leapYear)
          ++dim; // add a day to February if a leap year
        if (current_day >= dim)
          current_day -= dim;
        else
          break;
      }
      break;
    }
  }
  current_month += 1; // Months are 0..11 and returned format is dd/mm/ccyy hh:mm:ss
  current_day += 1;
  String date_time = (current_day < 10 ? "0" + String(current_day) : String(current_day)) + "/" + (current_month < 10 ? "0" + String(current_month) : String(current_month)) + "/" + String(current_year).substring(2) + " ";
  date_time += ((hours < 10) ? "0" + String(hours) : String(hours)) + ":";
  date_time += ((minutes < 10) ? "0" + String(minutes) : String(minutes)) + ":";
  date_time += ((seconds < 10) ? "0" + String(seconds) : String(seconds));
  return date_time;
}
void display_dial()
{                              // Processes a clients request for a dial-view of the data
  log_delete_approved = false; // PRevent accidental SD-Card deletion
  webpage = "";                // don't delete this command, it ensures the server works reliably!
  append_page_header();
  webpage += F("<script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>");
  webpage += F("<script type=\"text/javascript\">");
  webpage += "var temp=" + String(temp / 10, 2) + ",humi=" + String(humi / 10, 1) + ";";
  // https://developers.google.com/chart/interactive/docs/gallery/gauge
  webpage += F("google.load('visualization','1',{packages: ['gauge']});");
  webpage += F("google.setOnLoadCallback(drawgaugetemp);");
  webpage += F("google.setOnLoadCallback(drawgaugehumi);");
  webpage += F("var gaugetempOptions={min:-20,max:50,yellowFrom:-20,yellowTo:0,greenFrom:0,greenTo:30,redFrom:30,redTo:50,minorTicks:10,majorTicks:['-20','-10','0','10','20','30','40','50']};");
  webpage += F("var gaugehumiOptions={min:0,max:100,yellowFrom:0,yellowTo:25,greenFrom:25,greenTo:75,redFrom:75,redTo:100,minorTicks:10,majorTicks:['0','10','20','30','40','50','60','70','80','90','100']};");
  webpage += F("var gaugetemp,gaugehumi;");
  webpage += F("function drawgaugetemp() {gaugetempData = new google.visualization.DataTable();");
  webpage += F("gaugetempData.addColumn('number','deg-C');"); // 176 is Deg-symbol, there are problems displaying the deg-symbol in google charts
  webpage += F("gaugetempData.addRows(1);gaugetempData.setCell(0,0,temp);");
  webpage += F("gaugetemp = new google.visualization.Gauge(document.getElementById('gaugetemp_div'));");
  webpage += F("gaugetemp.draw(gaugetempData, gaugetempOptions);}");
  webpage += F("function drawgaugehumi() {gaugehumiData = new google.visualization.DataTable();gaugehumiData.addColumn('number','%');");
  webpage += F("gaugehumiData.addRows(1);gaugehumiData.setCell(0,0,humi);");
  webpage += F("gaugehumi = new google.visualization.Gauge(document.getElementById('gaugehumi_div'));");
  webpage += F("gaugehumi.draw(gaugehumiData,gaugehumiOptions);};");
  webpage += F("</script>");
  webpage += F("<div id='gaugetemp_div' style='width:300px;height:300px;display:block;margin:0 auto;margin-left:auto;margin-right:auto;'></div>");
  webpage += F("<div id='gaugehumi_div' style='width:300px;height:300px;display:block;margin:0 auto;margin-left:auto;margin-right:auto;'></div>");
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";
  lastcall = "dial";
}

// After the data has been displayed, select and copy it, then open Excel and Paste-Special and choose Text, then select, then insert graph to view
void LOG_view()
{
#ifdef ESP8266
  File datafile = SPIFFS.open(DataFile, "r"); // Now read data from SPIFFS
#else
  File datafile = SPIFFS.open("/" + DataFile, FILE_READ); // Now read data from FS
#endif
  if (datafile)
  {
    if (datafile.available())
    { // If data is available and present
      String dataType = "application/octet-stream";
      if (server.streamFile(datafile, dataType) != datafile.size())
      {
        Serial.print(F("Sent less data than expected!"));
      }
    }
  }
  datafile.close(); // close the file:
  webpage = "";
}

void readFile(void)
{
  // Faz a leitura do arquivo
  File rFile = SPIFFS.open(DataFile, "r");
  Serial.println("Reading file...");
  rFile.println("Log: Reading file.");

  while (rFile.available())
  {
    String line = rFile.readStringUntil('\n');
    buf += line;
    buf += "<br />";
  }
  rFile.println("Log: Finish of reading.");
  rFile.close();
}

void LOG_erase()
{               // Erase the datalog file
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  if (AUpdate)
    webpage += "<meta http-equiv='refresh' content='30'>"; // 30-sec refresh time and test is needed to stop auto updates repeating some commands
  if (log_delete_approved)
  {
#ifdef ESP8266
    if (SPIFFS.exists(DataFile))
    {
      SPIFFS.remove(DataFile);
      Serial.println(F("File deleted successfully"));
    }
#else
    if (SPIFFS.exists("/" + DataFile))
    {
      SPIFFS.remove("/" + DataFile);
      Serial.println(F("File deleted successfully"));
    }
#endif
    webpage += "<h3 style=\"color:orange;font-size:24px\">Log file '" + DataFile + "' has been erased</h3>";
    log_count = 0;
    index_ptr = 0;
    timer_cnt = 2000;            // To trigger first table update, essential
    log_delete_approved = false; // Re-enable FS deletion
  }
  else
  {
    log_delete_approved = true;
    webpage += "<h3 style=\"color:orange;font-size:24px\">Log file erasing is now enabled, repeat this option to erase the log. Graph or Dial Views disable erasing again</h3>";
  }
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";
}

void LOG_stats()
{               // Display file size of the datalog file
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  File datafile = SPIFFS.open(DataFile, "r"); // Now read data from SPIFFS

  webpage += "<h3 style=\"color:orange;font-size:24px\">Data Log file size = " + String(datafile.size()) + "-Bytes</h3>";
  webpage += "<h3 style=\"color:orange;font-size:24px\">Number of readings = " + String(log_count) + "</h3>";
  datafile.close();
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";
}

void setup()
{
  Serial.begin(115200);
  StartSPIFFS();
  // SPIFFS.remove("/"+DataFile); // In case file in ESP32 version gets corrupted, it happens a lot!!
  StartWiFi(ssid, password);
  StartTime();
  Serial.println(F("WiFi connected.."));
  server.begin();
  Serial.println(F("Webserver started..."));                                            // Start the webserver
  Serial.println("Use this URL to connect: http://" + WiFi.localIP().toString() + "/"); // Print the IP address
  //----------------------------------------------------------------------
  server.on("/LogV", LOG_view);
  server.on("/LogE", LOG_erase);
  server.on("/LogS", LOG_stats);
  server.begin();
  Serial.println(F("Webserver started...")); // Start the webserver

  index_ptr = 0;          // The array pointer that varies from 0 to table_size
  log_count = 0;          // Keeps a count of readings taken
  AScale = false;         // Google charts can AScale axis, this switches the function on/off
  max_temp = 30;          // Maximum displayed temperature as default
  min_temp = -10;         // Minimum displayed temperature as default
  timer_cnt = log_interval + 1;                    // To trigger first table update, essential
  AUpdate = true;         // Used to prevent a command from continually auto-updating, for example increase temp-scale would increase every 30-secs if not prevented from doing so.
  lastcall = "temp_humi"; // To determine what requested the AScale change

  update_log_time();           // Update the log_time
  log_delete_approved = false; // Used to prevent accidental deletion of card contents, requires two approvals
  // Serial.println(system_get_free_heap_size()); // diagnostic print to check for available RAM
}

void loop()
{
  server.handleClient();
  time_t now = time(nullptr);
  log_interval = log_time_unit * 10;               // inter-log time interval, default is 5-minutes between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
  
  if (time_now != "Thu Jan 01 00:00:00 1970" and timer_cnt >= log_interval)
  {                                          // If time is not yet set, returns 'Thu Jan 01 00:00:00 1970') so wait.
    timer_cnt = 0;                           // log_interval values are 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
    log_count += 1;                          // Increase logging event count
    sensor_data[index_ptr].lcnt = log_count; // Record current log number, time, temp and humidity readings
    sensor_data[index_ptr].temp = temp;
    sensor_data[index_ptr].humi = humi;
    sensor_data[index_ptr].ltime = calcDateTime(time(&now)); // time stamp of reading 'dd/mm/yy hh:mm:ss'
#ifdef ESP8266
    File datafile = SPIFFS.open(DataFile, "a+");
#else
    File datafile = SPIFFS.open("/" + DataFile, FILE_APPEND);
#endif
    time_t now = time(nullptr);
    if (datafile == true)
    {                                                                                                                                                                                   // if the file is available, write to it
      datafile.println(((log_count < 10) ? "0" : "") + String(log_count) + char(9) + String(temp / 10, 2) + char(9) + String(humi / 10, 2) + char(9) + calcDateTime(time(&now)) + "."); // TAB delimited
      Serial.println(((log_count < 10) ? "0" : "") + String(log_count) + " New Record Added");
    }
    datafile.close();
    index_ptr += 1; // Increment data record pointer
    if (index_ptr > table_size)
    { // if number of readings exceeds max_readings (e.g. 100) shift all data to the left and effectively scroll the display left
      index_ptr = table_size;
      for (int i = 0; i < table_size; i++)
      { // If data table is full, scroll all readings to the left in graphical terms, then add new reading to the end
        sensor_data[i].lcnt = sensor_data[i + 1].lcnt;
        sensor_data[i].temp = sensor_data[i + 1].temp;
        sensor_data[i].humi = sensor_data[i + 1].humi;
        sensor_data[i].ltime = sensor_data[i + 1].ltime;
      }
      sensor_data[table_size].lcnt = log_count;
      sensor_data[table_size].temp = temp;
      sensor_data[table_size].humi = humi;
      sensor_data[table_size].ltime = calcDateTime(time(&now));
    }
  }
  timer_cnt += 1; // Readings set by value of log_interval each 40 = 1min
  delay(500);     // Delay before next check for a client, adjust for 1-sec repeat interval. Temperature readings take some time to complete.
  // Serial.println(millis()); // Used to measure inter-sample timing
}
