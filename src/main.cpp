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
#define LED_RED 13      // Error message (WiFi/OTA fail) - GPIO 13 is safer
#define LED_GREEN 14    // Normal operation report - GPIO 14 is safer

// FreeRTOS task handles
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t httpTaskHandle = NULL;

// Mutex for shared data
SemaphoreHandle_t dataMutex = NULL;

unsigned long lastErrorBlink = 0;
bool errorLedState = false;
bool wifiConnected = false;
bool otaFailFlag = false;
bool hasError = false; // Flag to indicate if there is any error

// Global secure client để tái sử dụng kết nối
WiFiClientSecure* globalSecureClient = nullptr;
unsigned long lastConnectionTime = 0;
const unsigned long CONNECTION_REUSE_TIMEOUT = 30000; // 30 seconds

// Cấu hình SSL cho ngrok
void configureSSLClient(WiFiClientSecure* client) {
    client->setInsecure(); // Tạm thời bypass SSL verification
    client->setHandshakeTimeout(30); // 30 giây cho handshake
    client->setTimeout(20000); // 20 giây cho read/write
}

void addCommonHeaders(HTTPClient &http) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + AUTH_TOKEN);
    // Thêm headers cho ngrok
    http.addHeader("Connection", "close"); // Không dùng keep-alive với ngrok
    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "ESP32-OTA-Client/1.0");
}

// Khởi tạo global secure client
void initGlobalSecureClient() {
    if (globalSecureClient == nullptr) {
        globalSecureClient = new WiFiClientSecure();
        configureSSLClient(globalSecureClient);
        Serial.println("[SSL] Global secure client initialized with enhanced config");
    }
}

// Kiểm tra và reset connection nếu cần
void checkAndResetConnection() {
    unsigned long now = millis();
    if (globalSecureClient && (now - lastConnectionTime > CONNECTION_REUSE_TIMEOUT)) {
        Serial.println("[SSL] Resetting secure client connection");
        globalSecureClient->stop();
        delete globalSecureClient;
        globalSecureClient = nullptr;
        initGlobalSecureClient();
        lastConnectionTime = now;
    }
}

// Hàm helper để thực hiện HTTP request với error handling tốt hơn
int performHTTPRequest(const String& url, const String& method, const String& body = "") {
    HTTPClient http;
    int retryCount = 3;
    int httpCode = -1;

    for (int attempt = 1; attempt <= retryCount; attempt++) {
        Serial.printf("[HTTP] Attempt %d/%d for URL: %s\n", attempt, retryCount, url.c_str());

        http.setReuse(false);
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

        if (!http.begin(url)) { // Dùng HTTP, không cần WiFiClientSecure
            Serial.println("[HTTP] Failed to begin HTTP connection");
            delay(2000);
            continue;
        }

        http.setTimeout(20000);
        addCommonHeaders(http);

        if (method == "POST") {
            httpCode = http.POST(body);
        } else {
            httpCode = http.GET();
        }

        Serial.printf("[HTTP] HTTP response code: %d\n", httpCode);

        http.end();

        if (httpCode > 0 && httpCode < 400) {
            Serial.printf("[HTTP] Request successful with code: %d\n", httpCode);
            return httpCode;
        }

        delay(2000);
    }

    Serial.printf("[HTTP] All attempts failed. Final code: %d\n", httpCode);
    return httpCode;
}

