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
static const char *WIFI_SSID = "ssid";
static const char *WIFI_PASSWORD = "password";

LilyGo_Class amoled;

static lv_obj_t *tileview;
static lv_obj_t *t0; // Boot screen tile
static lv_obj_t *t1;
static lv_obj_t *t2;
static lv_obj_t *t3;
static lv_obj_t *t4; // Settings tile

static lv_obj_t *t0_label; // Boot screen label
static lv_obj_t *t1_label;
static lv_obj_t *t2_label;
static lv_obj_t *t3_label;
static lv_obj_t *t4_label; // Settings label

static bool t1_dark = false; // start tile #1 (forecast) in light mode

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
 * @brief Creates a simple weather icon using LVGL canvas
 * Returns an LVGL image object with the drawn icon
 */
lv_obj_t *createWeatherIcon(lv_obj_t *parent, WeatherCondition symbol)
{
    // Create a canvas to draw on (32x32 pixels)
    lv_obj_t *canvas = lv_canvas_create(parent);
    
    // Buffer for canvas (32x32 pixels, RGB565 = 2 bytes per pixel)
    static lv_color_t cbuf[32 * 32];
    lv_canvas_set_buffer(canvas, cbuf, 32, 32, LV_IMG_CF_TRUE_COLOR);
    
    // Fill with transparent background
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_0);
    
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.width = 2;
    line_dsc.opa = LV_OPA_COVER;
    
    switch (symbol.value)
    {
    case WeatherCondition::ClearSky:
    case WeatherCondition::NearlyClearSky:
    {
        // Draw sun (circle + rays)
        rect_dsc.bg_color = lv_color_hex(0xFFD700); // Gold
        rect_dsc.radius = LV_RADIUS_CIRCLE;
        lv_canvas_draw_rect(canvas, 10, 10, 12, 12, &rect_dsc);
        
        // Sun rays
        line_dsc.color = lv_color_hex(0xFFD700);
        lv_point_t rays[] = {
            {16, 3}, {16, 7},   // Top
            {16, 25}, {16, 29}, // Bottom
            {3, 16}, {7, 16},   // Left
            {25, 16}, {29, 16}, // Right
        };
        lv_canvas_draw_line(canvas, &rays[0], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &rays[2], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &rays[4], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &rays[6], 2, &line_dsc);
        break;
    }
    case WeatherCondition::VariableCloudiness:
    case WeatherCondition::HalfClearSky:
    {
        // Small sun
        rect_dsc.bg_color = lv_color_hex(0xFFD700);
        rect_dsc.radius = LV_RADIUS_CIRCLE;
        lv_canvas_draw_rect(canvas, 4, 4, 10, 10, &rect_dsc);
        
        // Cloud
        rect_dsc.bg_color = lv_color_hex(0xCCCCCC);
        lv_canvas_draw_rect(canvas, 12, 18, 16, 10, &rect_dsc);
        break;
    }
    case WeatherCondition::CloudySky:
    case WeatherCondition::Overcast:
    {
        // Cloud
        rect_dsc.bg_color = lv_color_hex(0x999999);
        rect_dsc.radius = 8;
        lv_canvas_draw_rect(canvas, 6, 8, 20, 16, &rect_dsc);
        break;
    }
    case WeatherCondition::LightRain:
    case WeatherCondition::ModerateRain:
    case WeatherCondition::LightRainShowers:
    case WeatherCondition::ModerateRainShowers:
    {
        // Cloud
        rect_dsc.bg_color = lv_color_hex(0x999999);
        rect_dsc.radius = 8;
        lv_canvas_draw_rect(canvas, 6, 4, 20, 12, &rect_dsc);
        
        // Rain drops
        line_dsc.color = lv_color_hex(0x4169E1); // Blue
        lv_point_t drops[] = {
            {10, 18}, {10, 26},
            {16, 18}, {16, 26},
            {22, 18}, {22, 26},
        };
        lv_canvas_draw_line(canvas, &drops[0], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &drops[2], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &drops[4], 2, &line_dsc);
        break;
    }
    case WeatherCondition::HeavyRain:
    case WeatherCondition::HeavyRainShowers:
    {
        // Dark cloud
        rect_dsc.bg_color = lv_color_hex(0x666666);
        rect_dsc.radius = 8;
        lv_canvas_draw_rect(canvas, 6, 4, 20, 12, &rect_dsc);
        
        // Heavy rain
        line_dsc.color = lv_color_hex(0x0000FF);
        line_dsc.width = 3;
        lv_point_t drops[] = {
            {8, 18}, {8, 28},
            {16, 18}, {16, 28},
            {24, 18}, {24, 28},
        };
        lv_canvas_draw_line(canvas, &drops[0], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &drops[2], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &drops[4], 2, &line_dsc);
        break;
    }
    case WeatherCondition::Thunderstorm:
    case WeatherCondition::Thunder:
    {
        // Dark cloud
        rect_dsc.bg_color = lv_color_hex(0x333333);
        rect_dsc.radius = 8;
        lv_canvas_draw_rect(canvas, 6, 4, 20, 12, &rect_dsc);
        
        // Lightning bolt
        line_dsc.color = lv_color_hex(0xFFFF00); // Yellow
        line_dsc.width = 3;
        lv_point_t bolt[] = {
            {18, 18}, {16, 22},
            {16, 22}, {20, 22},
            {20, 22}, {14, 28},
        };
        lv_canvas_draw_line(canvas, &bolt[0], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &bolt[2], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &bolt[4], 2, &line_dsc);
        break;
    }
    case WeatherCondition::LightSnowfall:
    case WeatherCondition::ModerateSnowfall:
    case WeatherCondition::HeavySnowfall:
    case WeatherCondition::LightSnowShowers:
    case WeatherCondition::ModerateSnowShowers:
    case WeatherCondition::HeavySnowShowers:
    {
        // Cloud
        rect_dsc.bg_color = lv_color_hex(0xCCCCCC);
        rect_dsc.radius = 8;
        lv_canvas_draw_rect(canvas, 6, 4, 20, 12, &rect_dsc);
        
        // Snowflakes (asterisks)
        line_dsc.color = lv_color_hex(0xFFFFFF);
        line_dsc.width = 2;
        // Draw simple snowflake pattern
        lv_point_t snow1[] = {{10, 18}, {10, 24}};
        lv_point_t snow2[] = {{7, 21}, {13, 21}};
        lv_canvas_draw_line(canvas, snow1, 2, &line_dsc);
        lv_canvas_draw_line(canvas, snow2, 2, &line_dsc);
        
        lv_point_t snow3[] = {{22, 18}, {22, 24}};
        lv_point_t snow4[] = {{19, 21}, {25, 21}};
        lv_canvas_draw_line(canvas, snow3, 2, &line_dsc);
        lv_canvas_draw_line(canvas, snow4, 2, &line_dsc);
        break;
    }
    case WeatherCondition::Fog:
    {
        // Horizontal fog lines
        line_dsc.color = lv_color_hex(0xCCCCCC);
        line_dsc.width = 3;
        lv_point_t fog[] = {
            {6, 10}, {26, 10},
            {6, 16}, {26, 16},
            {6, 22}, {26, 22},
        };
        lv_canvas_draw_line(canvas, &fog[0], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &fog[2], 2, &line_dsc);
        lv_canvas_draw_line(canvas, &fog[4], 2, &line_dsc);
        break;
    }
    default:
    {
        // Question mark for unknown
        rect_dsc.bg_color = lv_color_hex(0xFF0000);
        rect_dsc.radius = 5;
        lv_canvas_draw_rect(canvas, 8, 8, 16, 16, &rect_dsc);
        break;
    }
    }
    
    return canvas;
}

