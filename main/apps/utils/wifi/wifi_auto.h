/**
 * @file wifi_auto.h
 * @author acalatrava
 * @brief
 * @version 0.1
 * @date 2024-04-18
 *
 * @copyright Copyright (c) 2024
 *
 */

// Define constants for SSID and password sizes
#define SSID_SIZE 32
#define PASSWORD_SIZE 64

#pragma once
void wifi_auto_clear();
bool wifi_auto_connect(const char *ssid, const char *password);
bool wifi_auto_connect_if_saved();
bool wifi_is_credentials_saved();