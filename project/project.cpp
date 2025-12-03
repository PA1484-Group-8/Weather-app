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
static const char *WIFI_SSID = "AN";
static const char *WIFI_PASSWORD = "3feC=Mic@iKsi&Da";

LilyGo_Class amoled;

static lv_obj_t *tileview;
static lv_obj_t *t0; // Boot screen tile
static lv_obj_t *t1;
static lv_obj_t *t2;
static lv_obj_t *t3; // settings
static lv_obj_t *t4; // wifi

static lv_obj_t *t0_label; // Boot screen label
static lv_obj_t *t1_label;
static lv_obj_t *t2_label;

static lv_obj_t *t4_label; // wifi

// track Wi-Fi connection, whenever you need to access the internet you need to
// check that this is true.
static bool wifi_was_connected = false;
static unsigned long last_wifi_update = 0; // track last Wi-Fi update time

Preferences preferences; // for settings

/**
 * @brief Defines the 27 SMHI weather symbol codes that can be gotten for each
 * hour by the Forcast API in particular. For the hourly and historical weather
 * data you can't get these exacts ones from the API it seems. The project
 * description says the following:
 *
 * "As a user, I want to see the weather forecast for the next 7 days for
 * Karlskrona on the second screen in terms of temperature and weather
 * conditions with symbols (e.g., clear sky, rain, snow, thunder) per day at
 * 12:00."
 *
 * So we have to figure out how these codes get translated into actual images of
 * "clear sky" or "rain" etc.
 */
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
  // Construct with integer code
  WeatherCondition(int code) { value = static_cast<Value>(code); }
};

/**
 * @brief Converts a WeatherCondition enum into a weather symbol.
 * Uses Unicode weather symbols that display nicely.
 */
const char *getWeatherSymbol(WeatherCondition symbol)
{
  switch (symbol.value)
  {
  case WeatherCondition::ClearSky:
  case WeatherCondition::NearlyClearSky:
    return "☀"; // Sun
  case WeatherCondition::VariableCloudiness:
  case WeatherCondition::HalfClearSky:
    return "⛅"; // Sun behind cloud
  case WeatherCondition::CloudySky:
  case WeatherCondition::Overcast:
    return "☁"; // Cloud
  case WeatherCondition::Fog:
    return "🌫"; // Fog
  case WeatherCondition::LightRainShowers:
  case WeatherCondition::ModerateRainShowers:
  case WeatherCondition::LightRain:
  case WeatherCondition::ModerateRain:
    return "🌧"; // Cloud with rain
  case WeatherCondition::HeavyRainShowers:
  case WeatherCondition::HeavyRain:
    return "⛈"; // Cloud with rain and lightning
  case WeatherCondition::Thunderstorm:
  case WeatherCondition::Thunder:
    return "⚡"; // Lightning
  case WeatherCondition::LightSleetShowers:
  case WeatherCondition::ModerateSleetShowers:
  case WeatherCondition::HeavySleetShowers:
  case WeatherCondition::LightSleet:
  case WeatherCondition::ModerateSleet:
  case WeatherCondition::HeavySleet:
    return "🌨"; // Cloud with snow
  case WeatherCondition::LightSnowShowers:
  case WeatherCondition::ModerateSnowShowers:
  case WeatherCondition::HeavySnowShowers:
  case WeatherCondition::LightSnowfall:
  case WeatherCondition::ModerateSnowfall:
  case WeatherCondition::HeavySnowfall:
    return "❄"; // Snowflake
  default:
    return "?"; // Unknown
  }
}

/**
 * @brief Converts a WeatherCondition enum into a human-readable string.
 */
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

// the number of chars in the forcast time stamp
const int FORCAST_TIMESTAMP_SIZE = 20;
/**
 * @brief A blueprint for holding one hour of forecast data. It holds the
 * temperature, date, and a weather symbol code for that hour gotten by the SMHI
 * API.
 */
struct ForcastHourlyWeather
{
  float temperature;

