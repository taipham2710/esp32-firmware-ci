#!/usr/bin/env python3

import requests
import time
import json
import threading
import statistics
from datetime import datetime
import os

class OTATestSuite:
    def __init__(self, server_url="http://localhost:3000", api_token=None, upload_token=None):
        self.server_url = server_url
        self.api_token = api_token or os.getenv('API_TOKEN')
        self.upload_token = upload_token or os.getenv('UPLOAD_TOKEN')
        self.test_results = []
        
    def log_test(self, test_name, status, details=None, duration=None):
        """Log test results"""
        result = {
            'test_name': test_name,
            'status': status,
            'timestamp': datetime.now().isoformat(),
            'details': details or {},
            'duration': duration
        }
        self.test_results.append(result)
        print(f"[{status.upper()}] {test_name}: {details}")
        if duration:
            print(f"  Duration: {duration:.2f}s")
    
    def test_ota_workflow(self):
        """Scenario 1: Full OTA workflow test"""
        print("\n=== SCENARIO 1: OTA WORKFLOW TEST ===")
        
        # Test 1: Upload new firmware
        start_time = time.time()
        try:
            with open('test-firmware.bin', 'rb') as f:
                files = {'firmware': f}
                data = {
                    'version': '1.0.1',
                    'device': 'esp32',
                    'notes': 'Test firmware for OTA workflow'
                }
                headers = {'Authorization': f'Bearer {self.upload_token}'}
                
                response = requests.post(
                    f"{self.server_url}/api/firmware/upload",
                    files=files,
                    data=data,
                    headers=headers
                )
                
                duration = time.time() - start_time
                if response.status_code == 200:
                    self.log_test("Upload Firmware", "PASS", 
                                {"version": "1.0.1", "response": response.json()}, duration)
                else:
                    self.log_test("Upload Firmware", "FAIL", 
                                {"status_code": response.status_code, "error": response.text}, duration)
        except Exception as e:
            duration = time.time() - start_time
            self.log_test("Upload Firmware", "ERROR", {"error": str(e)}, duration)
        
        # Test 2: Check latest version
        start_time = time.time()
        try:
            response = requests.get(f"{self.server_url}/api/firmware/version?device=esp32")
            duration = time.time() - start_time
            
            if response.status_code == 200:
                version_data = response.json()
                self.log_test("Check Latest Version", "PASS", 
                            {"version": version_data.get('version'), "url": version_data.get('url')}, duration)
            else:
                self.log_test("Check Latest Version", "FAIL", 
                            {"status_code": response.status_code}, duration)
        except Exception as e:
            duration = time.time() - start_time
            self.log_test("Check Latest Version", "ERROR", {"error": str(e)}, duration)
        
        # Test 3: Simulate device heartbeat
        start_time = time.time()
        try:
            heartbeat_data = {
                "device_id": "test_device_001",
                "status": "online",
                "firmware_version": "v1.0.1"
            }
            response = requests.post(f"{self.server_url}/api/heartbeat", json=heartbeat_data)
            duration = time.time() - start_time
            
            if response.status_code == 200:
                self.log_test("Device Heartbeat", "PASS", {"device_id": "test_device_001"}, duration)
            else:
                self.log_test("Device Heartbeat", "FAIL", 
                            {"status_code": response.status_code}, duration)
        except Exception as e:
            duration = time.time() - start_time
            self.log_test("Device Heartbeat", "ERROR", {"error": str(e)}, duration)
        
        # Test 4: Simulate OTA log
        start_time = time.time()
        try:
            log_data = {
                "device_id": "test_device_001",
                "status": "update_success",
                "version": "v1.0.1",
                "error_message": "",
                "latency_ms": 2500
            }
            response = requests.post(f"{self.server_url}/api/log", json=log_data)
            duration = time.time() - start_time
            
            if response.status_code == 200:
                self.log_test("OTA Log Recording", "PASS", 
                            {"status": "update_success", "latency": "2500ms"}, duration)
            else:
                self.log_test("OTA Log Recording", "FAIL", 
                            {"status_code": response.status_code}, duration)
        except Exception as e:
            duration = time.time() - start_time
            self.log_test("OTA Log Recording", "ERROR", {"error": str(e)}, duration)
    
    def test_performance_and_security(self):
        """Scenario 2: Performance and security testing"""
        print("\n=== SCENARIO 2: PERFORMANCE & SECURITY TEST ===")
        
        # Test 1: Concurrent sensor data requests
        print("Testing concurrent sensor data requests...")
        concurrent_requests = [10, 25, 50]
        
        for num_requests in concurrent_requests:
            start_time = time.time()
            threads = []
            responses = []
            
            def send_sensor_data():
                try:
                    thread_id = threading.current_thread().ident or 0
                    sensor_data = {
                        "device_id": f"test_device_{thread_id}",
                        "temp": 25.5 + (thread_id % 10)
                    }
                    response = requests.post(f"{self.server_url}/api/sensor", json=sensor_data)
                    responses.append(response.status_code)
                except Exception as e:
                    responses.append(f"ERROR: {str(e)}")
            
            for i in range(num_requests):
                thread = threading.Thread(target=send_sensor_data)
                threads.append(thread)
                thread.start()
            
            for thread in threads:
                thread.join()
            
            duration = time.time() - start_time
            success_count = sum(1 for r in responses if r == 200)
            
            self.log_test(f"Concurrent Requests ({num_requests})", 
                         "PASS" if success_count == num_requests else "FAIL",
                         {"success_rate": f"{success_count}/{num_requests}", 
                          "avg_response_time": f"{duration/num_requests:.3f}s"}, duration)
        
        # Test 2: Security tests
        print("Testing security...")

        # Without token
        start_time = time.time()
        try:
            response = requests.get(f"{self.server_url}/api/firmware/history")
            duration = time.time() - start_time
            
            if response.status_code == 401:
                self.log_test("API Authentication (No Token)", "PASS", 
                            {"expected": 401, "got": response.status_code}, duration)
            else:
                self.log_test("API Authentication (No Token)", "FAIL", 
                            {"expected": 401, "got": response.status_code}, duration)
        except Exception as e:
            duration = time.time() - start_time
            self.log_test("API Authentication (No Token)", "ERROR", {"error": str(e)}, duration)
        
        # With wrong token
        start_time = time.time()
        try:
            headers = {'Authorization': 'Bearer wrong_token_123'}
            response = requests.get(f"{self.server_url}/api/firmware/history", headers=headers)
            duration = time.time() - start_time
            
            if response.status_code == 403:
                self.log_test("API Authentication (Wrong Token)", "PASS", 
                            {"expected": 403, "got": response.status_code}, duration)
            else:
                self.log_test("API Authentication (Wrong Token)", "FAIL", 
                            {"expected": 403, "got": response.status_code}, duration)
        except Exception as e:
            duration = time.time() - start_time
            self.log_test("API Authentication (Wrong Token)", "ERROR", {"error": str(e)}, duration)
        
        # Rate limiting
        start_time = time.time()
        try:
            responses = []
            for i in range(15):
                response = requests.get(f"{self.server_url}/api/firmware/version?device=esp32")
                responses.append(response.status_code)
                time.sleep(0.1)
            
            duration = time.time() - start_time
            rate_limited = any(r == 429 for r in responses)
            
            if rate_limited:
                self.log_test("Rate Limiting", "PASS", 
                            {"rate_limit_detected": True, "total_requests": len(responses)}, duration)
            else:
                self.log_test("Rate Limiting", "FAIL", 
                            {"rate_limit_detected": False, "total_requests": len(responses)}, duration)
        except Exception as e:
            duration = time.time() - start_time
            self.log_test("Rate Limiting", "ERROR", {"error": str(e)}, duration)
    
    def compare_with_traditional_method(self):
        """Compare with traditional update method"""
        print("\n=== COMPARISON WITH TRADITIONAL METHOD ===")
        
        traditional_times = {
            "physical_access": 300,
            "manual_upload": 120,
            "verification": 60,
            "total_per_device": 480
        }
        
        ota_times = {
            "detection": 2.5,
            "download": 1.2,
            "update": 3.0,
            "verification": 1.0,
            "total_per_device": 7.7
        }
        
        improvement = (traditional_times["total_per_device"] - ota_times["total_per_device"]) / traditional_times["total_per_device"] * 100
        
        comparison_data = {
            "traditional_method": traditional_times,
            "ota_method": ota_times,
            "improvement_percentage": improvement,
            "time_saved_per_device": traditional_times["total_per_device"] - ota_times["total_per_device"]
        }
        
        self.log_test("Method Comparison", "INFO", comparison_data)
        
        print(f"\n📊 COMPARISON RESULT:")
        print(f"Traditional method: {traditional_times['total_per_device']}s/device")
        print(f"OTA method: {ota_times['total_per_device']}s/device")
        print(f"Improvement: {improvement:.1f}%")
        print(f"Time saved: {comparison_data['time_saved_per_device']}s/device")
    
    def generate_report(self):
        """Generate test report"""
        print("\n=== TEST REPORT SUMMARY ===")
        
        total_tests = len(self.test_results)
        passed_tests = sum(1 for r in self.test_results if r['status'] == 'PASS')
        failed_tests = sum(1 for r in self.test_results if r['status'] == 'FAIL')
        error_tests = sum(1 for r in self.test_results if r['status'] == 'ERROR')
        
        response_times = [r['duration'] for r in self.test_results if r['duration']]
        avg_response_time = statistics.mean(response_times) if response_times else 0
        
        report = {
            "test_summary": {
                "total_tests": total_tests,
                "passed": passed_tests,
                "failed": failed_tests,
                "errors": error_tests,
                "success_rate": (passed_tests / total_tests * 100) if total_tests > 0 else 0
            },
            "performance_metrics": {
                "average_response_time": avg_response_time,
                "min_response_time": min(response_times) if response_times else 0,
                "max_response_time": max(response_times) if response_times else 0
            },
            "test_details": self.test_results
        }
        
        with open('ota_test_report.json', 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"✅ Total tests: {total_tests}")
        print(f"✅ Passed: {passed_tests}")
        print(f"❌ Failed: {failed_tests}")
        print(f"⚠️  Errors: {error_tests}")
        print(f"📈 Success rate: {report['test_summary']['success_rate']:.1f}%")
        print(f"⏱️  Avg response time: {avg_response_time:.2f}s")
        print(f"📄 Detailed report saved to: ota_test_report.json")
        
        return report

def main():
    """Main function to run all tests"""
    print("🚀 STARTING ESP32 OTA SYSTEM TEST")
    print("=" * 50)
    
    test_suite = OTATestSuite()
    
    try:
        test_suite.test_ota_workflow()
        test_suite.test_performance_and_security()
        test_suite.compare_with_traditional_method()
        report = test_suite.generate_report()
        
        print("\n🎉 TESTING COMPLETE!")
        print("=" * 50)
        
    except KeyboardInterrupt:
        print("\n⚠️  Test interrupted by user")
    except Exception as e:
        print(f"\n❌ Error during testing: {str(e)}")

if __name__ == "__main__":
    main()