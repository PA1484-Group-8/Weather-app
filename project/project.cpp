#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LV_Helper.h>
#include <LilyGo_AMOLED.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <lvgl.h>
#include <time.h>

// Wi-Fi credentials
static const char *WIFI_SSID = "";
static const char *WIFI_PASSWORD = "";

LilyGo_Class amoled;

static lv_obj_t *tileview;
static lv_obj_t *t0; // Boot screen tile (Screen 1)
static lv_obj_t *t1; // Forecast (Screen 2)
static lv_obj_t *t2; // Historical (Screen 3)
static lv_obj_t *t3; // Settings
static lv_obj_t *t4; // Wifi

static lv_obj_t *t0_label;
static lv_obj_t *t1_label;

// --- HISTORICAL DATA WIDGETS (For t2) ---
static lv_obj_t *history_chart;
static lv_chart_series_t *history_series;
static lv_obj_t *history_slider;
static lv_obj_t *history_location_label; // Shows the selected city name
static lv_obj_t *history_info_label;     // Shows parameter and value
static lv_obj_t *history_datetime_label; // Shows the date/time of the slider position
static const int CHART_WINDOW_SIZE = 24; // Show 24 hours at a time

static lv_obj_t *t4_label;

// track Wi-Fi connection
static bool wifi_was_connected = false;
static unsigned long last_wifi_update = 0;
bool ui_updated = true;

Preferences preferences;

// --- WEATHER STRUCTURES (Unchanged) ---
struct WeatherCondition
{
  enum Value : int
  {
    Unknown = 0,
    ClearSky = 1,
    NearlyClearSky = 2,
    VariableCloudiness = 3,
    HalfClearSky = 4,
    CloudySky = 5,
    Overcast = 6,
    Fog = 7,
    LightRainShowers = 8,
    ModerateRainShowers = 9,
    HeavyRainShowers = 10,
    Thunderstorm = 11,
    LightSleetShowers = 12,
    ModerateSleetShowers = 13,
    HeavySleetShowers = 14,
    LightSnowShowers = 15,
    ModerateSnowShowers = 16,
    HeavySnowShowers = 17,
    LightRain = 18,
    ModerateRain = 19,
    HeavyRain = 20,
    Thunder = 21,
    LightSleet = 22,
    ModerateSleet = 23,
    HeavySleet = 24,
    LightSnowfall = 25,
    ModerateSnowfall = 26,
    HeavySnowfall = 27
  };
  Value value;
  WeatherCondition() = default;
  WeatherCondition(int code) { value = static_cast<Value>(code); }
};

const char *getWeatherSymbol(WeatherCondition symbol)
{
  switch (symbol.value)
  {
  case WeatherCondition::ClearSky:
  case WeatherCondition::NearlyClearSky:
    return "☀";
  case WeatherCondition::VariableCloudiness:
  case WeatherCondition::HalfClearSky:
    return "⛅";
  case WeatherCondition::CloudySky:
  case WeatherCondition::Overcast:
    return "☁";
  case WeatherCondition::Fog:
    return "🌫";
  case WeatherCondition::LightRainShowers:
  case WeatherCondition::ModerateRainShowers:
  case WeatherCondition::LightRain:
  case WeatherCondition::ModerateRain:
    return "🌧";
  case WeatherCondition::HeavyRainShowers:
  case WeatherCondition::HeavyRain:
    return "⛈";
  case WeatherCondition::Thunderstorm:
  case WeatherCondition::Thunder:
    return "⚡";
  case WeatherCondition::LightSleetShowers:
  case WeatherCondition::ModerateSleetShowers:
  case WeatherCondition::HeavySleetShowers:
  case WeatherCondition::LightSleet:
  case WeatherCondition::ModerateSleet:
  case WeatherCondition::HeavySleet:
    return "🌨";
  case WeatherCondition::LightSnowShowers:
  case WeatherCondition::ModerateSnowShowers:
  case WeatherCondition::HeavySnowShowers:
  case WeatherCondition::LightSnowfall:
  case WeatherCondition::ModerateSnowfall:
  case WeatherCondition::HeavySnowfall:
    return "❄";
  default:
    return "?";
  }
}

