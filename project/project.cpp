#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>

// Wi-Fi credentials
static const char *WIFI_SSID = "AN";
static const char *WIFI_PASSWORD = "3feC=Mic@iKsi&Da";

LilyGo_Class amoled;

static lv_obj_t *tileview;
static lv_obj_t *t1;
static lv_obj_t *t2;
static lv_obj_t *t3;
static lv_obj_t *t4;

static lv_obj_t *t1_label;
static lv_obj_t *t2_label;
static lv_obj_t *t3_label;
static lv_obj_t *t4_label;

static bool t2_dark = false; // start tile #2 in light mode

// track Wi-Fi connection, whenever you need to access the internet you need to check that this is true.
static bool wifi_was_connected = false;
static unsigned long last_wifi_update = 0; // track last Wi-Fi update time

static unsigned long last_weather_update = 0;
static const long WEATHER_UPDATE_INTERVAL = 5 * 60 * 1000; // Update weather every 5 minutes (in milliseconds)

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
 * @brief Converts a WeatherCondition enum into a human-readable string.
 */
const char *
getWeatherString(WeatherCondition symbol)
{
    switch (symbol.value)
    {
    case WeatherCondition::ClearSky:
        return "ClearSky";
    case WeatherCondition::NearlyClearSky:
        return "NearlyClearSky";
    case WeatherCondition::VariableCloudiness:
        return "VariableCloudiness";
    case WeatherCondition::HalfClearSky:
        return "HalfClearSky";
    case WeatherCondition::CloudySky:
        return "CloudySky";
    case WeatherCondition::Overcast:
        return "Overcast";
    case WeatherCondition::Fog:
        return "Fog";
    case WeatherCondition::LightRainShowers:
        return "LightRainShowers";
    case WeatherCondition::ModerateRainShowers:
        return "ModerateRainShowers";
    case WeatherCondition::HeavyRainShowers:
        return "HeavyRainShowers";
    case WeatherCondition::Thunderstorm:
        return "Thunderstorm";
    case WeatherCondition::LightSleetShowers:
        return "LightSleetShowers";
    case WeatherCondition::ModerateSleetShowers:
        return "ModerateSleetShowers";
    case WeatherCondition::HeavySleetShowers:
        return "HeavySleetShowers";
    case WeatherCondition::LightSnowShowers:
        return "LightSnowShowers";
    case WeatherCondition::ModerateSnowShowers:
        return "ModerateSnowShowers";
    case WeatherCondition::HeavySnowShowers:
        return "HeavySnowShowers";
    case WeatherCondition::LightRain:
        return "LightRain";
    case WeatherCondition::ModerateRain:
        return "ModerateRain";
    case WeatherCondition::HeavyRain:
        return "HeavyRain";
    case WeatherCondition::Thunder:
        return "Thunder";
    case WeatherCondition::LightSleet:
        return "LightSleet";
    case WeatherCondition::ModerateSleet:
        return "ModerateSleet";
    case WeatherCondition::HeavySleet:
        return "HeavySleet";
    case WeatherCondition::LightSnowfall:
        return "LightSnowfall";
    case WeatherCondition::ModerateSnowfall:
        return "ModerateSnowfall";
    case WeatherCondition::HeavySnowfall:
        return "HeavySnowfall";
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

// The temperature of the most recent hour. (In karlskrona)
float currentHourlyTemp = -99.9;

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

// Function: Tile #2 Color change
static void apply_tile_colors(lv_obj_t *tile, lv_obj_t *label, bool dark)
{
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tile, dark ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_text_color(label, dark ? lv_color_white() : lv_color_black(), 0);
}

// Tile #2 click toggle
static void on_tile2_clicked(lv_event_t *e)
{
    LV_UNUSED(e);
    t2_dark = !t2_dark;
    apply_tile_colors(t2, t2_label, t2_dark);
}

// Function: Polished 3-second boot screen
static void show_boot_screen()
{
    lv_obj_t *scr = lv_scr_act();

    // Black background
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Label: Name, Group, Firmware
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "Group 8\nFirmware v1.2.0");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    // Horizontal progress line
    lv_obj_t *bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 220, 8);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_white(), 0);

    // Animate line over exactly 3 seconds
    const unsigned long duration_ms = 3000;
    unsigned long startTime = millis();

    while (millis() - startTime < duration_ms)
    {
        unsigned long elapsed = millis() - startTime;
        int lineValue = (elapsed * 100) / duration_ms;
        if (lineValue > 100)
            lineValue = 100;

        lv_bar_set_value(bar, lineValue, LV_ANIM_OFF);
        lv_timer_handler();
        delay(5);
    }

    lv_obj_clean(scr); // clear before main UI
}

