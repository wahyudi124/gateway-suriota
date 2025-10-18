#!/usr/bin/env python3
"""
Test script for data streaming functionality
Tests device streaming start/stop operations
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

class StreamingTester:
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

async def test_streaming():
    tester = StreamingTester()
    
    if not await tester.connect():
        return
    
    print("\n=== Testing Data Streaming ===")
    
    # Test 1: List devices to get device ID
    print("\n1. Listing devices to get device ID...")
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
    
    # Test 3: Start streaming for a specific device
    # Replace with actual device ID from step 1
    device_id = "Dec482"  # Use the device ID from your log
    print(f"\n3. Starting streaming for device {device_id}...")
    await tester.send_command({
        "op": "read",
        "type": "data",
        "device_id": device_id
    })
    
    # Test 4: Wait for streaming data
    print("\n4. Waiting for streaming data (30 seconds)...")
    print("You should see streaming data responses now...")
    await asyncio.sleep(30)
    
    # Test 5: Stop streaming
    print("\n5. Stopping streaming...")
    await tester.send_command({
        "op": "read",
        "type": "data",
        "device_id": "stop"
    })
    
    # Test 6: Try streaming with different device ID
    print("\n6. Testing with different device ID (should not match)...")
    await tester.send_command({
        "op": "read",
        "type": "data",
        "device_id": "NONEXISTENT"
    })
    
    await asyncio.sleep(5)
    
    # Test 7: Start streaming again
    print(f"\n7. Restarting streaming for device {device_id}...")
    await tester.send_command({
        "op": "read",
        "type": "data",
        "device_id": device_id
    })
    
    await asyncio.sleep(10)
    
    # Test 8: Final stop
    print("\n8. Final stop streaming...")
    await tester.send_command({
        "op": "read",
        "type": "data",
        "device_id": "stop"
    })
    
    print("\n=== Streaming Tests Completed ===")
    
    if tester.client:
        await tester.client.disconnect()

if __name__ == "__main__":
    asyncio.run(test_streaming())