const char *getWeatherString(WeatherCondition symbol)
{
  switch (symbol.value)
  {
  case WeatherCondition::ClearSky:
    return "Clear";
  case WeatherCondition::NearlyClearSky:
    return "Mostly Clear";
  case WeatherCondition::VariableCloudiness:
    return "Partly Cloudy";
  case WeatherCondition::HalfClearSky:
    return "Partly Cloudy";
  case WeatherCondition::CloudySky:
    return "Cloudy";
  case WeatherCondition::Overcast:
    return "Overcast";
  case WeatherCondition::Fog:
    return "Fog";
  case WeatherCondition::LightRainShowers:
    return "Light Rain";
  case WeatherCondition::ModerateRainShowers:
    return "Rain";
  case WeatherCondition::HeavyRainShowers:
    return "Heavy Rain";
  case WeatherCondition::Thunderstorm:
    return "Thunderstorm";
  case WeatherCondition::LightSleetShowers:
    return "Light Sleet";
  case WeatherCondition::ModerateSleetShowers:
    return "Sleet";
  case WeatherCondition::HeavySleetShowers:
    return "Heavy Sleet";
  case WeatherCondition::LightSnowShowers:
    return "Light Snow";
  case WeatherCondition::ModerateSnowShowers:
    return "Snow";
  case WeatherCondition::HeavySnowShowers:
    return "Heavy Snow";
  case WeatherCondition::LightRain:
    return "Light Rain";
  case WeatherCondition::ModerateRain:
    return "Rain";
  case WeatherCondition::HeavyRain:
    return "Heavy Rain";
  case WeatherCondition::Thunder:
    return "Thunder";
  case WeatherCondition::LightSleet:
    return "Light Sleet";
  case WeatherCondition::ModerateSleet:
    return "Sleet";
  case WeatherCondition::HeavySleet:
    return "Heavy Sleet";
  case WeatherCondition::LightSnowfall:
    return "Light Snow";
  case WeatherCondition::ModerateSnowfall:
    return "Snow";
  case WeatherCondition::HeavySnowfall:
    return "Heavy Snow";
  default:
    return "Unknown";
  }
}

const int FORCAST_TIMESTAMP_SIZE = 20;

struct ForcastHourlyWeather
{
  float temperature;
  char time[FORCAST_TIMESTAMP_SIZE + 1];
  WeatherCondition weatherCondition;
};

struct Parameter
{
  const char *label;
  const char *apiCode;
};

struct HistoricalSeries
{
  static constexpr int MAX_HOURS = 4000;
  float *values = nullptr;
  unsigned long long *timestamps = nullptr;
  int count = 0;
  bool isLoaded = false;
};

struct City
{
  const char *name;
  const char *lat;
  const char *lon;
  const char *stationID;
  ForcastHourlyWeather forecast[7];
  HistoricalSeries history[4];
  bool loaded_forcast;
  bool loaded_historical[4];
};

static City cities[] = {
    {"Karlskrona", "56.16156", "15.58661", "65090"},
    {"Stockholm", "59.33258", "18.0649", "97400"},
    {"Göteborg", "57.708870", "11.974560", "72420"},
    {"Malmö", "55.60587", "13.00073", "53300"},
    {"Kiruna", "67.85572", "20.22513", "180940"}};

static const int CITY_COUNT = sizeof(cities) / sizeof(cities[0]);

static Parameter parameters[] = {{"Temperture", "1"},
                                 {"Humiditiy", "6"},
                                 {"Wind speed", "4"},
                                 {"Air pressure", "9"}};

static const int PARAM_COUNT = sizeof(parameters) / sizeof(parameters[0]);

// current selcetions (indices)
static int selectedCityIndex = 0;
static int selectedParamIndex = 0;

// LVGL widgets on settings screen
static lv_obj_t *city_dropdown;
static lv_obj_t *param_dropdown;
static lv_obj_t *btn_save_default;
static lv_obj_t *btn_reset_defaults;
static lv_obj_t *settings_status_label;