/**
 * @brief Converts a WeatherCondition enum into a weather symbol.
 * Now returns text description only, icons are separate
 */
const char *getWeatherSymbol(WeatherCondition symbol)
{
    // Return empty string since we're using actual icons now
    return "";
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
        lv_label_set_text(t3_label, buf);
        lv_obj_center(t3_label); // only center once
        wifi_was_connected = true;
    }
    else if (current_status != WL_CONNECTED && wifi_was_connected)
    {
        lv_label_set_text(t3_label, "Wi-Fi: Connecting...");
        lv_obj_center(t3_label);
        wifi_was_connected = false;
    }
    // If status hasn't changed, do nothing → no redraw → smooth
}

// Function: Create settings screen content
static void create_settings_content()
{
    // Clear existing content
    lv_obj_clean(t4);
    
    // Create a container for better layout
    lv_obj_t *cont = lv_obj_create(t4);
    lv_obj_set_size(cont, lv_disp_get_hor_res(NULL) - 40, lv_disp_get_ver_res(NULL) - 40);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 20, 0);
    lv_obj_set_style_pad_row(cont, 15, 0);
    
    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    
    // City section
    lv_obj_t *city_heading = lv_label_create(cont);
    lv_label_set_text(city_heading, "City");
    lv_obj_set_style_text_font(city_heading, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(city_heading, lv_color_hex(0x2196F3), 0); // Blue color
    
    lv_obj_t *city_label = lv_label_create(cont);
    lv_label_set_text(city_label, "Current: Karlskrona");
    lv_obj_set_style_text_font(city_label, &lv_font_montserrat_18, 0);
    
    // Add spacing
    lv_obj_t *spacer1 = lv_obj_create(cont);
    lv_obj_set_size(spacer1, 1, 20);
    lv_obj_set_style_bg_opa(spacer1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer1, 0, 0);
    
    // Weather parameter section
    lv_obj_t *param_heading = lv_label_create(cont);
    lv_label_set_text(param_heading, "Weather Parameter");
    lv_obj_set_style_text_font(param_heading, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(param_heading, lv_color_hex(0x2196F3), 0); // Blue color
    
    lv_obj_t *param_label = lv_label_create(cont);
    lv_label_set_text(param_label, "Temperature: Celsius\nUpdate: Every 5 minutes");
    lv_obj_set_style_text_font(param_label, &lv_font_montserrat_18, 0);
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
    t3 = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_HOR); // Wi-Fi tile
    t4 = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_HOR); // Settings tile

    // Tile #0 - Boot Screen (Permanent)
    lv_obj_set_style_bg_color(t0, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(t0, LV_OPA_COVER, 0);
    
    t0_label = lv_label_create(t0);
    lv_label_set_text(t0_label, "Group 8\nFirmware v1.2.0");
    lv_obj_set_style_text_font(t0_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(t0_label, lv_color_white(), 0);
    lv_obj_center(t0_label);

    // Tile #1 - 7-Day Forecast
    // We'll recreate this when we have data to show icons properly
    lv_obj_set_style_bg_color(t1, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(t1, LV_OPA_COVER, 0);
    
    t1_label = lv_label_create(t1);
    lv_label_set_text(t1_label, "Loading forecast...");
    lv_obj_set_style_text_font(t1_label, &lv_font_montserrat_20, 0);
    lv_obj_center(t1_label);
    lv_obj_add_flag(t1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t1, on_tile1_clicked, LV_EVENT_CLICKED, NULL);

    // Tile #2 - Historical weather data
    t2_label = lv_label_create(t2);
    lv_label_set_text(t2_label, "Historical data: Loading...");
    lv_obj_set_style_text_font(t2_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t2_label);
    apply_tile_colors(t2, t2_label, false);

    // Tile #3 - Wi-Fi status
    t3_label = lv_label_create(t3);
    lv_label_set_text(t3_label, "Wi-Fi: Connecting...");
    lv_obj_set_style_text_font(t3_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t3_label);
    apply_tile_colors(t3, t3_label, false);
    
    // Tile #4 - Settings
    lv_obj_set_style_bg_color(t4, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(t4, LV_OPA_COVER, 0);
    create_settings_content();
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
    char buffer[256];
    char dateStr[16];

    // --- Update Tile 1: 7-Day Forecast with Actual Icons ---
    // Clear the tile first
    lv_obj_clean(t1);
    
    // Set background
    if (t1_dark) {
        lv_obj_set_style_bg_color(t1, lv_color_black(), 0);
    } else {
        lv_obj_set_style_bg_color(t1, lv_color_white(), 0);
    }
    lv_obj_set_style_bg_opa(t1, LV_OPA_COVER, 0);
    
    // Create title
    lv_obj_t *title = lv_label_create(t1);
    lv_label_set_text(title, "7-Day Forecast");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    if (t1_dark) {
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
    }
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Create a container for the forecast items
    lv_obj_t *cont = lv_obj_create(t1);
    lv_obj_set_size(cont, lv_disp_get_hor_res(NULL) - 20, lv_disp_get_ver_res(NULL) - 60);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 8, 0);
    lv_obj_set_style_pad_all(cont, 5, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    
    if (t1_dark) {
        lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
        lv_obj_set_style_border_color(cont, lv_color_hex(0x404040), 0);
    } else {
        lv_obj_set_style_bg_color(cont, lv_color_white(), 0);
    }
    
    // Add each day's forecast
    for (int i = 0; i < 7; i++)
    {
        formatDate(sevenDayForecast.hours[i].time, dateStr, sizeof(dateStr));
        
        // Create row container for each day
        lv_obj_t *row = lv_obj_create(cont);
        lv_obj_set_size(row, lv_pct(100), 40);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(row, 5, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        
        if (t1_dark) {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x202020), 0);
        } else {
            lv_obj_set_style_bg_color(row, lv_color_hex(0xF5F5F5), 0);
        }
        
        // Create and add weather icon
        lv_obj_t *icon = createWeatherIcon(row, sevenDayForecast.hours[i].weatherCondition);
        
        // Date label
        lv_obj_t *date_label = lv_label_create(row);
        lv_label_set_text(date_label, dateStr);
        lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
        if (t1_dark) {
            lv_obj_set_style_text_color(date_label, lv_color_white(), 0);
        }
        lv_obj_set_width(date_label, 60);
        
        // Temperature label
        lv_obj_t *temp_label = lv_label_create(row);
        snprintf(buffer, sizeof(buffer), "%.1f°C", sevenDayForecast.hours[i].temperature);
        lv_label_set_text(temp_label, buffer);
        lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_18, 0);
        if (t1_dark) {
            lv_obj_set_style_text_color(temp_label, lv_color_white(), 0);
        }
        lv_obj_set_width(temp_label, 65);
        
        // Condition label
        lv_obj_t *cond_label = lv_label_create(row);
        lv_label_set_text(cond_label, getWeatherString(sevenDayForecast.hours[i].weatherCondition));
        lv_obj_set_style_text_font(cond_label, &lv_font_montserrat_14, 0);
        if (t1_dark) {
            lv_obj_set_style_text_color(cond_label, lv_color_white(), 0);
        }
        lv_obj_set_flex_grow(cond_label, 1);
    }
    
    // Re-add click event
    lv_obj_add_flag(t1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t1, on_tile1_clicked, LV_EVENT_CLICKED, NULL);

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
