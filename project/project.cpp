#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>
#include <Preferences.h>

// Wi-Fi credentials
static const char *WIFI_SSID = "iPhone (2)";
static const char *WIFI_PASSWORD = "6z3sveylgk3a";

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
static lv_obj_t *t3_label; // settings
static lv_obj_t *t4_label; // wifi


static bool t1_dark = false; // start tile #1 (forecast) in light mode

// track Wi-Fi connection, whenever you need to access the internet you need to check that this is true.
static bool wifi_was_connected = false;
static unsigned long last_wifi_update = 0; // track last Wi-Fi update time

static unsigned long last_weather_update = 0;
static const long WEATHER_UPDATE_INTERVAL = 5 * 60 * 1000; // Update weather every 5 minutes (in milliseconds)

Preferences preferences; // for settings

/**
 * @brief Defines the 27 SMHI weather symbol codes that can be gotten for each hour by the Forcast API in particular. For the hourly and historical weather data you can't get these exacts ones from the API it seems. The project description says the following:
 *
 * "As a user, I want to see the weather forecast for the next 7 days for Karlskrona on the second screen in terms of temperature and weather conditions with symbols (e.g., clear sky, rain, snow, thunder) per day at 12:00."
 *
 * So we have to figure out how these codes get translated into actual images of "clear sky" or "rain" etc.
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
    WeatherCondition(int code)
    {
        value = static_cast<Value>(code);
    }
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
 * @brief A blueprint for holding one hour of forecast data. It holds the temperature, date, and a weather symbol code for that hour gotten by the SMHI API.
 */
struct ForcastHourlyWeather
{
    float temperature;

    //  example "2025-11-06T14:00:00Z"
    char time[FORCAST_TIMESTAMP_SIZE + 1];
    WeatherCondition weatherCondition;
};

/**
 * @brief A blueprint for holding one day of historical average temperature data (the average temperature of that day).
 */
struct DailyAverageTemp
{
    float averageTemperature;
    // example "2025-06-29"
    char time[11];
};

// Holds the weather forcast for the next 7 days. The object holds an array of 7 elements. Each element is a `ForcastHourlyWeather` object with the forcast for the temperature at 12:00pm daytime that day.
struct
{
    ForcastHourlyWeather hours[7];
} sevenDayForecast;

// Holds the historical weather data for a max of 150 days from the time of calling the API (Item at index 0 is the oldest day maybe day 130 from now while the last element in the array is yesterday). It's an array of 150 'DailyAverageTemp' objects.
struct
{
    // The API seemed to return the last about 130 days, so 150 to be safe.
    // Use `used_length` for the amount of actual filled slots.
    DailyAverageTemp days[150];

    // The number of elements in the array that are filled. (not all 150)
    int used_length = 0;

} lastMonthsAverageTemps;

// Cities and Parameter
struct City {
    const char *name;
    double lat;
    double lon;
    int stationID;
};