static void apply_tile_colors(lv_obj_t *tile)
{
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(tile, lv_color_white(), 0);
}

static void update_wifi_status()
{
  wl_status_t current_status = WiFi.status();

  if (current_status == WL_CONNECTED && !wifi_was_connected)
  {
    IPAddress ip = WiFi.localIP();
    char buf[64];
    snprintf(buf, sizeof(buf), "Wi-Fi: %s\nIP: %d.%d.%d.%d",
             WiFi.SSID().c_str(), ip[0], ip[1], ip[2], ip[3]);
    lv_label_set_text(t4_label, buf);
    lv_obj_center(t4_label);
    wifi_was_connected = true;
  }
  else if (current_status != WL_CONNECTED && wifi_was_connected)
  {
    lv_label_set_text(t4_label, "Wi-Fi: Connecting...");
    lv_obj_center(t4_label);
    wifi_was_connected = false;
  }
}

// Helper to format timestamp string into "YYYY-MM-DD HH:00"
void formatTimestamp(unsigned long long timestamp, char *output, size_t outputSize)
{
    // 1. Basic Validation: If timestamp is 0, consider it empty
    if (timestamp == 0) { 
        snprintf(output, outputSize, "No Data"); 
        return; 
    }

    
    timestamp = timestamp / 1000; 

    // 2. Convert integer to time_t (standard C time type)
    time_t rawTime = (time_t)timestamp;

    // 3. Convert time_t to a struct tm (broken-down time: year, month, etc.)
    // We use gmtime for UTC to match your previous "Z" (Zulu/UTC) logic.
    // If you have configured the ESP32 timezone and want local time, use localtime(&rawTime) instead.
    struct tm *timeInfo = gmtime(&rawTime);

    if (timeInfo == nullptr) {
        snprintf(output, outputSize, "Invalid Time");
        return;
    }

    // 4. Format the output
    // tm_year is years since 1900, tm_mon is 0-11
    snprintf(output, outputSize, "%04d-%02d-%02d %02d:00", 
             timeInfo->tm_year + 1900, 
             timeInfo->tm_mon + 1, 
             timeInfo->tm_mday, 
             timeInfo->tm_hour);
}

void formatDate(const char *timestamp, char *output, size_t outputSize)
{
  if (strlen(timestamp) < 10)
  {
    snprintf(output, outputSize, "???");
    return;
  }
  int year, month, day;
  sscanf(timestamp, "%d-%d-%d", &year, &month, &day);
  const char *monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  if (month >= 1 && month <= 12)
  {
    snprintf(output, outputSize, "%s %d", monthNames[month - 1], day);
  }
  else
  {
    snprintf(output, outputSize, "???");
  }
}

bool is_it_twelve(const char time[])
{
  char pattern[] = "____-__-___12:00:00_";
  for (int j = 11; j < 15; j++)
  {
    if (pattern[j] != time[j])
      return false;
  }
  return true;
}

// --- Set Chart Range Dynamically ---
void set_chart_range_by_parameter(int param_index)
{
  int min_val, max_val;
  int tick_count = 5;

  // Y-axis label area for 4-digit numbers (e.g., 1050)
  const int Y_TICK_LENGTH = 60;

  if (param_index == 0)
  { // Temperture (C)
    min_val = -20;
    max_val = 30;
  }
  else if (param_index == 1)
  { // Humiditiy (%)
    min_val = 0;
    max_val = 100;
    tick_count = 6;
  }
  else if (param_index == 2)
  { // Wind speed (m/s)
    min_val = 0;
    max_val = 30;
  }
  else if (param_index == 3)
  { // Air pressure (hPa)
    min_val = 950;
    max_val = 1050;
    tick_count = 6;
  }
  else
  {
    min_val = 0;
    max_val = 100; // Default safe range
  }

  lv_chart_set_range(history_chart, LV_CHART_AXIS_PRIMARY_Y, min_val, max_val);

  // Last parameter (60) controls the space for the label
  lv_chart_set_axis_tick(history_chart, LV_CHART_AXIS_PRIMARY_Y, 10, 5, tick_count, 2, true, Y_TICK_LENGTH);
}

