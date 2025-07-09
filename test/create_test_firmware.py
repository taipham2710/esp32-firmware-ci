#!/usr/bin/env python3

import os
import random

def create_test_firmware(filename="test-firmware.bin", size_mb=1):
    """Create a dummy firmware file for testing"""
    
    # Create a dummy firmware file with random data
    size_bytes = size_mb * 1024 * 1024
    
    with open(filename, 'wb') as f:
        # Write some header bytes to make it look like a real firmware
        f.write(b'ESP32_FIRMWARE_TEST')
        
        # Fill the rest with random data
        remaining_size = size_bytes - 18
        f.write(os.urandom(remaining_size))
    
    print(f"✅ Created test firmware: {filename} ({size_mb}MB)")
    return filename

def create_multiple_test_firmwares():
    """Create multiple test firmware files with different versions"""
    
    versions = [
        ("test-firmware-v1.0.0.bin", 0.5),
        ("test-firmware-v1.0.1.bin", 0.8),
        ("test-firmware-v1.0.2.bin", 1.0),
        ("test-firmware-v1.1.0.bin", 1.2),
    ]
    
    for filename, size in versions:
        create_test_firmware(filename, int(size))
    
    print(f"✅ Created {len(versions)} test firmware files")

if __name__ == "__main__":
    print("🔧 Creating test firmware files...")
    create_test_firmware()
    create_multiple_test_firmwares()
    print("🎉 Test firmware creation completed!") 