void setup_wifi() {
    Serial.print("Connecting to WiFi...");
    digitalWrite(LED_GREEN, LOW);  // Turn off green LED initially
    digitalWrite(LED_RED, HIGH);   // Turn on red LED to indicate connection attempt
    wifiConnected = false;
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    digitalWrite(LED_GREEN, HIGH); // Turn on green LED when connected
    digitalWrite(LED_RED, LOW);    // Turn off red LED when connected
    wifiConnected = true;
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[WiFi] Waiting for NTP time sync...");
    time_t now = time(nullptr);
    while (now < 1700000000) {  // Giá trị ~2023 trở lên
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println();
    Serial.print("[WiFi] Time synchronized: ");
    Serial.println(ctime(&now));
}

void reconnect() {
    int retryCount = 0;
    const int maxRetries = 3;
    
    while (!client.connected() && retryCount < maxRetries) {
        Serial.println("[MQTT] Attempting to connect to broker...");
        
        // Generate a unique client ID
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("[MQTT] Connected to broker!");
            // Subscribe to any required topics here
            // client.subscribe("topic/example");
            return;
        } else {
            retryCount++;
            Serial.print("[MQTT] Failed, rc=");
            Serial.print(client.state());
            Serial.printf(" attempt %d/%d\n", retryCount, maxRetries);
            
            // Exponential backoff
            delay(3000 * retryCount);
        }
    }
    
    if (retryCount >= maxRetries) {
        Serial.println("[MQTT] Max retry attempts reached, will try again later");
    }
}

void blinkErrorLed() {
    unsigned long now = millis();
    if (now - lastErrorBlink > 2000) { // Blink every 2 seconds
        errorLedState = !errorLedState;
        digitalWrite(LED_RED, errorLedState ? HIGH : LOW);
        lastErrorBlink = now;
    }
} 

bool sendHeartbeatWithRetry(int maxRetry = 3) {
    if (String(OTA_SERVER).length() == 0) {
        Serial.println("[Heartbeat] OTA_SERVER not configured!");
        return false;
    }
    String heartbeatUrl = String(OTA_SERVER) + "/api/heartbeat";
    if (heartbeatUrl.startsWith("https://")) heartbeatUrl.replace("https://", "http://");
    String body = String("{\"device_id\":\"") + DEVICE_ID + "\"," +
                  "\"status\":\"online\"," +
                  "\"firmware_version\":\"" + FIRMWARE_VERSION + "\"}";
    Serial.print("[Heartbeat] Sending to: "); Serial.println(heartbeatUrl);
    int code = performHTTPRequest(heartbeatUrl, "POST", body);
    if (code > 0 && code < 400) {
        Serial.println("[Heartbeat] Sent successfully!");
        return true;
    } else {
        Serial.println("[Heartbeat] Failed to send!");
        otaFailFlag = true;
        return false;
    }
}

bool sendOtaLogWithRetry(const char* status, const char* version, const char* error_message, int latency_ms, int maxRetry = 3) {
    if (String(OTA_SERVER).length() == 0) {
        Serial.println("[OTA Log] OTA_SERVER not configured!");
        return false;
    }
    String logUrl = String(OTA_SERVER) + "/api/log";
    if (logUrl.startsWith("https://")) logUrl.replace("https://", "http://");
    String body = String("{\"device_id\":\"") + DEVICE_ID + "\"," +
                  "\"status\":\"" + status + "\"," +
                  "\"version\":\"" + version + "\"," +
                  "\"error_message\":\"" + error_message + "\"," +
                  "\"latency_ms\":" + String(latency_ms) + "}";
    Serial.print("[OTA Log] Sending to: "); Serial.println(logUrl);
    int code = performHTTPRequest(logUrl, "POST", body);
    if (code > 0 && code < 400) {
        Serial.println("[OTA Log] Sent successfully!");
        return true;
    } else {
        Serial.println("[OTA Log] Failed to send!");
        return false;
    }
}

// Hàm so sánh version dạng x.y.z, trả về 1 nếu v1 > v2, -1 nếu v1 < v2, 0 nếu bằng nhau
int compareVersion(const String& v1, const String& v2) {
    int vnum1 = 0, vnum2 = 0;
    int i = 0, j = 0;
    while (i < v1.length() || j < v2.length()) {
        vnum1 = 0;
        vnum2 = 0;
        while (i < v1.length() && v1[i] != '.') {
            vnum1 = vnum1 * 10 + (v1[i] - '0');
            i++;
        }
        while (j < v2.length() && v2[j] != '.') {
            vnum2 = vnum2 * 10 + (v2[j] - '0');
            j++;
        }
        if (vnum1 > vnum2) return 1;
        if (vnum1 < vnum2) return -1;
        i++;
        j++;
    }
    return 0;
}