// --- LOGIC FOR HISTORY SCROLLING ---
/**
 * @brief Updates the chart to show a window of data ending at `slider_index`
 */
void update_history_view(int slider_index)
{
  // Basic safety checks
  if (!cities[selectedCityIndex].history[selectedParamIndex].isLoaded)
    return;

  HistoricalSeries &current_history = cities[selectedCityIndex].history[selectedParamIndex];
  int total_count = current_history.count;
  float *values = current_history.values;
  unsigned long long *timestamps = current_history.timestamps;

  // Bounds check
  if (slider_index < 0)
    slider_index = 0;
  if (slider_index >= total_count)
    slider_index = total_count - 1;

  // 1. Update the Info Label (Parameter Value)
  char buf[64];
  snprintf(buf, sizeof(buf), "%s: %.1f",
           parameters[selectedParamIndex].label,
           values[slider_index]);
  lv_label_set_text(history_info_label, buf);

  // 2. Update the Date/Time Label
  char time_buf[64];
  formatTimestamp(timestamps[slider_index], time_buf, sizeof(time_buf));
  lv_label_set_text(history_datetime_label, time_buf);

  // 3. Update the Chart
  int start_idx = slider_index - (CHART_WINDOW_SIZE - 1);
  lv_chart_set_point_count(history_chart, CHART_WINDOW_SIZE);

  for (int i = 0; i < CHART_WINDOW_SIZE; i++)
  {
    int current_data_idx = start_idx + i;

    if (current_data_idx >= 0 && current_data_idx < total_count)
    {
      lv_chart_set_next_value(history_chart, history_series, (lv_coord_t)values[current_data_idx]);
    }
    else
    {
      if (total_count > 0 && current_data_idx < 0)
        lv_chart_set_next_value(history_chart, history_series, (lv_coord_t)values[0]);
      else
        lv_chart_set_next_value(history_chart, history_series, 0);
    }
  }
  lv_chart_refresh(history_chart);
}

// Callback for Slider Interaction
static void history_slider_event_cb(lv_event_t *e)
{
  lv_obj_t *slider = lv_event_get_target(e);
  int value = (int)lv_slider_get_value(slider);
  update_history_view(value);
}

/**
 * @brief Updates all UI labels with data from global variables.
 */
void update_ui()
{
  char buffer[2048];
  char dateStr[16];

  // --- Update Tile 1: 7-Day Forecast ---
  snprintf(buffer, sizeof(buffer), "7-Day Forecast (12:00) in %s\n\n", cities[selectedCityIndex].name);

  for (int i = 0; i < 7; i++)
  {
    formatDate(cities[selectedCityIndex].forecast[i].time, dateStr, sizeof(dateStr));
    char line[128];
    snprintf(line, sizeof(line), "%s %s %.1f°C %s\n",
             getWeatherSymbol(cities[selectedCityIndex].forecast[i].weatherCondition),
             dateStr, cities[selectedCityIndex].forecast[i].temperature,
             getWeatherString(cities[selectedCityIndex].forecast[i].weatherCondition));
    strcat(buffer, line);
  }
  lv_label_set_text(t1_label, buffer);
  lv_obj_center(t1_label);

  // --- Update Tile 2 (Historical Data) ---
  int count = cities[selectedCityIndex].history[selectedParamIndex].count;
  
  // 1. Update Location Label
  lv_label_set_text(history_location_label, cities[selectedCityIndex].name);

  // Always update chart range based on the currently selected parameter
  set_chart_range_by_parameter(selectedParamIndex);

  if (count > 0)
  {
    // 2. Configure Slider Range: 0 to (Total items - 1)
    lv_slider_set_range(history_slider, 0, count - 1);

    // 3. Set Slider to "Latest" (Far right)
    lv_slider_set_value(history_slider, count - 1, LV_ANIM_ON);

    // 4. Enable Slider
    lv_obj_clear_state(history_slider, LV_STATE_DISABLED);

    // 5. Update Chart, Info Label, and Time Label
    update_history_view(count - 1);
  }
  else
  {
    lv_label_set_text(history_info_label, parameters[selectedParamIndex].label);
    lv_label_set_text(history_datetime_label, "No Data Loaded"); // Clear time label
    lv_chart_set_point_count(history_chart, 0);       // Clear chart
    lv_obj_add_state(history_slider, LV_STATE_DISABLED); // Disable slider
  }

  lv_label_set_text(settings_status_label, "");
}