  //  example "2025-11-06T14:00:00Z"
  char time[FORCAST_TIMESTAMP_SIZE + 1];
  WeatherCondition weatherCondition;
};

// We will use these indices: 0=Temp, 1=Humidity, 2=Wind, 3=Pressure
struct Parameter
{
  const char *label;
  const char *apiCode;
};

// 2. A container for one specific historical parameter (e.g., Wind Speed) It can hold up to MAX_HOURS entries of data.
struct HistoricalSeries
{
  static constexpr int MAX_HOURS = 4000; // I want to set this to 4000 to store the full API data but then it runs out of memory and won't compile.
  float *values = nullptr;
  int count = 0;
  bool isLoaded = false; // Helpful to check if we actually have data
};

// 3. The Master City Struct
// This holds static config AND dynamic weather data
struct City
{
  // Static Config
  const char *name;
  const char *lat;
  const char *lon;
  const char *stationID;

  // Dynamic Data Storage
  ForcastHourlyWeather forecast[7]; // 7 days of forecast

  HistoricalSeries history[4]; // 4 slots: [0]Temp, [1]Hum, [2]Wind, [3]Press

  bool loaded_forcast;
  bool loaded_historical[4];
};

static City cities[] = {

    {
        "Karlskrona",
        "56.16156",
        "15.58661",
        "65090",
    },
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

// LVGL widgets on settings screen with references for:
static lv_obj_t *city_dropdown;
static lv_obj_t *param_dropdown;
static lv_obj_t *btn_save_default;
static lv_obj_t *btn_reset_defaults;
static lv_obj_t *settings_status_label;

// Function: Tile Color change
static void apply_tile_colors(lv_obj_t *tile)
{
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(tile, lv_color_white(), 0);
}

// Function: Update Wi-Fi status on Tile #3 (non-blocking, smooth)
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
    lv_obj_center(t4_label); // only center once
    wifi_was_connected = true;
  }
  else if (current_status != WL_CONNECTED && wifi_was_connected)
  {
    lv_label_set_text(t4_label, "Wi-Fi: Connecting...");
    lv_obj_center(t4_label);
    wifi_was_connected = false;
  }
  // If status hasn't changed, do nothing → no redraw → smooth
}

/**
 * @brief Extracts day and month from ISO timestamp
 * Example: "2025-11-27T12:00:00Z" -> "Nov 27"
 */
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
    {
      return false;
    }
  }
  return true;
}

/**
 * @brief Updates all UI labels with data from global variables.
 */
void update_ui()
{
  char buffer[2048]; // Larger buffer for symbols and formatting
  char dateStr[16];

  // --- Update Tile 1: 7-Day Forecast with Symbols ---
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
  lv_obj_center(t1_label); // Re-center

  // --- Update Tile 2: Historical Weather ---
  // Show the total days fetched and the most recent (yesterday)
  if (cities[selectedCityIndex].history[selectedParamIndex].count > 0)
  {
    int mostRecentIndex = cities[selectedCityIndex].history[selectedParamIndex].count - 1;
    snprintf(buffer, sizeof(buffer),
             "Historical Data:\n"
             "Fetched %d Hours.\n"
             "City: %s\n"
             "%s: (%.1f):\n",
             cities[selectedCityIndex].history[selectedParamIndex].count,
             cities[selectedCityIndex].name,
             parameters[selectedParamIndex].label,
             cities[selectedCityIndex].history[selectedParamIndex].values[mostRecentIndex]);
  }
  else
  {
    snprintf(buffer, sizeof(buffer), "Historical Data:\nNo data loaded.");
  }

  lv_label_set_text(settings_status_label, "");
  lv_label_set_text(t2_label, buffer);
  lv_obj_center(t2_label); // Re-center
}

