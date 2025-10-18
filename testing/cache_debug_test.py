#!/usr/bin/env python3
"""
Test script for debugging cache loading issues
Tests device listing and cache functionality
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

class CacheDebugTester:
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

async def test_cache_debug():
    tester = CacheDebugTester()
    
    if not await tester.connect():
        return
    
    print("\n=== Testing Cache Debug ===")
    
    # Test 1: List devices
    print("\n1. Listing devices...")
    await tester.send_command({
        "op": "read",
        "type": "devices"
    })
    
    # Test 2: Get devices summary
    print("\n2. Getting devices summary...")
    await tester.send_command({
        "op": "read",
        "type": "devices_summary"
    })
    
    # Test 3: Try to read a specific device (use the device ID from logs)
    print("\n3. Reading specific device...")
    await tester.send_command({
        "op": "read",
        "type": "device",
        "device_id": "Ddfec4"  # Replace with actual device ID
    })
    
    # Test 4: Create a new device to test cache functionality
    print("\n4. Creating a new test device...")
    await tester.send_command({
        "op": "create",
        "type": "device",
        "config": {
            "device_name": "Cache Test Device",
            "protocol": "RTU",
            "serial_port": 1,
            "baud_rate": 9600,
            "parity": "None",
            "data_bits": 8,
            "stop_bits": 1,
            "slave_id": 10,
            "timeout": 1000,
            "retry_count": 3,
            "refresh_rate_ms": 5000
        }
    })
    
    # Test 5: List devices again to see if new device appears
    print("\n5. Listing devices after creation...")
    await tester.send_command({
        "op": "read",
        "type": "devices"
    })
    
    print("\n=== Cache Debug Tests Completed ===")
    print("Check the ESP32 serial output for detailed debug information.")
    
    if tester.client:
        await tester.client.disconnect()

if __name__ == "__main__":
    asyncio.run(test_cache_debug())