// Settings callbacks
void settings_value_changed(lv_event_t *e)
{
  ui_updated = true;
  lv_obj_t *obj = lv_event_get_target(e);
  if (obj == city_dropdown)
  {
    selectedCityIndex = lv_dropdown_get_selected(obj);
    lv_label_set_text(settings_status_label, "City selected - updating UI...");
  }
  else if (obj == param_dropdown)
  {
    selectedParamIndex = lv_dropdown_get_selected(obj);
    lv_label_set_text(settings_status_label, "Parameters selected - updating UI...");
  }
}

static void on_save_defaults(lv_event_t *e)
{
  LV_UNUSED(e);
  preferences.begin("weather", false);
  preferences.putUInt("city_idx", (uint32_t)selectedCityIndex);
  preferences.putUInt("param_idx", (uint32_t)selectedParamIndex);
  preferences.end();
  lv_label_set_text(settings_status_label, "Defaults saved!");
  Serial.println("Defaults saved to Preferences.");
}

static void on_reset_deaults(lv_event_t *e)
{
  LV_UNUSED(e);
  selectedCityIndex = 0;
  selectedParamIndex = 0;
  lv_dropdown_set_selected(city_dropdown, selectedCityIndex);
  lv_dropdown_set_selected(param_dropdown, selectedParamIndex);
  preferences.begin("weather", false);
  preferences.clear();
  preferences.end();
  lv_label_set_text(settings_status_label, "Defaults reset.");
  Serial.println("Preferences cleared and UI reset.");
}

static void get_saved_preferences()
{
  preferences.begin("weather", true);
  selectedCityIndex = preferences.getUInt("city_idx", 0);
  selectedParamIndex = preferences.getUInt("param_idx", 0);
  preferences.end();
  Serial.printf("Loaded Preferences: city_idx=%d, param_idx=%d\n", selectedCityIndex, selectedParamIndex);
}

