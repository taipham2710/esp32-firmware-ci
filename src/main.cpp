#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) delay(500);
}

void reconnect() {
    while (!client.connected()) client.connect("ESP32Client");
}

void setup() {
    Serial.begin(115200);
    setup_wifi();
    client.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop();
    float temp = 25.0 + (rand() % 1000) / 100.0;
    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", temp);
    client.publish("edge/sensor/temperature", payload);
    delay(5000);
}