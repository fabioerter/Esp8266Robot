////////////// SPIFFS Support ////////////////////////////////
// For ESP8266 See: http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html
#include <Arduino.h>
#include "FS.h"

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
int log_time_unit = 15;    // default is 1-minute between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
log_interval = log_time_unit * 10; // inter-log time interval, default is 5-minutes between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
timer_cnt = log_interval + 1;      // To trigger first table update, essential

void update_log_time()
{
  float log_hrs;
  log_hrs = table_size * log_interval / time_reference;
  log_hrs = log_hrs / 60.0; // Should not be needed, but compiler can't calculate the result in-line!
  float log_mins = (log_hrs - int(log_hrs)) * 60;
  log_time = String(int(log_hrs)) + ":" + ((log_mins < 10) ? "0" + String(int(log_mins)) : String(int(log_mins))) + " Hrs  of readings, (" + String(log_interval) + ")secs per reading";
  //log_time += ", Free-mem:("+String(system_get_free_heap_size())+")";
}

void StartSPIFFS()
{
  boolean SPIFFS_Status;
  SPIFFS_Status = SPIFFS.begin();
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
