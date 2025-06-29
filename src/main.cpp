#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <PubSubClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "config.h"

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastHeartbeat = 0;
unsigned long lastOtaCheck = 0;
unsigned long lastLog = 0;

void setup_wifi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    if (!client.connected()) {
        Serial.println("[MQTT] Attempting to connect to broker...");
        if (client.connect("ESP32Client")) {
            Serial.println("[MQTT] Connected to broker!");
        } else {
            Serial.print("[MQTT] Failed, rc=");
            Serial.print(client.state());
            Serial.println(" will retry later.");
            delay(5000);
        }
    }
}

bool sendHeartbeatWithRetry(int maxRetry = 3) {
    for (int i = 0; i < maxRetry; ++i) {
        HTTPClient http;
        http.begin(String(OTA_SERVER) + "/api/heartbeat");
        http.addHeader("Content-Type", "application/json");
        String body = String("{\"device_id\":\"") + DEVICE_ID + "\"," +
                      "\"status\":\"online\"," +
                      "\"firmware_version\":\"" + FIRMWARE_VERSION + "\"}";
        int code = http.POST(body);
        http.end();
        Serial.print("[Heartbeat] Status code: "); Serial.println(code);
        if (code > 0 && code < 400) {
            Serial.println("[Heartbeat] Sent successfully!");
            return true;
        }
        delay(1000); // wait before retrying
        Serial.print("[Heartbeat] Retry "); Serial.print(i + 1); Serial.println(" of "); Serial.print(maxRetry);
        if (i < maxRetry - 1) {
            Serial.println("[Heartbeat] Retrying...");
        } else {
            Serial.println("[Heartbeat] No more retries left.");
        }
    }
    Serial.println("[Heartbeat] Failed to send after retries!");
    return false;
}

bool sendOtaLogWithRetry(const char* status, const char* version, const char* error_message, int latency_ms, int maxRetry = 3) {
    for (int i = 0; i < maxRetry; ++i) {
        HTTPClient http;
        http.begin(String(OTA_SERVER) + "/api/log");
        http.addHeader("Content-Type", "application/json");
        String body = String("{\"device_id\":\"") + DEVICE_ID + "\"," +
                      "\"status\":\"" + status + "\"," +
                      "\"version\":\"" + version + "\"," +
                      "\"error_message\":\"" + error_message + "\"," +
                      "\"latency_ms\":" + String(latency_ms) + "}";
        int code = http.POST(body);
        http.end();
        Serial.print("[OTA Log] Status: "); Serial.print(status);
        Serial.print(", Version: "); Serial.print(version);
        Serial.print(", Error: "); Serial.print(error_message);
        Serial.print(", Latency: "); Serial.print(latency_ms);
        Serial.print(", HTTP code: "); Serial.println(code);
        if (code > 0 && code < 400) {
            Serial.println("[OTA Log] Sent successfully!");
            return true;
        }
        delay(1000);
    }
    Serial.println("[OTA Log] Failed to send after retries!");
    return false;
}

bool checkAndUpdateFirmware() {
    HTTPClient http;
    http.begin(String(OTA_SERVER) + "/api/firmware/version?device=esp32");
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        int vIdx = payload.indexOf("\"version\":");
        int urlIdx = payload.indexOf("\"url\":");
        if (vIdx > 0 && urlIdx > 0) {
            String newVersion = payload.substring(vIdx + 11, payload.indexOf('"', vIdx + 12));
            String url = payload.substring(urlIdx + 7, payload.indexOf('"', urlIdx + 8));
            if (newVersion != FIRMWARE_VERSION) {
                Serial.print("[OTA] New firmware available: "); Serial.println(newVersion);
                Serial.print("[OTA] Downloading from: "); Serial.println(url);
                http.end();
                unsigned long t0 = millis();
                t_httpUpdate_return ret = HTTPUpdate().update(espClient, url.c_str());
                int latency = millis() - t0;
                if (ret == HTTP_UPDATE_OK) {
                    Serial.println("[OTA] Update successful!");
                    sendOtaLogWithRetry("update_success", newVersion.c_str(), "", latency);
                    delay(2000);
                    ESP.restart();
                } else {
                    Serial.print("[OTA] Update failed, code: "); Serial.println((int)ret);
                    sendOtaLogWithRetry("update_failed", newVersion.c_str(), "OTA failed", latency);
                }
                return true;
            } else {
                Serial.println("[OTA] Firmware is up to date.");
            }
        }
    } else {
        Serial.print("[OTA] Failed to check firmware version, HTTP code: "); Serial.println(httpCode);
    }
    http.end();
    return false;
}