// Function: Creates UI
static void create_ui()
{
  // --- STYLES FOR SETTINGS TILE ---
  
  // Style for all button and dropdown text (using known available font 28)
  static lv_style_t style_text_large;
  lv_style_init(&style_text_large);
  lv_style_set_text_font(&style_text_large, &montserrat_se_28); 

  // Style for the dropdown container/button part (Custom border/bg)
  static lv_style_t style_dropdown_clean;
  lv_style_init(&style_dropdown_clean);
  lv_style_set_bg_color(&style_dropdown_clean, lv_color_white()); // Fixed args
  lv_style_set_border_width(&style_dropdown_clean, 2); // Fixed args
  lv_style_set_border_color(&style_dropdown_clean, lv_palette_main(LV_PALETTE_BLUE)); // Fixed args
  lv_style_set_text_font(&style_dropdown_clean, &montserrat_se_28); // Fixed args
  lv_style_set_pad_all(&style_dropdown_clean, 5); // Fixed args

  // Style for the dropdown list (Menu)
  static lv_style_t style_dropdown_list;
  lv_style_init(&style_dropdown_list);
  lv_style_set_text_font(&style_dropdown_list, &montserrat_se_28);

  // --- BASE LAYOUT ---
  tileview = lv_tileview_create(lv_scr_act());
  lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

  // Add tiles
  t0 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR); // Boot
  t1 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR); // Forecast
  t2 = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR); // History (Tile 3)
  t3 = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_HOR); // Settings
  t4 = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_HOR); // Wifi

  // Tile #0 - Boot Screen
  lv_obj_set_style_bg_color(t0, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(t0, LV_OPA_COVER, 0);
  t0_label = lv_label_create(t0);
  lv_label_set_text(t0_label, "Group 8\nFirmware v1.2.0");
  lv_obj_set_style_text_font(t0_label, &montserrat_se_28, 0);
  lv_obj_set_style_text_color(t0_label, lv_color_white(), 0);
  lv_obj_center(t0_label);

  // Tile #1 - 7-Day Forecast
  t1_label = lv_label_create(t1);
  lv_label_set_text(t1_label, "Forecast data: Loading...");
  lv_obj_set_style_text_font(t1_label, &montserrat_se_28, 0);
  lv_obj_center(t1_label);
  apply_tile_colors(t1);

  // --- Tile #2 (Screen 3) - Historical Weather ---
  apply_tile_colors(t2);

  // Location Label (Top Center)
  history_location_label = lv_label_create(t2);
  lv_label_set_text(history_location_label, cities[selectedCityIndex].name);
  lv_obj_set_style_text_font(history_location_label, &montserrat_se_28, 0);
  lv_obj_align(history_location_label, LV_ALIGN_TOP_MID, 0, 10);

  // Info Label (Parameter Value)
  history_info_label = lv_label_create(t2);
  lv_label_set_text(history_info_label, "History: Loading...");
  lv_obj_set_style_text_font(history_info_label, &montserrat_se_28, 0);
  lv_obj_align(history_info_label, LV_ALIGN_TOP_MID, 0, 40);

  // Date/Time Label (Below Info)
  history_datetime_label = lv_label_create(t2);
  lv_label_set_text(history_datetime_label, "Date/Time N/A");
  lv_obj_set_style_text_font(history_datetime_label, &montserrat_se_28, 0);
  lv_obj_align(history_datetime_label, LV_ALIGN_TOP_MID, 0, 75); 

  // Chart (Middle)
  history_chart = lv_chart_create(t2);
  lv_obj_set_size(history_chart, 200, 180); 
  lv_obj_align(history_chart, LV_ALIGN_CENTER, 0, 10);
  lv_chart_set_type(history_chart, LV_CHART_TYPE_LINE);

  // Set initial range based on current selection (default is temp)
  set_chart_range_by_parameter(selectedParamIndex);

  lv_chart_set_point_count(history_chart, CHART_WINDOW_SIZE);

  history_series = lv_chart_add_series(history_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

  // Slider (Bottom)
  history_slider = lv_slider_create(t2);
  lv_obj_set_width(history_slider, 200);
  lv_obj_align(history_slider, LV_ALIGN_BOTTOM_MID, 0, -10); 
  lv_obj_add_event_cb(history_slider, history_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_state(history_slider, LV_STATE_DISABLED); // Disabled until data loads


  // --- Tile #3 - Settings ---
  lv_obj_t *t3_label = lv_label_create(t3);
  lv_label_set_text(t3_label, "Settings");
  lv_obj_set_style_text_font(t3_label, &montserrat_se_28, 0); 
  lv_obj_align(t3_label, LV_ALIGN_TOP_MID, 0, 6);
  apply_tile_colors(t3);

  // --- Dropdowns (City & Parameter) ---
  String cityOptions;
  for (int i = 0; i < CITY_COUNT; ++i)
  {
    cityOptions += cities[i].name;
    if (i < CITY_COUNT - 1)
      cityOptions += "\n";
  }
  city_dropdown = lv_dropdown_create(t3);
  lv_dropdown_set_options(city_dropdown, cityOptions.c_str());
  lv_obj_set_size(city_dropdown, 220, 50); // Sized
  lv_obj_align(city_dropdown, LV_ALIGN_TOP_LEFT, 10, 60);
  lv_obj_add_style(city_dropdown, &style_dropdown_clean, LV_PART_MAIN); 
  lv_obj_t *list = lv_dropdown_get_list(city_dropdown);
  lv_obj_add_style(list, &style_dropdown_list, LV_PART_MAIN); 
  lv_dropdown_set_selected(city_dropdown, selectedCityIndex);
  lv_obj_add_event_cb(city_dropdown, settings_value_changed, LV_EVENT_VALUE_CHANGED, NULL);

  String paramOptions;
  for (int i = 0; i < PARAM_COUNT; ++i)
  {
    paramOptions += parameters[i].label;
    if (i < PARAM_COUNT - 1)
      paramOptions += "\n";
  }
  param_dropdown = lv_dropdown_create(t3);
  lv_dropdown_set_options(param_dropdown, paramOptions.c_str());
  lv_obj_set_size(param_dropdown, 220, 50); // Sized
  lv_obj_align(param_dropdown, LV_ALIGN_TOP_LEFT, 10, 130);
  lv_obj_add_style(param_dropdown, &style_dropdown_clean, LV_PART_MAIN); 
  lv_obj_t *list2 = lv_dropdown_get_list(param_dropdown);
  lv_obj_add_style(list2, &style_dropdown_list, LV_PART_MAIN); 
  lv_dropdown_set_selected(param_dropdown, selectedParamIndex);
  lv_obj_add_event_cb(param_dropdown, settings_value_changed, LV_EVENT_VALUE_CHANGED, NULL);

  // --- Buttons ---
  btn_save_default = lv_btn_create(t3);
  lv_obj_align(btn_save_default, LV_ALIGN_TOP_RIGHT, -10, 60);
  lv_obj_set_size(btn_save_default, 180, 50); // Sized
  lv_obj_add_style(btn_save_default, &style_text_large, LV_PART_MAIN);
  lv_obj_t *lbl_save = lv_label_create(btn_save_default);
  lv_label_set_text(lbl_save, "Save Default");
  lv_obj_center(lbl_save);
  lv_obj_add_event_cb(btn_save_default, on_save_defaults, LV_EVENT_CLICKED, NULL);

  btn_reset_defaults = lv_btn_create(t3);
  lv_obj_align(btn_reset_defaults, LV_ALIGN_TOP_RIGHT, -10, 130); // Aligned next to Parameter
  lv_obj_set_size(btn_reset_defaults, 180, 50); // Sized
  lv_obj_add_style(btn_reset_defaults, &style_text_large, LV_PART_MAIN);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset_defaults);
  lv_label_set_text(lbl_reset, "Reset Default");
  lv_obj_center(lbl_reset);
  lv_obj_add_event_cb(btn_reset_defaults, on_reset_deaults, LV_EVENT_CLICKED, NULL);

  // Status label (at bottom)
  settings_status_label = lv_label_create(t3);
  lv_label_set_text(settings_status_label, "");
  lv_obj_set_style_text_font(settings_status_label, &montserrat_se_28, 0); 
  lv_obj_align(settings_status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

  // Tile #4 - Wifi
  t4_label = lv_label_create(t4);
  lv_label_set_text(t4_label, "Wi-Fi: Connecting...");
  lv_obj_set_style_text_font(t4_label, &montserrat_se_28, 0);
  lv_obj_center(t4_label);
  apply_tile_colors(t4);

  lv_obj_set_tile(tileview, t0, LV_ANIM_OFF);
}

// ... (Rest of the standard fetch functions and Allocator) ...
static bool fetchJsonFromServer(const String &url, JsonDocument &doc)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[HTTP] Error: Wi-Fi not connected.");
    return false;
  }
  Serial.printf("[HTTP] Fetching URL: %s\n", url.c_str());
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);
  if (http.begin(url))
  {
    http.addHeader("Accept-Encoding", "identity");
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
      Stream &stream = http.getStream();
      DeserializationError error = deserializeJson(doc, stream);
      http.end();
      if (error)
      {
        Serial.print("[JSON] deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
      }
      Serial.println("[JSON] Parse successful.");
      return true;
    }
    else
    {
      Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
      http.end();
      return false;
    }
  }
  else
  {
    Serial.printf("[HTTP] Unable to connect to %s\n", url.c_str());
    return false;
  }
}

