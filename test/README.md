# Hướng dẫn Kiểm thử Hệ thống ESP32 OTA

### 📋 Tổng quan

Tài liệu này hướng dẫn thực hiện 2 kịch bản kiểm thử chính:

1. **Kịch bản 1**: Kiểm thử Quy trình OTA và Khôi phục Tự động
2. **Kịch bản 2**: Kiểm thử Hiệu suất và Bảo mật

### 🛠️ Chuẩn bị

#### 1. Cài đặt dependencies
```bash
# Trong thư mục web-server-for-firmware-management
npm install

# Cài đặt Python dependencies
pip install requests statistics
```

#### 2. Tạo test firmware
```bash
cd esp32-firmware-ci/test
python create_test_firmware.py
```

#### 3. Cấu hình environment variables
```bash
# Tạo file .env trong web-server-for-firmware-management
cp env.example .env

# Chỉnh sửa .env với các token thực tế
API_TOKEN=your_api_token_here
UPLOAD_TOKEN=your_upload_token_here
```

### 🚀 Chạy kiểm thử

#### Bước 1: Khởi động server
```bash
cd web-server-for-firmware-management
npm start
```

#### Bước 2: Chạy test suite
```bash
cd esp32-firmware-ci/test
python test_ota_scenario.py
```

### 📊 Kịch bản 1: Kiểm thử Quy trình OTA

#### Mục tiêu
- Kiểm thử tính ổn định của quy trình OTA
- Đánh giá khả năng khôi phục tự động
- Đo lường thời gian downtime

#### Các test case
1. **Upload Firmware**: Upload firmware mới lên server
2. **Check Latest Version**: Kiểm tra version mới nhất
3. **Device Heartbeat**: Simulate device heartbeat
4. **OTA Log Recording**: Ghi log kết quả update

#### Metrics đo được
- Thời gian upload firmware
- Thời gian phát hiện version mới
- Thời gian gửi heartbeat
- Thời gian ghi log

### 🔒 Kịch bản 2: Kiểm thử Hiệu suất và Bảo mật

#### Mục tiêu
- Đánh giá hiệu suất dưới tải
- Kiểm thử bảo mật API
- So sánh với phương pháp truyền thống

#### Các test case
1. **Concurrent Requests**: Test với 10, 25, 50 requests đồng thời
2. **API Authentication**: Test không có token và token sai
3. **Rate Limiting**: Test giới hạn request
4. **Method Comparison**: So sánh với phương pháp truyền thống

#### Metrics đo được
- Thời gian phản hồi trung bình
- Tỷ lệ thành công
- Hiệu suất dưới tải
- Bảo mật API

### 📈 Kết quả mong đợi

#### Hiệu suất
- **Thời gian OTA**: < 10 giây
- **Thời gian phản hồi API**: < 500ms
- **Tỷ lệ thành công**: > 95%

#### Bảo mật
- **Authentication**: Tất cả API protected đều yêu cầu token
- **Rate Limiting**: Hoạt động đúng với giới hạn
- **Input Validation**: Reject invalid input

#### So sánh với phương pháp truyền thống
- **Tiết kiệm thời gian**: > 90%
- **Không cần tiếp cận vật lý**: ✅
- **Monitoring và logging**: ✅

### 📄 Báo cáo kết quả

Sau khi chạy test, script sẽ tạo file `ota_test_report.json` với:

```json
{
  "test_summary": {
    "total_tests": 15,
    "passed": 14,
    "failed": 1,
    "success_rate": 93.3
  },
  "performance_metrics": {
    "average_response_time": 0.25,
    "min_response_time": 0.12,
    "max_response_time": 0.45
  },
  "test_details": [...]
}
```

### 🎯 Demo cho thầy cô

#### 1. Demo OTA Workflow
```bash
# 1. Upload firmware mới
curl -X POST http://localhost:3000/api/firmware/upload \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -F "firmware=@test-firmware.bin" \
  -F "version=1.0.1"

# 2. ESP32 tự động phát hiện và update
# 3. Xem log kết quả
curl http://localhost:3000/api/logs \
  -H "Authorization: Bearer YOUR_TOKEN"
```

#### 2. Demo Performance
```bash
# Chạy test hiệu suất
python test_ota_scenario.py
```

#### 3. Demo Security
```bash
# Test không có token
curl http://localhost:3000/api/firmware/history
# Kết quả: 401 Unauthorized

# Test với token sai
curl http://localhost:3000/api/firmware/history \
  -H "Authorization: Bearer wrong_token"
# Kết quả: 403 Forbidden
```

### 🔧 Troubleshooting

#### Lỗi thường gặp
1. **Server không khởi động**: Kiểm tra port 3000 có bị chiếm không
2. **Test fail**: Kiểm tra environment variables và token
3. **ESP32 không update**: Kiểm tra WiFi và server URL

#### Debug
```bash
# Xem log server
tail -f web-server-for-firmware-management/logs/app.log

# Test API manually
curl http://localhost:3000/health
```

### 📝 Kết luận

Hệ thống ESP32 OTA đã được kiểm thử toàn diện với:
- ✅ Quy trình OTA ổn định
- ✅ Khả năng khôi phục tự động
- ✅ Hiệu suất cao dưới tải
- ✅ Bảo mật tốt
- ✅ Tiết kiệm thời gian đáng kể so với phương pháp truyền thống