/**
 * @file wifi_auto.cpp
 * @author Antonio Calatrava
 * @brief This file contains the implementation of the WiFi auto connection utility functions.
 * @version 0.1
 * @date 2024-04-18
 */

#include "wifi_auto.h"
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_sntp.h"
#include "spdlog/spdlog.h"
#include <stdlib.h>
#include <cstdlib>
#include <esp_system.h>
#include "ezTime/src/ezTime.h"

// Define a struct to hold WiFi credentials
struct WiFiCredentials
{
    char ssid[SSID_SIZE];
    char password[PASSWORD_SIZE];
};

void wifi_auto_save(const char *ssid, const char *password)
{
    // Create a WiFiCredentials struct from the input parameters
    WiFiCredentials credentials;
    strncpy(credentials.ssid, ssid, SSID_SIZE - 1);
    credentials.ssid[SSID_SIZE - 1] = '\0'; // Add null terminator
    strncpy(credentials.password, password, PASSWORD_SIZE - 1);
    credentials.password[PASSWORD_SIZE - 1] = '\0'; // Add null terminator

    // Write to console that we are saving the credentials
    spdlog::info("Saving credentials to Preferences");

    Preferences preferences;
    preferences.begin("wifi", false);
    preferences.putString("ssid", credentials.ssid);
    preferences.putString("password", credentials.password);
    preferences.end();
}

void wifi_auto_clear()
{
    Preferences preferences;
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
}

bool wifi_auto_connect(const char *ssid, const char *password)
{
    spdlog::info("wifi_auto_connect");
    spdlog::info(ssid);
    spdlog::info(password);
    WiFi.begin(ssid, password);
    WiFi.waitForConnectResult(10 * 1000);
    if (WiFi.status() == WL_CONNECTED)
    {
        // Save credentials
        wifi_auto_save(ssid, password);

        // Set timezone
        if (!esp_sntp_enabled())
        {
            // Get timezone using API
            WiFiClient client;
            HTTPClient http;

            http.begin(client, "http://ip-api.com/json");
            int httpCode = http.GET();

            if (httpCode == HTTP_CODE_OK)

            {
                String response = http.getString();
                spdlog::info(response.c_str());

                // Parse the JSON response manually
                int startIndex = response.indexOf("\"timezone\":\"") + 12;
                int endIndex = response.indexOf("\"", startIndex);
                String timeZone = response.substring(startIndex, endIndex);
                spdlog::info("Time Zone: ");
                spdlog::info(timeZone.c_str());

                Timezone currentTimezone;
                currentTimezone.setLocation(timeZone);
                currentTimezone.setDefault();

                spdlog::info(currentTimezone.getPosix().c_str());
                setenv("TZ", currentTimezone.getPosix().c_str(), 1);
            }
            else
            {
                spdlog::error("Error: " + http.errorToString(httpCode));
                setenv("TZ", "GMT", 1);
            }

            tzset();
            esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "pool.ntp.org");
            esp_sntp_init();
        }
    }
    return WiFi.status() == WL_CONNECTED;
}

bool wifi_is_credentials_saved()
{
    WiFiCredentials credentials;
    Preferences preferences;
    preferences.begin("wifi", true);
    int ssid_len = preferences.getString("ssid", credentials.ssid, SSID_SIZE);
    int pwd_len = preferences.getString("password", credentials.password, PASSWORD_SIZE);
    preferences.end();

    return ssid_len > 0 && pwd_len > 0;
}

bool wifi_auto_connect_if_saved()
{
    spdlog::info("wifi_auto_connect_if_saved");
    WiFiCredentials credentials;
    Preferences preferences;
    preferences.begin("wifi", true);
    int ssid_len = preferences.getString("ssid", credentials.ssid, SSID_SIZE);
    int pwd_len = preferences.getString("password", credentials.password, PASSWORD_SIZE);
    preferences.end();

    if (ssid_len > 0 && pwd_len > 0)
    {
        spdlog::info(credentials.ssid);
        spdlog::info(credentials.password);
        for (int i = 0; i < 3; i++)
        {
            if (wifi_auto_connect(credentials.ssid, credentials.password))
            {
                return true;
            }
            delay(1000);
        }
        return false;
    }
    else
    {
        spdlog::warn("No saved WiFi credentials.");
        return false;
    }
}