struct SpiRamAllocator
{
  void *allocate(size_t size) { return ps_malloc(size); }
  void deallocate(void *pointer) { free(pointer); }
  void *reallocate(void *ptr, size_t new_size) { return ps_realloc(ptr, new_size); }
};
SpiRamAllocator myPsramAllocator;
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

bool fetchForcast(int c)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }
  SpiRamJsonDocument doc(200000);
  String forecastUrl = "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/";
  forecastUrl += cities[c].lon;
  forecastUrl += "/lat/";
  forecastUrl += cities[c].lat;
  forecastUrl += "/data.json";
  Serial.printf("Fetching Forecast for %s...\n", cities[c].name);
  if (fetchJsonFromServer(forecastUrl, doc))
  {
    JsonArray hours = doc["timeSeries"].as<JsonArray>();
    int skip = 0;
    int next_day = 0;
    for (JsonVariant hour : hours)
    {
      if (skip < 12)
      {
        skip++;
        continue;
      }
      const char *time = hour["time"].as<const char *>();
      if (time != nullptr)
      {
        if (is_it_twelve(time) && next_day < 7)
        {
          ForcastHourlyWeather &hourly = cities[c].forecast[next_day];
          hourly.temperature = hour["data"]["air_temperature"].as<float>();
          hourly.weatherCondition = WeatherCondition(hour["data"]["symbol_code"].as<int>());
          strncpy(hourly.time, time, 20);
          hourly.time[20] = '\0';
          next_day++;
        }
      }
      cities[c].loaded_forcast = true;
    }
    return true;
  }
  return false;
}