// Gửi thông báo lên Slack nếu có webhook
void sendSlackNotification(const String& message) {
#ifdef SLACK_WEBHOOK_URL
    HTTPClient http;
    http.begin(SLACK_WEBHOOK_URL);
    http.addHeader("Content-Type", "application/json");
    String payload = String("{\"text\": ") + "\"" + message + "\"}";
    int code = http.POST(payload);
    http.end();
    Serial.printf("[Slack] Sent notification, code: %d\n", code);
#endif
}

bool checkAndUpdateFirmware() {
    if (String(OTA_SERVER).length() == 0) {
        Serial.println("[OTA] OTA_SERVER not configured!");
        return false;
    }
    String versionUrl = String(OTA_SERVER) + "/api/firmware/version?device=esp32";
    Serial.print("[OTA] Checking firmware version at: ");
    Serial.println(versionUrl);

    HTTPClient http;
    WiFiClientSecure secureClient;
    secureClient.setHandshakeTimeout(30);
    secureClient.setTimeout(20000);

    http.setReuse(false);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    if (!http.begin(secureClient, versionUrl)) {
        Serial.println("[OTA] Failed to begin HTTP connection");
        otaFailFlag = true;
        return false;
    }

    int httpCode = http.GET();
    Serial.printf("[OTA] Version check response code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_TEMPORARY_REDIRECT || 
        httpCode == HTTP_CODE_FOUND || 
        httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String redirectedUrl = http.header("Location");
        Serial.printf("[OTA] Redirected to: %s\n", redirectedUrl.c_str());
        httpCode = 200; // Treat as success for ngrok redirects
    }

    if (httpCode == 200) {
        String payload = http.getString();
        Serial.print("[OTA] Response payload: "); Serial.println(payload);
        int vIdx = payload.indexOf("\"version\":");
        int urlIdx = payload.indexOf("\"url\":");
        if (vIdx > 0 && urlIdx > 0) {
            String newVersion = payload.substring(vIdx + 11, payload.indexOf('"', vIdx + 12));
            String url = payload.substring(urlIdx + 7, payload.indexOf('"', urlIdx + 8));
            Serial.printf("[OTA] Current version: %s, Available version: %s\n", FIRMWARE_VERSION, newVersion.c_str());
            int cmp = compareVersion(newVersion, FIRMWARE_VERSION);
            if (cmp > 0) {
                Serial.print("[OTA] New firmware available: "); Serial.println(newVersion);
                Serial.print("[OTA] Downloading from: "); Serial.println(url);
                sendSlackNotification(String("[OTA] New firmware available: ") + newVersion);
                http.end();
                digitalWrite(LED_GREEN, LOW);
                digitalWrite(LED_RED, HIGH);
                unsigned long t0 = millis();
                t_httpUpdate_return ret;
                if (url.startsWith("https://")) {
                    WiFiClientSecure secureClient2;
                    secureClient2.setHandshakeTimeout(30);
                    secureClient2.setTimeout(30000);
                    ret = HTTPUpdate().update(secureClient2, url.c_str());
                } else {
                    ret = HTTPUpdate().update(espClient, url.c_str());
                }
                int latency = millis() - t0;
                if (ret == HTTP_UPDATE_OK) {
                    Serial.println("[OTA] Update successful!");
                    sendOtaLogWithRetry("update_success", newVersion.c_str(), "", latency);
                    sendSlackNotification(String("[OTA] Update successful: ") + newVersion);
                    delay(2000);
                    ESP.restart();
                } else {
                    Serial.print("[OTA] Update failed, code: "); Serial.println((int)ret);
                    sendOtaLogWithRetry("update_failed", newVersion.c_str(), "OTA failed", latency);
                    sendSlackNotification(String("[OTA] Update failed: ") + newVersion);
                    otaFailFlag = true;
                }
                return true;
            } else {
                Serial.println("[OTA] Firmware is up to date.");
            }
        } else {
            Serial.println("[OTA] Failed to parse version response");
        }
    } else {
        Serial.print("[OTA] Failed to check firmware version, HTTP code: "); Serial.println(httpCode);
        otaFailFlag = true;
    }
    http.end();
    return false;
}