static City cities[]= {
    {"Stockholm",        59.3293, 18.0686, 98210},
    {"Göteborg",         57.7089, 11.9746, 71420},
    {"Malmö",            55.6050, 13.0038, 53400},
    {"Uppsala",          59.8586, 17.6389, 97530},
    {"Västerås",         59.6099, 16.5448, 94800},
    {"Örebro",           59.2741, 15.2066, 94820},
    {"Linköping",        58.4108, 15.6214, 97500},
    {"Helsingborg",      56.0465, 12.6945, 62480},
    {"Jönköping",        57.7815, 14.1562, 77160},
    {"Norrköping",       58.5877, 16.1924, 97510},
    {"Lund",             55.7047, 13.1910, 53400},
    {"Umeå",             63.8258, 20.2630, 140480},
    {"Gävle",            60.6749, 17.1413, 107290},
    {"Borås",            57.7210, 12.9398, 71470},
    {"Eskilstuna",       59.3666, 16.5077, 96330},
    {"Södertälje",       59.1955, 17.6253, 97590},
    {"Karlstad",         59.4022, 13.5115, 94870},
    {"Täby",             59.4439, 18.0687, 98210},
    {"Växjö",            56.8790, 14.8059, 65090},
    {"Halmstad",         56.6743, 12.8570, 62430},
    {"Sundsvall",        62.3908, 17.3069, 128870},
    {"Luleå",            65.5848, 22.1567, 159880},
    {"Trollhättan",      58.2860, 12.2913, 72310},
    {"Östersund",        63.1792, 14.6357, 144740},
    {"Borlänge",         60.4858, 15.4371, 106290},
    {"Upplands Väsby",   59.5184, 17.9110, 98210},
    {"Tumba",            59.1981, 17.8337, 97590},
    {"Kristianstad",     56.0313, 14.1524, 61910},
    {"Kalmar",           56.6634, 16.3568, 85250},
    {"Skövde",           58.3903, 13.8460, 81530},
    {"Karlskrona",       56.1612, 15.5869, 65090},
    {"Skellefteå",       64.7500, 20.9500, 159410},
    {"Uddevalla",        58.3500, 11.9333, 72350},
    {"Falun",            60.6074, 15.6310, 106210},
    {"Trelleborg",       55.3751, 13.1569, 53400},
    {"Nyköping",         58.7557, 17.0079, 97100},
    {"Karlskoga",        59.3266, 14.5230, 94840},
    {"Varberg",          57.1057, 12.2508, 62410},
    {"Örnsköldsvik",     63.2909, 18.7153, 128370},
    {"Vänersborg",       58.3807, 12.3235, 72310},
    {"Motala",           58.5371, 15.0365, 97540},
    {"Lidingö",          59.3668, 18.1334, 98210},
    {"Piteå",            65.3167, 21.4783, 159890},
    {"Mölndal",          57.6554, 12.0138, 71420},
    {"Alingsås",         57.9303, 12.5336, 71470},
    {"Sandviken",        60.6200, 16.7750, 107290},
    {"Boo (Nacka)",      59.3660, 18.2650, 98210},
    {"Åkersberga",       59.4794, 18.2997, 98210},
    {"Visby",            57.6348, 18.2948, 94300},
};
static const int CITY_COUNT = sizeof(cities)/sizeof(cities[0]);

struct Parameter {
    const char *label;
    const char *smhiName; // parameter name used in historical API
    const char *paramCode; // for forecast paramters array names, i.e "t" for temperature
};
static Parameter parameters[] = {
    {"Temperture", "air_temperature", "t"},
    {"Humiditiy", "humidity", "h"},
    {"Wind speed", "wind_speed", "ws"}
};
static const int PARAM_COUNT = sizeof(parameters)/sizeof(parameters[0]);

// current selcetions (indices)
static int selectedCityIndex = 0;
static int selectedParamIndex = 0;

//LVGL widgets on settings screen with references for:
static lv_obj_t *city_dropdown;
static lv_obj_t *param_dropdown;
static lv_obj_t *btn_save_default;
static lv_obj_t *btn_reset_defaults;
static lv_obj_t *settings_status_label;


// Function: Tile Color change
static void apply_tile_colors(lv_obj_t *tile, lv_obj_t *label, bool dark)
{
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tile, dark ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_text_color(label, dark ? lv_color_white() : lv_color_black(), 0);
}

// Tile #1 (forecast) click toggle
static void on_tile1_clicked(lv_event_t *e)
{
    LV_UNUSED(e);
    t1_dark = !t1_dark;
    apply_tile_colors(t1, t1_label, t1_dark);
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
                 WiFi.SSID().c_str(),
                 ip[0], ip[1], ip[2], ip[3]);
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

