# ESP32 Firmware CI/CD - MQTT & OTA DevOps Project

## Kiến trúc dự án

```
esp32-firmware-ci/
├── src/                 # Code nguồn chính
│   └── main.cpp         # Chương trình firmware ESP32 (WiFi, MQTT, OTA)
├── include/             # Header tự định nghĩa
├── lib/                 
├── test/                # Unit test cho các hàm logic
│   └── test_main.cpp
├── platformio.ini       # File cấu hình PlatformIO (board, lib, build...)
├── .github/
│   └── workflows/
│       └── ci.yml       # Workflow CI/CD: build, unit test, upload firmware OTA
├── README.md            
```

---

## 1. Chức năng chính

- **Kết nối WiFi & MQTT**: Gửi/nhận dữ liệu, điều khiển với Broker (EMQX) tại Edge.
- **Tích hợp OTA Firmware**: ESP32 tự động kiểm tra, tải và cập nhật firmware mới từ Web server.
- **CI/CD tự động**: Build, test, upload firmware lên OTA server khi push lên GitHub.
- **Unit test**: Kiểm thử logic với PlatformIO.

---

## 2. Hướng phát triển

### 2.1. Yêu cầu

- [VSCode + PlatformIO extension](https://platformio.org/install/ide?install=vscode) **hoặc** Python 3 + PlatformIO CLI (`pip install platformio`)
- ESP32 DevKit hoặc board tương đương

### 2.2. Build local

```bash
# Cài PlatformIO nếu cần
pip install platformio

# Build firmware (tại thư mục gốc)
pio run
```
File firmware tạo ra: `.pio/build/esp32dev/firmware.bin`

### 2.3. Flash firmware lên ESP32

```bash
pio run -t upload
```

### 2.4. Chỉnh sửa cấu hình

- Sửa các thông tin WiFi, MQTT, OTA server trong `src/main.cpp`:
  ```cpp
  const char* ssid = "YOUR_WIFI_SSID";
  const char* password = "YOUR_WIFI_PASSWORD";
  const char* mqtt_server = "192.168.1.100"; // Địa chỉ EMQX edge
  const char* ota_version_url = "http://<OTA_SERVER>/firmware/version.json";
  const char* ota_bin_url_tmpl = "http://<OTA_SERVER>/firmware/esp32-v%s.bin";
  const char* firmware_version = "1.0.0";
  ```

---

## 3. CI/CD Workflow (GitHub Actions)

- Khi push lên branch `main`:
  - Tự động build firmware.
  - Chạy unit test.
  - Upload firmware lên server OTA nếu build thành công.

**Secrets cần cấu hình trên repo:**

- `FIRMWARE_UPLOAD_URL`: Endpoint upload firmware (ví dụ: https://ota.example.com/upload)
- `FIRMWARE_UPLOAD_TOKEN`: Token xác thực upload (Bearer Token)

---

## 4. OTA Firmware Update

- ESP32 kiểm tra version mới theo chu kỳ (mặc định 1h/lần) tại `version.json` (trên web server).
- Nếu có phiên bản mới, tự động tải và flash firmware mới qua HTTP.

---

## 5. Unit Test với PlatformIO

- Viết test logic trong thư mục `test/`.
- Chạy test local:

  ```bash
  pio test
  ```

- Test tự động chạy trong CI/CD workflow.

---

## 6. Tài liệu tham khảo

- [PlatformIO Docs](https://docs.platformio.org/)
- [PubSubClient (MQTT)](https://pubsubclient.knolleary.net/)
- [ArduinoJson](https://arduinojson.org/)
- [ESP32 Arduino OTA](https://github.com/espressif/arduino-esp32/blob/master/libraries/Update/examples/HTTPUpdate/HTTPUpdate.ino)

---