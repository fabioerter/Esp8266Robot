#include <Arduino.h>
int log_interval, index_ptr, log_count, log_interval, table_size;
String DataFile;
typedef struct
{
  int lcnt;     // Sequential log count
  String ltime; // Time record of when reading was taken
  sint16 temp;  // Temperature values, short unsigned 16-bit integer to reduce memory requirement, saved as x10 more to preserve 0.1 resolution
  sint16 humi;  // Humidity values, short unsigned 16-bit integer to reduce memory requirement, saved as x10 more to preserve 0.1 resolution
} record_type;
record_type sensor_data[];