void sendSensorDataHttp(float temp, float humidity, float light, int maxRetry = 3) {
    if (String(OTA_SERVER).length() == 0) {
        Serial.println("[Sensor] OTA_SERVER not configured!");
        return;
    }
    String sensorUrl = String(OTA_SERVER) + "/api/sensor";
    if (sensorUrl.startsWith("https://")) sensorUrl.replace("https://", "http://");
    String body = String("{\"device_id\":\"") + DEVICE_ID + "\"," +
                  "\"temp\":" + String(temp, 2) + "," +
                  "\"humidity\":" + String(humidity, 2) + "," +
                  "\"light\":" + String(light, 2) + "}";
    Serial.print("[Sensor] Sending to: "); Serial.println(sensorUrl);
    int code = performHTTPRequest(sensorUrl, "POST", body);
    if (code > 0 && code < 400) {
        Serial.println(" -> Success!");
    } else {
        Serial.println(" -> Failed!");
        otaFailFlag = true;
    }
}

// MQTT Task - runs on Core 1
void mqttTask(void *pvParameters) {
    Serial.println("[MQTT Task] Started on Core " + String(xPortGetCoreID()));
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!client.connected()) {
                reconnect();
            }
            
            if (client.connected()) {
                client.loop();
                
                // Generate sensor data
                float temp = 25.0 + (rand() % 1000) / 100.0;
                float humidity = 50.0 + (rand() % 1000) / 100.0;
                float light = 100.0 + (rand() % 1000) / 10.0;
                
                // Publish to MQTT with QoS 1 and retain flag
                char payload[128];
                snprintf(payload, sizeof(payload), 
                    "{\"device_id\":\"%s\",\"temp\":%.2f,\"humidity\":%.2f,\"light\":%.2f}", 
                    DEVICE_ID, temp, humidity, light);
                
                if (client.publish("edge/sensor/data", payload, true)) {
                    Serial.print("[MQTT] Published: "); Serial.println(payload);
                } else {
                    Serial.println("[MQTT] Failed to publish message");
                }
            }
        }
        
        // Use proper FreeRTOS delay
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000));
    }
}

// HTTP Task - runs on Core 1
void httpTask(void *pvParameters) {
    Serial.println("[HTTP Task] Started on Core " + String(xPortGetCoreID()));
    
    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            // Generate sensor data
            float temp = 25.0 + (rand() % 1000) / 100.0;
            float humidity = 50.0 + (rand() % 1000) / 100.0;
            float light = 100.0 + (rand() % 1000) / 10.0;
            
            // Send sensor data via HTTP (giảm tần suất để tránh overload)
            sendSensorDataHttp(temp, humidity, light);
            
            // Send heartbeat every 60 seconds (tăng từ 30s)
            unsigned long now = millis();
            if (now - lastHeartbeat > 60000) {
                Serial.println("[HTTP Task] Sending heartbeat...");
                sendHeartbeatWithRetry();
                lastHeartbeat = now;
            }
            
            // Check for OTA update every 5 minutes (tăng từ 60s)
            if (now - lastOtaCheck > 300000) {
                Serial.println("[HTTP Task] Checking for OTA update...");
                checkAndUpdateFirmware();
                lastOtaCheck = now;
            }
        } else {
            Serial.println("[HTTP Task] WiFi not connected, waiting...");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Tăng delay lên 10 seconds để giảm tải
    }
}