// Settings callbacks
void settings_value_changed(lv_event_t *e)
{
  // when dropdown value changes, update selected index
  lv_obj_t *obj = lv_event_get_target(e);
  if (obj == city_dropdown)
  {
    selectedCityIndex = lv_dropdown_get_selected(obj);
    lv_label_set_text(settings_status_label,
                      "City selected - updating UI...");
  }
  else if (obj == param_dropdown)
  {
    selectedParamIndex = lv_dropdown_get_selected(obj);
    lv_label_set_text(settings_status_label,
                      "Parameters selected - updating UI...");
  }
  // UI gets updated automatically in the loop so we don't need to do it here.
}

static void on_save_defaults(lv_event_t *e)
{
  LV_UNUSED(e); // reset to built-in defaults (index 0)
  preferences.begin("weather", false);
  preferences.putUInt("city_idx", (uint32_t)selectedCityIndex);
  preferences.putUInt("param_idx", (uint32_t)selectedParamIndex);
  preferences.end();
  lv_label_set_text(settings_status_label, "Defaults saved!");
  Serial.println("Defaults saved to Preferences.");
}

static void on_reset_deaults(lv_event_t *e)
{
  LV_UNUSED(e); // reset to built-in defaults (index 0)
  selectedCityIndex = 0;
  selectedParamIndex = 0;
  lv_dropdown_set_selected(city_dropdown, selectedCityIndex);
  lv_dropdown_set_selected(param_dropdown, selectedParamIndex);

  preferences.begin("weather", false);
  preferences.clear();
  preferences.end();

  lv_label_set_text(settings_status_label,
                    "Defaults reset to built-in defaults.");
  Serial.println("Preferences cleared and UI reset.");
}

static void get_saved_preferences()
{
  preferences.begin("weather", true);
  selectedCityIndex = preferences.getUInt("city_idx", 0);
  selectedParamIndex = preferences.getUInt("param_idx", 0);
  preferences.end();

  Serial.printf("Loaded Preferences: city_idx=%d, param_idx=%d\n",
                selectedCityIndex, selectedParamIndex);
}