void sendSensorDataHttp(float temp, float humidity, float light, int maxRetry = 3) {
    for (int i = 0; i < maxRetry; ++i) {
        HTTPClient http;
        http.begin(String(OTA_SERVER) + "/api/sensor"); // change to your actual sensor endpoint
        if (http.getSize() < 0) {
            Serial.println("[Sensor] Failed to connect to sensor endpoint!");
            http.end();
            return;
        }
        Serial.println("[Sensor] Sending sensor data...");
        Serial.print("[Sensor] Temp: "); Serial.print(temp);
        Serial.print(", Humidity: "); Serial.print(humidity);
        Serial.print(", Light: "); Serial.println(light);
        Serial.print("[Sensor] Attempt: "); Serial.println(i + 1);
        // Set headers and body
        http.addHeader("Content-Type", "application/json");
        String body = String("{\"device_id\":\"") + DEVICE_ID + "\"," +
                      "\"temp\":" + String(temp, 2) + "," +
                      "\"humidity\":" + String(humidity, 2) + "," +
                      "\"light\":" + String(light, 2) + "}";
        int code = http.POST(body);
        http.end();
        Serial.print("[Sensor] Temp: "); Serial.print(temp);
        Serial.print(", Humidity: "); Serial.print(humidity);
        Serial.print(", Light: "); Serial.print(light);
        Serial.print(", HTTP code: "); Serial.println(code);
        if (code > 0 && code < 400) {
            Serial.println("[Sensor] Data sent successfully!");
            return;
        }
        delay(1000);
    }
    Serial.println("[Sensor] Failed to send data after retries!");
}

void setup() {
    Serial.begin(115200);
    setup_wifi();
    client.setServer(MQTT_HOST, MQTT_PORT);

    // Rollback OTA: If new firmware is pending verification, wait for 30s and mark it as valid
    Serial.println("[OTA] Checking OTA state...");
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            Serial.println("[OTA] Firmware pending verify, waiting 30s to mark as valid...");
            delay(30000); // wait for 30 seconds
            Serial.println("[OTA] Marking firmware as valid...");
            // Mark the firmware as valid, preventing rollback
            esp_ota_mark_app_valid_cancel_rollback();
            Serial.println("[OTA] Firmware marked as valid, rollback cancelled.");
        } else if (ota_state == ESP_OTA_IMG_ABORTED) {
            // Firmware rollback detected
            Serial.println("[OTA] Firmware rollback detected!");
            // Send log to server
            Serial.print("[OTA] Current firmware version: "); Serial.println(FIRMWARE_VERSION);
            Serial.print("[OTA] Sending rollback log to server...");
            Serial.println("[OTA] Firmware rollback detected, sending log...");
            sendOtaLogWithRetry("rollback", FIRMWARE_VERSION, "Firmware rollback triggered", 0);
            Serial.println("[OTA] Firmware rollback log sent.");
        }
    }
}

void loop() {
    Serial.println("[DEBUG] loop running...");
    if (!client.connected()) reconnect();
    client.loop();
    // Send a lot of sensor data
    Serial.println("[Main] Sending sensor data...");
    float temp = 25.0 + (rand() % 1000) / 100.0;
    float humidity = 50.0 + (rand() % 1000) / 100.0;
    float light = 100.0 + (rand() % 1000) / 10.0;
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"temp\":%.2f,\"humidity\":%.2f,\"light\":%.2f}", temp, humidity, light);
    client.publish("edge/sensor/data", payload);
    Serial.print("[MQTT] Published: "); Serial.println(payload);
    sendSensorDataHttp(temp, humidity, light); // send sensor data via HTTP

    unsigned long now = millis();
    if (now - lastHeartbeat > 30000) { // sending heartbeat every 30s
        Serial.println("[Main] Sending heartbeat...");
        sendHeartbeatWithRetry();
        lastHeartbeat = now;
    }
    if (now - lastOtaCheck > 60000) { // checking OTA update every 60s
        Serial.println("[Main] Checking for OTA update...");
        checkAndUpdateFirmware();
        lastOtaCheck = now;
    }
    delay(5000); // simulate some delay for the loop
}