// Thêm hàm test HTTPS với endpoint công khai
void testPublicHTTPS() {
    Serial.println("[TEST] Bắt đầu kiểm tra HTTPS với endpoint công khai...");
    HTTPClient http;
    WiFiClientSecure secureClient;
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    const char* testUrl = "https://jsonplaceholder.typicode.com/posts/1";
    if (!http.begin(secureClient, testUrl)) {
        Serial.println("[TEST] Không thể bắt đầu kết nối HTTPS tới endpoint công khai!");
        return;
    }
    int httpCode = http.GET();
    Serial.printf("[TEST] Mã phản hồi HTTPS: %d\n", httpCode);
    if (httpCode > 0 && httpCode < 400) {
        String payload = http.getString();
        Serial.println("[TEST] Nhận dữ liệu thành công từ endpoint công khai!");
        Serial.println(payload);
    } else {
        Serial.println("[TEST] Lỗi khi truy cập endpoint công khai!");
    }
    http.end();
}

void setup() {
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_RED, LOW);    // Turn off red LED initially
    digitalWrite(LED_GREEN, LOW);  // Turn off green LED initially
    Serial.begin(115200);
    
    setup_wifi();
    
    // Initialize global secure client
    initGlobalSecureClient();
    
    // Configure MQTT client
    client.setServer(MQTT_HOST, MQTT_PORT);
    client.setKeepAlive(60); // Set keepalive to 60 seconds
    client.setSocketTimeout(20); // Socket timeout 20 seconds
    client.setBufferSize(2048); // Increase buffer size

    // Create mutex for shared data
    dataMutex = xSemaphoreCreateMutex();

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
            sendSlackNotification(String("[OTA] Firmware rollback to version: ") + FIRMWARE_VERSION);
            Serial.println("[OTA] Firmware rollback log sent.");
        }
    }
    
    // Create FreeRTOS tasks
    Serial.println("[Setup] Creating FreeRTOS tasks...");
    xTaskCreatePinnedToCore(
        mqttTask,           // Task function
        "MQTT Task",        // Task name
        4096,               // Stack size
        NULL,               // Parameters
        1,                  // Priority
        &mqttTaskHandle,    // Task handle
        1                   // Core (1 for MQTT)
    );
    
    xTaskCreatePinnedToCore(
        httpTask,           // Task function
        "HTTP Task",        // Task name
        8192,               // Stack size (larger for HTTP operations)
        NULL,               // Parameters
        1,                  // Priority
        &httpTaskHandle,    // Task handle
        1                   // Core (1 for HTTP)
    );
    
    Serial.println("[Setup] FreeRTOS tasks created successfully!");

    Serial.println("[Setup] Bắt đầu test HTTPS endpoint công khai để xác định lỗi SSL...");
    testPublicHTTPS();
}

void loop() {
    // Main loop now only handles LED status and WiFi monitoring
    // MQTT and HTTP operations are handled by FreeRTOS tasks
    
    // Check WiFi connection status
    if (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_GREEN, LOW);  // Turn off green LED when WiFi is not connected
        digitalWrite(LED_RED, HIGH);   // Turn on red LED when WiFi is not connected
        Serial.println("[Main] WiFi not connected, retrying...");
        
        // Clean up secure client on WiFi disconnection
        if (globalSecureClient) {
            globalSecureClient->stop();
            delete globalSecureClient;
            globalSecureClient = nullptr;
        }
        
        setup_wifi();
        
        // Reinitialize secure client after WiFi reconnection
        initGlobalSecureClient();
        
        wifiConnected = false;
        hasError = true;
    } else {
        wifiConnected = true;
        
        // Check another error condition: OTA failure
        if (otaFailFlag) {
            hasError = true;
            digitalWrite(LED_GREEN, LOW);  // Turn off green LED when there is an error
            digitalWrite(LED_RED, HIGH);   // Turn on red LED when there is an error
        } else {
            hasError = false;
            digitalWrite(LED_GREEN, HIGH); // Turn on green LED when no error
            digitalWrite(LED_RED, LOW);    // Turn off red LED when no error
        }
    }
    
    // Small delay to prevent watchdog issues
    delay(1000);
}