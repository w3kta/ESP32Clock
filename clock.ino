#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Replace with your network credentials
const char* ssid = "ssid";
const char* password = "pass";

// Set up NTP client with Google time server
WiFiUDP udp;
NTPClient timeClient(udp, "time.google.com", 0, 60000); // Using time.google.com server and a 60-second update interval

// Store date and time
String current_date;
String current_time;

// Store hour, minute
static int32_t hour;
static int32_t minute;
bool sync_time_date = false;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// If logging is enabled, it will inform the user about what is happening in the library
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

String format_time(int time) {
  return (time < 10) ? "0" + String(time) : String(time);
}

static lv_obj_t * text_label_time;
static lv_obj_t * text_label_date;

static void timer_cb(lv_timer_t * timer){
  LV_UNUSED(timer);
  minute++; // Only increment the minute
  if(minute > 59) {
    minute = 0;
    hour++;
    sync_time_date = true;
    if(hour > 23) {
      hour = 0;
    }
  }

  String hour_time_f = format_time(hour);
  String minute_time_f = format_time(minute);

  String final_time_str = String(hour_time_f) + ":" + String(minute_time_f);
  lv_label_set_text(text_label_time, final_time_str.c_str());
  lv_label_set_text(text_label_date, current_date.c_str());
}

void lv_create_main_gui(void) {
  // Get the time and date from NTP
  while(hour==0 && minute==0) {
    get_date_and_time();
  }
  Serial.println("Current Time: " + current_time);
  Serial.println("Current Date: " + current_date);

  lv_timer_t * timer = lv_timer_create(timer_cb, 60000, NULL); // Timer to update every minute
  lv_timer_ready(timer);

  // Create a text label for the time aligned center
  text_label_time = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_time, "");
  lv_obj_align(text_label_time, LV_ALIGN_CENTER, 0, -30);
  
  // Set font type, size, and color for time text
  static lv_style_t style_text_label;
  lv_style_init(&style_text_label);
  lv_style_set_text_font(&style_text_label, &lv_font_montserrat_48);
  lv_style_set_text_color(&style_text_label, lv_color_hex(0xFFFFFF));  // White color
  lv_obj_add_style(text_label_time, &style_text_label, 0);

  // Create a text label for the date aligned center
  text_label_date = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_date, current_date.c_str());
  lv_obj_align(text_label_date, LV_ALIGN_CENTER, 0, 40);
  
  // Set font type, size, and color for date text
  static lv_style_t style_text_label2;
  lv_style_init(&style_text_label2);
  lv_style_set_text_font(&style_text_label2, &lv_font_montserrat_30);
  lv_style_set_text_color(&style_text_label2, lv_color_hex(0xFFFFFF));  // White color
  lv_obj_add_style(text_label_date, &style_text_label2, 0); 
}

bool isDST(int year, int month, int day) {
  // Calculate the last Sunday in March and October
  struct tm timeInfo;
  time_t rawTime;
  
  // Get the last Sunday of March
  timeInfo.tm_year = year - 1900;
  timeInfo.tm_mon = 2; // March (0-based index)
  timeInfo.tm_mday = 31; // Last day of March
  mktime(&timeInfo); // Normalize the date to the correct day of the week
  int lastSundayMarch = timeInfo.tm_wday == 0 ? 31 : 31 - timeInfo.tm_wday; // Last Sunday of March

  // Get the last Sunday of October
  timeInfo.tm_mon = 9; // October
  timeInfo.tm_mday = 31; // Last day of October
  mktime(&timeInfo); // Normalize the date to the correct day of the week
  int lastSundayOctober = timeInfo.tm_wday == 0 ? 31 : 31 - timeInfo.tm_wday; // Last Sunday of October

  // Now check if the current date is between the last Sundays of March and October
  timeInfo.tm_mon = month - 1;  // Convert to 0-based month index
  timeInfo.tm_mday = day;  // Current day

  mktime(&timeInfo);  // Normalize the date to the correct weekday

  // Check if the date is in the DST range
  if ((month > 3 && month < 10) || (month == 3 && day >= lastSundayMarch) || (month == 10 && day <= lastSundayOctober)) {
    return true;  // DST is active
  }
  return false;  // DST is not active
}

void get_date_and_time() {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update(); // Update the time
    unsigned long epochTime = timeClient.getEpochTime(); // Get the time in seconds since Jan 1, 1970

    // Get current date from epoch time
    struct tm *timeinfo;
    time_t rawTime = epochTime;
    timeinfo = localtime(&rawTime);
    int year = timeinfo->tm_year + 1900;
    int month = timeinfo->tm_mon + 1;
    int day = timeinfo->tm_mday;

    // Apply timezone offset for Europe/Budapest
    int timezoneOffset = 1 * 3600; // Default to UTC+1 (Standard Time)

    // Check if DST is active
    if (isDST(year, month, day)) {
      timezoneOffset = 2 * 3600; // UTC+2 for daylight saving time
    }

    // Apply the calculated timezone offset
    epochTime += timezoneOffset;

    // Convert epoch time to readable time (no seconds)
    hour = (epochTime % 86400L) / 3600;
    minute = (epochTime % 3600) / 60;

    // Format current time as hh:mm
    current_time = format_time(hour) + ":" + format_time(minute);

    // Calculate the current date using the epoch time
    timeinfo = localtime(&rawTime);  // Update the time info

    // Format the date as yyyy-mm-dd
    char dateBuffer[11];
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", timeinfo);
    current_date = String(dateBuffer);
  } else {
    Serial.println("Not connected to Wi-Fi");
  }
}

void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to Wi-Fi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Start LVGL
  lv_init();
  // Register print function for debugging
  lv_log_register_print_cb(log_print);

  // Create a display object
  lv_display_t * disp;
  // Initialize the TFT display using the TFT_eSPI library
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);

  // Set background color to black
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);  // Black color

  // Start NTP client
  timeClient.begin();

  // Function to draw the GUI
  lv_create_main_gui();
}

void loop() {
  // Get the time and date from NTP
  if(sync_time_date) {
    sync_time_date = false;
    get_date_and_time();
    while(hour==0 && minute==0) {
      get_date_and_time();
    }
  }
  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  delay(5);           // let this time pass
}