// Function: Update Wi-Fi status on Tile #4 (non-blocking, smooth)
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

    // Add three horizontal tiles
    t1 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR);
    t2 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);
    t3 = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR);
    t4 = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_HOR); // Wi-Fi tile

    // Tile #1
    // IDK, for now just the current temperature.
    t1_label = lv_label_create(t1);
    lv_label_set_text(t1_label, "Current temperature: Nope...");
    lv_obj_set_style_text_font(t1_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t1_label);
    apply_tile_colors(t1, t1_label, false);

    // Tile #2
    // Forcast weather data
    t2_label = lv_label_create(t2);
    lv_label_set_text(t2_label, "Forcast data: Not yet apparently...");
    lv_obj_set_style_text_font(t2_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t2_label);
    apply_tile_colors(t2, t2_label, false);
    lv_obj_add_flag(t2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t2, on_tile2_clicked, LV_EVENT_CLICKED, NULL);

    // Tile #3
    // Historical weather data
    t3_label = lv_label_create(t3);
    lv_label_set_text(t3_label, "Historical data: Not yet huh...");
    lv_obj_set_style_text_font(t3_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t3_label);
    apply_tile_colors(t3, t3_label, false);

    // Tile #4 (Wi-Fi status)
    t4_label = lv_label_create(t4);
    lv_label_set_text(t4_label, "Wi-Fi: Connecting...");
    lv_obj_set_style_text_font(t4_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t4_label);
    apply_tile_colors(t4, t4_label, false);
}

/**
 * @brief Updates all UI labels with data from global variables.
 */
void update_ui()
{
    char buffer[1024]; // A buffer for building the label strings

    // --- Update Tile 1: Current Weather ---
    snprintf(buffer, sizeof(buffer), "Current Temp:\n%.1f C",
             currentHourlyTemp);
    lv_label_set_text(t1_label, buffer);
    lv_obj_center(t1_label); // Re-center after text change

    // --- Update Tile 2: 7-Day Forecast ---
    snprintf(buffer, sizeof(buffer),
             "7-Day Forecast (at 12:00):\n"
             "%s: %.2f C, %s\n"
             "%s: %.2f C, %s\n"
             "%s: %.2f C, %s\n"
             "%s: %.2f C, %s\n"
             "%s: %.2f C, %s\n"
             "%s: %.2f C, %s\n"
             "%s: %.2f C, %s\n",
             sevenDayForecast.hours[0].time,
             sevenDayForecast.hours[0].temperature,
             getWeatherString(sevenDayForecast.hours[0].weatherCondition),
             sevenDayForecast.hours[1].time,
             sevenDayForecast.hours[1].temperature,
             getWeatherString(sevenDayForecast.hours[1].weatherCondition),
             sevenDayForecast.hours[2].time,
             sevenDayForecast.hours[2].temperature,
             getWeatherString(sevenDayForecast.hours[2].weatherCondition),
             sevenDayForecast.hours[3].time,
             sevenDayForecast.hours[3].temperature,
             getWeatherString(sevenDayForecast.hours[3].weatherCondition),
             sevenDayForecast.hours[4].time,
             sevenDayForecast.hours[4].temperature,
             getWeatherString(sevenDayForecast.hours[4].weatherCondition),
             sevenDayForecast.hours[5].time,
             sevenDayForecast.hours[5].temperature,
             getWeatherString(sevenDayForecast.hours[5].weatherCondition),
             sevenDayForecast.hours[6].time,
             sevenDayForecast.hours[6].temperature,
             getWeatherString(sevenDayForecast.hours[6].weatherCondition));
    lv_label_set_text(t2_label, buffer);
    lv_obj_center(t2_label); // Re-center

    // --- Update Tile 3: Historical Weather ---
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
    lv_label_set_text(t3_label, buffer);
    lv_obj_center(t3_label); // Re-center
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

    // Show boot screen for 3 seconds
    show_boot_screen();

    // Create main UI
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

        // Update the
        String hourlyUrl = "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/1/station/65090/period/latest-hour/data.json";
        StaticJsonDocument<2048> hourlyDoc; // Small, on the stack
        if (fetchJsonFromServer(hourlyUrl, hourlyDoc))
        {
            currentHourlyTemp = hourlyDoc["value"][0]["value"].as<float>();
        }

        // Update the seven day forcast global object.
        String sevenDayForcastUrl = "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/15.589/lat/56.15/data.json";
        DynamicJsonDocument forecastDoc(40000); // 32k, on the heap
        if (fetchJsonFromServer(sevenDayForcastUrl, forecastDoc))
        {

            JsonArray hours = forecastDoc["timeSeries"].as<JsonArray>();
            int skips = 12;
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
        DynamicJsonDocument historicalDoc(16536);
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