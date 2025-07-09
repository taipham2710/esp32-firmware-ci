// config.h - ESP32 Configuration
#pragma once

// WiFi Configuration
#define WIFI_SSID   "your_wifi_ssid"
#define WIFI_PASS   "your_wifi_password"

// MQTT Configuration (via ngrok tunnel)
#define MQTT_HOST   "your_ngrok_mqtt_tunnel.ngrok.io"  // Thay bằng URL ngrok MQTT
#define MQTT_PORT   1883

// OTA Server Configuration (via ngrok tunnel - HTTPS)
#define OTA_SERVER  "http://your_ngrok_web_tunnel.ngrok.io"  // Thay bằng URL HTTP

// Device Configuration
#define DEVICE_ID   "esp32-001"
#define FIRMWARE_VERSION "1.0.0"

// Authentication
#define AUTH_TOKEN  "your_api_token_here" 