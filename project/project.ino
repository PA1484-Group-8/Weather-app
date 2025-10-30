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
static const char* WIFI_SSID     = "AN";
static const char* WIFI_PASSWORD = "3feC=Mic@iKsi&Da";

LilyGo_Class amoled;

static lv_obj_t* tileview;
static lv_obj_t* t1;
static lv_obj_t* t2;
static lv_obj_t* t3;       // Tile #3
static lv_obj_t* t1_label;
static lv_obj_t* t2_label;
static lv_obj_t* t3_label;
static bool t2_dark = false;            // start tile #2 in light mode

// track Wi-Fi connection, whenever you need to access the internet you need to check that this is true.
static bool wifi_was_connected = false; 
static unsigned long last_wifi_update = 0; // track last Wi-Fi update time

static unsigned long last_weather_update = 0;
static const long WEATHER_UPDATE_INTERVAL = 1 * 60 * 1000; // Update weather every minute (in milliseconds)


// Function: Tile #2 Color change
static void apply_tile_colors(lv_obj_t* tile, lv_obj_t* label, bool dark)
{
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tile, dark ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_text_color(label, dark ? lv_color_white() : lv_color_black(), 0);
}

// Tile #2 click toggle
static void on_tile2_clicked(lv_event_t* e)
{
    LV_UNUSED(e);
    t2_dark = !t2_dark;
    apply_tile_colors(t2, t2_label, t2_dark);
}

// Function: Polished 3-second boot screen
static void show_boot_screen()
{
    lv_obj_t* scr = lv_scr_act();

    // Black background
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Label: Name, Group, Firmware
    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "Group 8\nFirmware v1.2.0");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    // Horizontal progress line
    lv_obj_t* bar = lv_bar_create(scr);
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
        if (lineValue > 100) lineValue = 100;

        lv_bar_set_value(bar, lineValue, LV_ANIM_OFF);
        lv_timer_handler();
        delay(5);
    }

    lv_obj_clean(scr); // clear before main UI
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

// Function: Creates UI
static void create_ui()
{
    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

    // Add three horizontal tiles
    t1 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR);
    t2 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);
    t3 = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR); // Wi-Fi tile

    // Tile #1
    t1_label = lv_label_create(t1);
    lv_label_set_text(t1_label, "Hello Students");
    lv_obj_set_style_text_font(t1_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t1_label);
    apply_tile_colors(t1, t1_label, false);

    // Tile #2
    t2_label = lv_label_create(t2);
    lv_label_set_text(t2_label, "Welcome to the workshop");
    lv_obj_set_style_text_font(t2_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t2_label);
    apply_tile_colors(t2, t2_label, false);
    lv_obj_add_flag(t2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t2, on_tile2_clicked, LV_EVENT_CLICKED, NULL);

    // Tile #3 (Wi-Fi status)
    t3_label = lv_label_create(t3);
    lv_label_set_text(t3_label, "Wi-Fi: Connecting...");
    lv_obj_set_style_text_font(t3_label, &lv_font_montserrat_28, 0);
    lv_obj_center(t3_label);
    apply_tile_colors(t3, t3_label, false);
}


/**
 * @brief Fetches weather data from SMHI API, documentation: https://opendata.smhi.se/metobs/introduction.
 */
static void fetchWeatherData()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Cannot fetch weather, Wi-Fi not connected.");
        lv_label_set_text(t1_label, "Wi-Fi is disconnected.\nCannot fetch weather.");
        lv_obj_center(t1_label);
        return;
    }

    Serial.println("Fetching weather data...");
    lv_label_set_text(t1_label, "Fetching weather data...");
    lv_obj_center(t1_label);

    HTTPClient http;
    char api_url[256];

    String version = "1.0";
    String parameter = "1"; // Lufttemperatur
    String station = "65090"; // Karlskrona-Söderstjerna
    String period = "latest-hour";

    // https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/1/station/65090/period/latest-hour/data.json
    // Construct the API URL
    snprintf(api_url, sizeof(api_url),
             "https://opendata-download-metobs.smhi.se/api/version/%s/parameter/%s/station/%s/period/%s/data.json",
             version.c_str(), parameter.c_str(), station.c_str(), period.c_str());

    Serial.printf("API URL: %s\n", api_url);

    if (http.begin(api_url))
    {
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            Serial.println("API Response:");
            Serial.println(payload);

            // Parse the JSON response
            DynamicJsonDocument doc(2048); // Allocate space for the JSON document
            DeserializationError error = deserializeJson(doc, payload);

            if (error)
            {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
                lv_label_set_text(t1_label, "Failed to parse\nweather data.");
                lv_obj_center(t1_label);
            }
            else
            {
                
                // Extract data
                const char* temp = doc["value"][0]["value"];
                // Format the output string
                char weather_buf[256];
                snprintf(weather_buf, sizeof(weather_buf),
                         "Karlskrona\n"
                         "%s °C\n",
                         temp);

                // Update Tile #1 label
                lv_label_set_text(t1_label, weather_buf);
                lv_obj_center(t1_label);
            }
        }
        else
        {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            char error_buf[100];
            snprintf(error_buf, sizeof(error_buf), "HTTP Error: %d\nCheck API Key?", httpCode);
            lv_label_set_text(t1_label, error_buf);
            lv_obj_center(t1_label);
        }

        http.end();
    }
    else
    {
        Serial.printf("[HTTP] Unable to connect to %s\n", api_url);
        lv_label_set_text(t1_label, "Failed to connect\nto weather server.");
        lv_obj_center(t1_label);
    }
}

// Must-have setup function
void setup()
{
    Serial.begin(115200);
    delay(200);

    if (!amoled.begin()) {
        Serial.println("Failed to init LilyGO AMOLED.");
        while (true) delay(1000);
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
        fetchWeatherData();
        last_weather_update = millis();
    }
}
