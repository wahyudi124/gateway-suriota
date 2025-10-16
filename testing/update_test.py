#!/usr/bin/env python3
"""
Test script for BLE CRUD Update operations
Tests device and register update functionality
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

class BLEUpdateTester:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        
    async def connect(self):
        print("Scanning for SURIOTA CRUD Service...")
        devices = await BleakScanner.discover()
        device = next((d for d in devices if d.name and "SURIOTA" in d.name), None)
        
        if device:
            print(f"Found device: {device.name} ({device.address})")
            self.client = BleakClient(device.address)
            await self.client.connect()
            await self.client.start_notify(
                "11111111-1111-1111-1111-111111111102", 
                self._notification_handler
            )
            print("Connected successfully!")
            return True
        else:
            print("SURIOTA device not found")
            return False
    
    def _notification_handler(self, sender, data):
        fragment = data.decode('utf-8')
        if fragment == "<END>":
            try:
                response = json.loads(self.response_buffer)
                print(f"Response: {json.dumps(response, indent=2)}")
            except json.JSONDecodeError:
                print(f"Invalid JSON response: {self.response_buffer}")
            self.response_buffer = ""
        else:
            self.response_buffer += fragment
    
    async def send_command(self, command):
        json_str = json.dumps(command, separators=(',', ':'))
        print(f"Sending: {json_str}")
        
        # Send with fragmentation
        chunk_size = 18
        for i in range(0, len(json_str), chunk_size):
            chunk = json_str[i:i+chunk_size]
            await self.client.write_gatt_char(
                "11111111-1111-1111-1111-111111111101", 
                chunk.encode()
            )
            await asyncio.sleep(0.1)
        
        # Send end marker
        await self.client.write_gatt_char(
            "11111111-1111-1111-1111-111111111101", 
            "<END>".encode()
        )
        await asyncio.sleep(2)  # Wait for response

async def test_update_operations():
    tester = BLEUpdateTester()
    
    if not await tester.connect():
        return
    
    print("\n=== Testing Update Operations ===")
    
    # Test 1: Create a device first
    print("\n1. Creating test device...")
    await tester.send_command({
        "op": "create",
        "type": "device",
        "config": {
            "device_name": "Test Device",
            "protocol": "TCP",
            "ip": "192.168.1.100",
            "port": 502,
            "slave_id": 1,
            "timeout": 1000,
            "retry_count": 3,
            "refresh_rate_ms": 1000
        }
    })
    
    # Test 2: List devices to get device ID
    print("\n2. Listing devices to get device ID...")
    await tester.send_command({
        "op": "read",
        "type": "devices"
    })
    
    # Note: In real test, you would capture the device_id from response
    # For this example, we'll use a placeholder
    device_id = "D123ABC"  # Replace with actual device_id from response
    
    # Test 3: Update device
    print(f"\n3. Updating device {device_id}...")
    await tester.send_command({
        "op": "update",
        "type": "device",
        "device_id": device_id,
        "config": {
            "device_name": "Updated Test Device",
            "protocol": "TCP",
            "ip": "192.168.1.200",
            "port": 502,
            "slave_id": 2,
            "timeout": 2000,
            "retry_count": 5,
            "refresh_rate_ms": 500
        }
    })
    
    # Test 4: Create a register
    print(f"\n4. Creating register for device {device_id}...")
    await tester.send_command({
        "op": "create",
        "type": "register",
        "device_id": device_id,
        "config": {
            "address": 40001,
            "register_name": "TEMP_SENSOR",
            "type": "Holding Register",
            "function_code": 3,
            "data_type": "float32",
            "description": "Temperature Sensor",
            "refresh_rate_ms": 1000
        }
    })
    
    # Test 5: Update register
    register_id = "R456DEF"  # Replace with actual register_id from response
    print(f"\n5. Updating register {register_id}...")
    await tester.send_command({
        "op": "update",
        "type": "register",
        "device_id": device_id,
        "register_id": register_id,
        "config": {
            "address": 40001,
            "register_name": "UPDATED_TEMP_SENSOR",
            "type": "Holding Register",
            "function_code": 3,
            "data_type": "int16",
            "description": "Updated Temperature Sensor",
            "refresh_rate_ms": 2000
        }
    })
    
    # Test 6: Read updated device to verify changes
    print(f"\n6. Reading updated device {device_id}...")
    await tester.send_command({
        "op": "read",
        "type": "device",
        "device_id": device_id
    })
    
    print("\n=== Update Tests Completed ===")
    
    if tester.client:
        await tester.client.disconnect()

if __name__ == "__main__":
    asyncio.run(test_update_operations())