// Function: Creates UI
static void create_ui()
{
  tileview = lv_tileview_create(lv_scr_act());
  lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL),
                  lv_disp_get_ver_res(NULL));
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

  // Add tiles
  t0 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR); // Boot screen tile
  t1 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR); // 7-day forecast
  t2 = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR); // Historical data
  t3 = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_HOR); // configure settings
  t4 = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_HOR); // Wi-Fi tile

  // Tile #0 - Boot Screen (Permanent)
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

  // Tile #2 - Historical weather data
  t2_label = lv_label_create(t2);
  lv_label_set_text(t2_label, "Historical data: Loading...");
  lv_obj_set_style_text_font(t2_label, &montserrat_se_28, 0);
  lv_obj_center(t2_label);
  apply_tile_colors(t2);

  // Tile #3 - drop-down settings
  lv_obj_t *t3_label = lv_label_create(t3);
  lv_label_set_text(t3_label, "Settings");
  lv_obj_set_style_text_font(t3_label, &montserrat_se_28, 0);
  lv_obj_align(t3_label, LV_ALIGN_TOP_MID, 0, 6);
  apply_tile_colors(t3);

  // City dropdown, build options string
  String cityOptions;
  for (int i = 0; i < CITY_COUNT; ++i)
  {
    cityOptions += cities[i].name;
    if (i < CITY_COUNT - 1)
    {
      cityOptions += "\n";
    }
  }
  city_dropdown = lv_dropdown_create(t3);
  lv_dropdown_set_options(city_dropdown, cityOptions.c_str());
  lv_obj_set_width(city_dropdown, 200);
  lv_obj_align(city_dropdown, LV_ALIGN_TOP_LEFT, 10, 50);

  // Set font for the "header" (the button you see when closed)
  lv_obj_set_style_text_font(city_dropdown, &montserrat_se_28, 0);

  // Set font for the "list" (the popup menu)
  lv_obj_t *list = lv_dropdown_get_list(city_dropdown);
  lv_obj_set_style_text_font(list, &montserrat_se_28, LV_PART_MAIN);

  lv_dropdown_set_selected(city_dropdown, selectedCityIndex);
  lv_obj_add_event_cb(city_dropdown, settings_value_changed, LV_EVENT_VALUE_CHANGED, NULL);

  // Parameter dropdown
  String paramOptions;
  for (int i = 0; i < PARAM_COUNT; ++i)
  {
    paramOptions += parameters[i].label;
    if (i < PARAM_COUNT - 1)
    {
      paramOptions += "\n";
    }
  }
  param_dropdown = lv_dropdown_create(t3);
  lv_dropdown_set_options(param_dropdown, paramOptions.c_str());
  lv_obj_set_width(param_dropdown, 200);
  lv_obj_align(param_dropdown, LV_ALIGN_TOP_LEFT, 10, 100);

  // Set font for the "header" (the button you see when closed)
  lv_obj_set_style_text_font(param_dropdown, &montserrat_se_28, 0);

  // Set font for the "list" (the popup menu)
  lv_obj_t *list2 = lv_dropdown_get_list(param_dropdown);
  lv_obj_set_style_text_font(list2, &montserrat_se_28, LV_PART_MAIN);

  lv_dropdown_set_selected(param_dropdown, selectedParamIndex);
  lv_obj_add_event_cb(param_dropdown, settings_value_changed, LV_EVENT_VALUE_CHANGED, NULL);

  // Save default button
  btn_save_default = lv_btn_create(t3);
  lv_obj_align(btn_save_default, LV_ALIGN_TOP_RIGHT, -10, 50);
  lv_obj_set_width(btn_save_default, 160);
  lv_obj_t *lbl_save = lv_label_create(btn_save_default);
  lv_label_set_text(lbl_save, "Save As Default");
  lv_obj_add_event_cb(btn_save_default, on_save_defaults, LV_EVENT_CLICKED, NULL);


  // Reset Defaults button
  btn_reset_defaults = lv_btn_create(t3);
  lv_obj_align(btn_save_default, LV_ALIGN_BOTTOM_RIGHT, -10, -50);
  lv_obj_set_width(btn_reset_defaults, 160);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset_defaults);
  lv_label_set_text(lbl_reset, "Reset Defaults");
  lv_obj_add_event_cb(btn_reset_defaults, on_reset_deaults, LV_EVENT_CLICKED, NULL);


  // Status label
  settings_status_label = lv_label_create(t3);
  lv_label_set_text(settings_status_label, "");
  lv_obj_align(settings_status_label, LV_ALIGN_TOP_MID, 0, 170);

  // Tile #4 - Wi-Fi status
  t4_label = lv_label_create(t4);
  lv_label_set_text(t4_label, "Wi-Fi: Connecting...");
  lv_obj_set_style_text_font(t4_label, &montserrat_se_28, 0);
  lv_obj_center(t4_label);
  apply_tile_colors(t4);

  // Explicitly set the screen to Tile 0 (Boot screen)
  // This is necessary to ensure that the boot screen is shown first and not
  // some other tile.
  lv_obj_set_tile(tileview, t0, LV_ANIM_OFF);
}

/**
 * @brief A REUSABLE helper function to fetch and parse JSON from any URL.
 * This function is "decoupled" - it does NOT update any UI.
 *
 * @param url The full URL to fetch from.
 * @param doc A reference to the JsonDocument that will be filled.
 * @return true if the fetch AND parse were successful, false otherwise.
 */
