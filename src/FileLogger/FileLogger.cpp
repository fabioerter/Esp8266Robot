////////////// SPIFFS Support ////////////////////////////////
// For ESP8266 See: http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html
#include <Arduino.h>
#include "FS.h"
#include ".\"

String DataFile = "datalog.txt";
int const table_size = 72; // 80 is about the maximum for the available memory and Google Charts, based on 3 samples/hour * 24 * 1 day = 72 displayed, but not stored!
typedef struct
{
  int lcnt;     // Sequential log count
  String ltime; // Time record of when reading was taken
  sint16 temp;  // Temperature values, short unsigned 16-bit integer to reduce memory requirement, saved as x10 more to preserve 0.1 resolution
  sint16 humi;  // Humidity values, short unsigned 16-bit integer to reduce memory requirement, saved as x10 more to preserve 0.1 resolution
} record_type;
record_type sensor_data[table_size + 1]; // Define the data array
int index_ptr, log_count, log_interval;
bool auto_smooth;
int time_reference = 60; // Time reference for calculating /log-time (nearly in secs) to convert to minutes
int log_time_unit = 15;  // default is 1-minute between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
String webpage, time_now, log_time, lastcall, time_str;
int timer_cnt, max_temp, min_temp;


void update_log_time()
{
  float log_hrs;
  log_hrs = table_size * log_interval / time_reference;
  log_hrs = log_hrs / 60.0; // Should not be needed, but compiler can't calculate the result in-line!
  float log_mins = (log_hrs - int(log_hrs)) * 60;
  log_time = String(int(log_hrs)) + ":" + ((log_mins < 10) ? "0" + String(int(log_mins)) : String(int(log_mins))) + " Hrs  of readings, (" + String(log_interval) + ")secs per reading";
  // log_time += ", Free-mem:("+String(system_get_free_heap_size())+")";
}

void StartSPIFFS()
{
  boolean SPIFFS_Status;
  SPIFFS_Status = SPIFFS.begin();
  log_interval = log_time_unit * 10; // inter-log time interval, default is 5-minutes between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
  timer_cnt = log_interval + 1;      // To trigger first table update, essential
  if (SPIFFS_Status == false)
  { // Most likely SPIFFS has not yet been formated, so do so
    Serial.println("Formatting SPIFFS Please wait .... ");
    if (SPIFFS.format() == true)
      Serial.println("SPIFFS formatted successfully");
    if (SPIFFS.begin() == false)
      Serial.println("SPIFFS failed to start...");
  }
  else
    Serial.println("SPIFFS Started successfully...");
}

void prefill_array()
{ // After power-down or restart and if the FS has readings, load them back in
#ifdef ESP8266
  File datafile = SPIFFS.open(DataFile, "r");
#else
  File datafile = SPIFFS.open("/" + DataFile, FILE_READ);
#endif
  while (datafile.available())
  {                                       // if the file is available, read from it
    int read_ahead = datafile.parseInt(); // Sometimes at the end of file, NULL data is returned, this tests for that
    if (read_ahead != 0)
    { // Probably wasn't null data to use it, but first data element could have been zero and there is never a record 0!
      sensor_data[index_ptr].lcnt = read_ahead;
      sensor_data[index_ptr].temp = datafile.parseFloat() * 10;
      sensor_data[index_ptr].humi = datafile.parseFloat() * 10;
      sensor_data[index_ptr].ltime = datafile.readStringUntil('.');
      sensor_data[index_ptr].ltime.trim();
      index_ptr += 1;
      log_count += 1;
    }
    if (index_ptr > table_size)
    {
      for (int i = 0; i < table_size; i++)
      {
        sensor_data[i].lcnt = sensor_data[i + 1].lcnt;
        sensor_data[i].temp = sensor_data[i + 1].temp;
        sensor_data[i].humi = sensor_data[i + 1].humi;
        sensor_data[i].ltime = sensor_data[i + 1].ltime;
        sensor_data[i].ltime.trim();
      }
      index_ptr = table_size;
    }
  }
  datafile.close();
  // Diagnostic print to check if data is being recovered from SPIFFS correctly
  for (int i = 0; i <= index_ptr; i++)
  {
    Serial.println(((i < 10) ? "0" : "") + String(sensor_data[i].lcnt) + " " + String(sensor_data[i].temp) + " " + String(sensor_data[i].humi) + " " + String(sensor_data[i].ltime));
  }
  datafile.close();
  if (auto_smooth)
  { // During restarts there can be a discontinuity in readings, giving a spike in the graph, this smooths that out, off by default though
    // At this point the array holds data from the FS, but sometimes during outage and resume, reading discontinuity occurs, so try to correct those.
    float last_temp, last_humi;
    for (int i = 1; i < table_size; i++)
    {
      last_temp = sensor_data[i].temp;
      last_humi = sensor_data[i].humi;
      // Correct next reading if it is more than 10% different from last values
      if ((sensor_data[i + 1].temp > (last_temp * 1.1)) || (sensor_data[i + 1].temp < (last_temp / 1.1)))
        sensor_data[i + 1].temp = (sensor_data[i + 1].temp + last_temp) / 2; // +/-1% different then use last value
      if ((sensor_data[i + 1].humi > (last_humi * 1.1)) || (sensor_data[i + 1].humi < (last_humi / 1.1)))
        sensor_data[i + 1].humi = (sensor_data[i + 1].humi + last_humi) / 2;
    }
  }
  Serial.println("Restored data from SPIFFS");
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