bool fetchHistorical(int c, int p)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }
  SpiRamJsonDocument doc(200000);
  String histUrl = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/";
  histUrl += parameters[p].apiCode;
  histUrl += "/station/";
  histUrl += cities[c].stationID;
  histUrl += "/period/latest-months/data.json";
  Serial.printf("Fetching History (%s) for %s...\n", parameters[p].label, cities[c].name);
  if (fetchJsonFromServer(histUrl, doc))
  {
    HistoricalSeries &current_history = cities[c].history[p];

    JsonArray days = doc["value"].as<JsonArray>();
    int idx = 0;
    for (JsonVariant day : days)
    {
      if (idx >= HistoricalSeries::MAX_HOURS)
        break;

      current_history.values[idx] = day["value"].as<float>();

      // 1755226800000
      current_history.timestamps[idx] = day["date"].as<unsigned long long>();
      
      idx++;
    }
    current_history.count = idx;
    current_history.isLoaded = true;
    cities[c].loaded_historical[p] = true;
    
    return true;
  }
  return false;
}

void setup()
{
  for (int i = 0; i < CITY_COUNT; ++i)
  {
    for (int j = 0; j < PARAM_COUNT; ++j)
    {
      // Allocate values array
      cities[i].history[j].values = (float *)ps_malloc(HistoricalSeries::MAX_HOURS * sizeof(float));
      cities[i].history[j].timestamps = (unsigned long long *)ps_malloc(HistoricalSeries::MAX_HOURS * sizeof(unsigned long long));

      if (cities[i].history[j].values == nullptr || cities[i].history[j].timestamps == nullptr)
      {
        Serial.println("FATAL: Failed to allocate historical data memory!");
        while (true)
          ;
      }
      
    }
  }
  Serial.begin(115200);
  delay(200);

  if (!amoled.begin())
  {
    Serial.println("Failed to init LilyGO AMOLED.");
    while (true)
      delay(1000);
  }
  beginLvglHelper(amoled);
  get_saved_preferences();
  create_ui();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
  update_wifi_status();
}

void loop()
{
  if (ui_updated)
  {
    update_ui();
    ui_updated = false;
  }

  lv_timer_handler();
  if (millis() - last_wifi_update > 500)
  {
    update_wifi_status();
    last_wifi_update = millis();
  }

  // Re-fetch data if the user changes city or parameter
  if (cities[selectedCityIndex].loaded_forcast != true)
  {
    if (fetchForcast(selectedCityIndex))
    {
      update_ui();
    }
  }
  if (cities[selectedCityIndex].loaded_historical[selectedParamIndex] != true)
  {
    if (fetchHistorical(selectedCityIndex, selectedParamIndex))
    {
      update_ui();
    }
  }
}