static bool fetchJsonFromServer(const String &url, JsonDocument &doc)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[HTTP] Error: Wi-Fi not connected.");
    return false;
  }

  Serial.printf("[HTTP] Fetching URL: %s\n", url.c_str());
  HTTPClient http;

  // 1. FORCE HTTP 1.0 (Fixes Chunked Encoding issues / InvalidInput)
  http.useHTTP10(true);

  // 2. TIMEOUT (Give the http response a bit more time)
  http.setTimeout(10000);

  if (http.begin(url))
  {
    // 3. DISABLE COMPRESSION (Force plain text)
    http.addHeader("Accept-Encoding", "identity");
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {

      Stream &stream = http.getStream();
      DeserializationError error = deserializeJson(doc, stream);
      http.end(); // End http *after* parsing stream

      if (error)
      {
        Serial.print("[JSON] deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
      }

      Serial.println("[JSON] Parse successful.");
      return true; // Success!
    }
    else
    {
      Serial.printf("[HTTP] GET failed, error: %s\n",
                    http.errorToString(httpCode).c_str());
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

// This allows making use of the 8MB of extra ps ram.
// (It happened in the previous `fetchAllWeatherData` function that when allocating 100 kilobyte to handle json it ran out of normal ram.)
struct SpiRamAllocator
{
  void *allocate(size_t size)
  {
    return ps_malloc(size);
  }
  void deallocate(void *pointer)
  {
    free(pointer);
  }
  void *reallocate(void *ptr, size_t new_size)
  {
    return ps_realloc(ptr, new_size);
  }
};
SpiRamAllocator myPsramAllocator;

// Define a custom type that uses this allocator
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

/**
 *@brief Fetches the forcast data for the city with index c in the cities array.
 *@returns true if successful, false otherwise.
 */
bool fetchForcast(int c)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Aborting fetching API because no internet connection");

    return false;
  }
  // 200KB
  SpiRamJsonDocument doc(200000);

  // 1. Fetch Forecast for City[c]
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
      // Guarantees that we skip todays 12:00pm and take the next 7.
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
          hourly.weatherCondition =
              WeatherCondition(hour["data"]["symbol_code"].as<int>());
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

/**
 *@brief Fetches the historical data for the city with index c and weather parameter with index p.
 *@returns true if successful, false otherwise.
 */
bool fetchHistorical(int c, int p)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Aborting fetching API because no internet connection");

    return false;
  }

  // 200KB
  SpiRamJsonDocument doc(200000);
  String histUrl = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/";
  histUrl += parameters[p].apiCode;
  histUrl += "/station/";
  histUrl += cities[c].stationID;
  histUrl += "/period/latest-months/data.json";

  Serial.printf("Fetching History (%s) for %s...\n", parameters[p].label, cities[c].name);

  if (fetchJsonFromServer(histUrl, doc))
  {
    JsonArray days = doc["value"].as<JsonArray>();
    int idx = 0;
    // Access the specific history slot: cities[c].history[p]
    for (JsonVariant day : days)
    {
      if (idx >= HistoricalSeries::MAX_HOURS)
        break;
      cities[c].history[p].values[idx] = day["value"].as<float>();
      idx++;
    }
    cities[c].history[p].count = idx;
    cities[c].history[p].isLoaded = true;
    cities[c].loaded_historical[p] = true;
    return true;
  }
  return false;
}

// Must-have setup function
void setup()
{

  // Before anything we need to initialize the `cities` object fully.
  for (int i = 0; i < CITY_COUNT; ++i) {
    for (int j = 0; j < PARAM_COUNT; ++j) {
      // Allocate the memory on the Heap at runtime
      cities[i].history[j].values = (float *)ps_malloc(HistoricalSeries::MAX_HOURS * sizeof(float));
      // Always check for successful allocation
      if (cities[i].history[j].values == nullptr) {
        Serial.println("FATAL: Failed to allocate historical data memory!");
        while (true); // Halt execution
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

  // Load saved default preferences otherwise default to city index 0, Karlskrona and param index 0, air temp.
  get_saved_preferences();

  // Create main UI (boot screen is now permanent as tile 0)
  create_ui();

  // Connect Wi-Fi once (non-blocking)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);

  update_wifi_status();
}

// Loop continuously
void loop()
{
  lv_timer_handler(); // handle LVGL updates frequently

  // Update Wi-Fi status every 500ms without blocking
  if (millis() - last_wifi_update > 500)
  {
    update_wifi_status();
    last_wifi_update = millis();
  }

  // In every loop we check if we need to fetch data for what is currently is trying to be displayed.
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