// Function: Creates UI
static void create_ui()
{
    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
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
    lv_obj_set_style_text_font(t0_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(t0_label, lv_color_white(), 0);
    lv_obj_center(t0_label);

    // Tile #1 - 7-Day Forecast
    t1_label = lv_label_create(t1);
    lv_label_set_text(t1_label, "Forecast data: Loading...");
    lv_obj_set_style_text_font(t1_label, &lv_font_montserrat_20, 0);
    lv_obj_center(t1_label);
    apply_tile_colors(t1, t1_label, false);
    lv_obj_add_flag(t1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t1, on_tile1_clicked, LV_EVENT_CLICKED, NULL);

    // Tile #2 - Historical weather data
    t2_label = lv_label_create(t2);
    lv_label_set_text(t2_label, "Historical data: Loading...");
    lv_obj_set_style_text_font(t2_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t2_label);
    apply_tile_colors(t2, t2_label, false);

    // Tile #3 - drop-down settings
    t3_label = lv_label_create(t3);
    lv_label_set_text(t3_label, "Settings");
    lv_obj_set_style_text_font(t3_label, &lv_font_montserrat_28, 0);
    lv_obj_align(t3_label, LV_ALIGN_TOP_MID, 0, 6);

    // City dropdown, build options string
    String cityOptions;
    for (int i = 0; i < CITY_COUNT; ++i) {
        cityOptions += cities[i].name;
        if (i < CITY_COUNT - 1) {
            cityOptions += "\n";
        }
    }
    city_dropdown = lv_dropdown_create(t3);
    lv_dropdown_set_options(city_dropdown, cityOptions.c_str());
    lv_obj_set_width(city_dropdown, 200);
    lv_obj_align(city_dropdown, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_dropdown_set_selected(city_dropdown, selectedCityIndex);

    // Parameter dropdown
    String paramOptions;
    for (int i = 0; i < PARAM_COUNT; ++i) {
        paramOptions += parameters[i].label;
        if (i < PARAM_COUNT - 1) {
            paramOptions += "\n";
        }
    }
    param_dropdown = lv_dropdown_create(t3);
    lv_dropdown_set_options_static(param_dropdown, paramOptions.c_str());
    lv_obj_set_width(param_dropdown, 200);
    lv_obj_align(param_dropdown, LV_ALIGN_TOP_LEFT, 10, 100);
    lv_dropdown_set_selected(param_dropdown, selectedParamIndex);

    // Save default button
    btn_save_default = lv_btn_create(t3);
    lv_obj_align(btn_save_default, LV_ALIGN_TOP_RIGHT, -10, 50);
    lv_obj_set_width(btn_save_default, 160);
    lv_obj_t *lbl_save = lv_label_create(btn_save_default);
    lv_label_set_text(lbl_save, "Save As Default");

    // Reset Defaults button
    btn_reset_defaults = lv_btn_create(t3);

    // Status label
    settings_status_label = lv_label_create(t3);
    lv_label_set_text(settings_status_label, "");
    lv_obj_align(settings_status_label, LV_ALIGN_TOP_MID, 0, 170);

    // Tile #4 - Wi-Fi status
    t4_label = lv_label_create(t4);
    lv_label_set_text(t4_label, "Wi-Fi: Connecting...");
    lv_obj_set_style_text_font(t4_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t4_label);
    apply_tile_colors(t4, t4_label, false);
}

/**
 * @brief Extracts day and month from ISO timestamp
 * Example: "2025-11-27T12:00:00Z" -> "Nov 27"
 */
void formatDate(const char* timestamp, char* output, size_t outputSize)
{
    if (strlen(timestamp) < 10) {
        snprintf(output, outputSize, "???");
        return;
    }
    
    int year, month, day;
    sscanf(timestamp, "%d-%d-%d", &year, &month, &day);
    
    const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    if (month >= 1 && month <= 12) {
        snprintf(output, outputSize, "%s %d", monthNames[month - 1], day);
    } else {
        snprintf(output, outputSize, "???");
    }
}

/**
 * @brief Updates all UI labels with data from global variables.
 */
void update_ui()
{
    char buffer[2048]; // Larger buffer for symbols and formatting
    char dateStr[16];

    // --- Update Tile 1: 7-Day Forecast with Symbols ---
    snprintf(buffer, sizeof(buffer), "7-Day Forecast (12:00)\n\n");
    
    for (int i = 0; i < 7; i++)
    {
        formatDate(sevenDayForecast.hours[i].time, dateStr, sizeof(dateStr));
        
        char line[128];
        snprintf(line, sizeof(line), "%s %s %.1f°C %s\n",
                 getWeatherSymbol(sevenDayForecast.hours[i].weatherCondition),
                 dateStr,
                 sevenDayForecast.hours[i].temperature,
                 getWeatherString(sevenDayForecast.hours[i].weatherCondition));
        
        strcat(buffer, line);
    }
    
    lv_label_set_text(t1_label, buffer);
    lv_obj_center(t1_label); // Re-center

    // --- Update Tile 2: Historical Weather ---
    // Show the total days fetched and the most recent (yesterday)
    if (lastMonthsAverageTemps.used_length > 0)
    {
        int mostRecentIndex = lastMonthsAverageTemps.used_length - 1;
        snprintf(buffer, sizeof(buffer),
                 "Historical Data:\n"
                 "Fetched %d days.\n"
                 "Yesterday (%s):\n"
                 "Avg Temp: %.1f C",
                 lastMonthsAverageTemps.used_length,
                 lastMonthsAverageTemps.days[mostRecentIndex].time,
                 lastMonthsAverageTemps.days[mostRecentIndex].averageTemperature);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Historical Data:\nNo data loaded.");
    }
    lv_label_set_text(t2_label, buffer);
    lv_obj_center(t2_label); // Re-center
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

    if (http.begin(url))
    {
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            DeserializationError error = deserializeJson(doc, payload);
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

// Settings callbacks
static void settings_value_changed(lv_event_t *e) {
    // when dropdown value changes, update selected index
    lv_obj_t *obj = lv_event_get_target(e);
    if (obj == city_dropdown) {
        selectedCityIndex = lv_dropdown_get_selected(obj);
        lv_label_set_text(settings_status_label, "City selected - will apply on next refresh");
        last_weather_update = 0; // trigger immediate refresh
    } else if (obj == param_dropdown) {
        selectedParamIndex = lv_dropdown_get_selected(obj);
        lv_label_set_text(settings_status_label, "Parameters selected - will apply on next refresh");
        last_weather_update = 0;
    }
}
static void on_save_defaults(lv_event_t *e) {
    LV_UNUSED(e); // reset to built-in defaults (index 0)
    preferences.begin("weather", false);
    preferences.putUInt("city_idx", (uint32_t)selectedCityIndex);
    preferences.putUInt("param_idx", (uint32_t)selectedParamIndex);
    preferences.end();
    lv_label_set_text(settings_status_label, "Defaults saved!");
    Serial.println("Defaults saved to Preferences.");
    
}

static void on_reset_deaults(lv_event_t *e) {
    LV_UNUSED(e); // reset to built-in defaults (index 0)
    selectedCityIndex = 0;
    selectedParamIndex = 0;
    lv_dropdown_set_selected(city_dropdown, selectedCityIndex);
    lv_dropdown_set_selected(param_dropdown, selectedParamIndex);
    
    preferences.begin("weather", false);
    preferences.clear();
    preferences.end();

    lv_label_set_text(settings_status_label, "Defaults reset to built-in defaults.");
    Serial.println("Preferences cleared and UI reset.");
    last_weather_update = 0;
}

// Must-have setup function
void setup()
{
    Serial.begin(115200);
    delay(200);

    if (!amoled.begin())
    {
        Serial.println("Failed to init LilyGO AMOLED.");
        while (true)
            delay(1000);
    }

    beginLvglHelper(amoled);

    // Create main UI (boot screen is now permanent as tile 0)
    create_ui();

    // Connect Wi-Fi once (non-blocking)
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
    last_weather_update = 0; // Set to 0 to trigger an immediate update on first connection
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
    if (wifi_was_connected && (millis() - last_weather_update > WEATHER_UPDATE_INTERVAL || last_weather_update == 0))
    {
        // Update the seven day forcast global object.
        String sevenDayForcastUrl = "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/15.589/lat/56.15/data.json";
        DynamicJsonDocument forecastDoc(40000); // 32k, on the heap
        if (fetchJsonFromServer(sevenDayForcastUrl, forecastDoc))
        {

            JsonArray hours = forecastDoc["timeSeries"].as<JsonArray>();
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
                        ForcastHourlyWeather &hourly = sevenDayForecast.hours[next_day];

                        hourly.temperature = hour["data"]["air_temperature"].as<float>();
                        hourly.weatherCondition = WeatherCondition(hour["data"]["symbol_code"].as<int>());
                        strncpy(hourly.time, time, 20);
                        hourly.time[20] = '\0';
                        next_day++;
                    }
                }
            }
        }

        // Update historical last months global object.
        String historicalLastMonths_url = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/2/station/65090/period/latest-months/data.json";
        DynamicJsonDocument historicalDoc(60000); // innan 16536
        if (fetchJsonFromServer(historicalLastMonths_url, historicalDoc))
        {
            JsonArray days = historicalDoc["value"].as<JsonArray>();
            int i = 0;
            for (JsonVariant day : days)
            {
                const char *time = day["ref"].as<const char *>();
                if (time != nullptr)
                {
                    DailyAverageTemp &daily = lastMonthsAverageTemps.days[i];
                    daily.averageTemperature = day["value"].as<float>();
                    strncpy(daily.time, time, 10);
                    daily.time[10] = '\0';
                }
                i++;
            }
            lastMonthsAverageTemps.used_length = i;
        }

        update_ui();

        last_weather_update = millis();
    }
}