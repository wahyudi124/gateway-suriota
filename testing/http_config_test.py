#!/usr/bin/env python3
"""
Test script for HTTP Manager configuration
Tests HTTP config update and data sending functionality
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

class HTTPConfigTester:
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

async def test_http_configuration():
    tester = HTTPConfigTester()
    
    if not await tester.connect():
        return
    
    print("\n=== Testing HTTP Configuration ===")
    
    # Test 1: Read current server configuration
    print("\n1. Reading current server configuration...")
    await tester.send_command({
        "op": "read",
        "type": "server_config"
    })
    
    # Test 2: Update server config to use HTTP protocol
    print("\n2. Updating server config to use HTTP protocol...")
    await tester.send_command({
        "op": "update",
        "type": "server_config",
        "config": {
            "communication": {
                "mode": "WIFI",
                "connection_mode": "Automatic",
                "ip_address": "192.168.1.100",
                "mac_address": "00:1A:2B:3C:4D:5E",
                "wifi": {
                    "ssid": "TestNetwork",
                    "password": "TestPassword123"
                }
            },
            "protocol": "http",
            "data_interval": {
                "value": 2000,
                "unit": "ms"
            },
            "mqtt_config": {
                "enabled": false,
                "broker_address": "mqtt.test.com",
                "broker_port": 1883,
                "client_id": "test_client",
                "username": "test_user",
                "password": "test_pass",
                "topic_publish": "test/data",
                "topic_subscribe": "test/control",
                "keep_alive": 60,
                "clean_session": true,
                "use_tls": false
            },
            "http_config": {
                "enabled": true,
                "endpoint_url": "https://httpbin.org/post",
                "method": "POST",
                "headers": {
                    "Authorization": "Bearer test_token_123",
                    "Content-Type": "application/json",
                    "X-Device-ID": "esp32_gateway_001",
                    "X-API-Version": "v1.0"
                },
                "body_format": "json",
                "timeout": 15000,
                "retry": 3
            }
        }
    })
    
    # Test 3: Read updated configuration to verify changes
    print("\n3. Reading updated server configuration...")
    await tester.send_command({
        "op": "read",
        "type": "server_config"
    })
    
    # Test 4: Test with MQTT protocol
    print("\n4. Updating server config to use MQTT protocol...")
    await tester.send_command({
        "op": "update",
        "type": "server_config",
        "config": {
            "communication": {
                "mode": "WIFI",
                "connection_mode": "Automatic",
                "ip_address": "192.168.1.100",
                "mac_address": "00:1A:2B:3C:4D:5E",
                "wifi": {
                    "ssid": "TestNetwork",
                    "password": "TestPassword123"
                }
            },
            "protocol": "mqtt",
            "data_interval": {
                "value": 1000,
                "unit": "ms"
            },
            "mqtt_config": {
                "enabled": true,
                "broker_address": "broker.hivemq.com",
                "broker_port": 1883,
                "client_id": "esp32_test_client",
                "username": "",
                "password": "",
                "topic_publish": "test/esp32/data",
                "topic_subscribe": "test/esp32/control",
                "keep_alive": 60,
                "clean_session": true,
                "use_tls": false
            },
            "http_config": {
                "enabled": false,
                "endpoint_url": "https://httpbin.org/post",
                "method": "POST",
                "headers": {
                    "Authorization": "Bearer disabled_token",
                    "Content-Type": "application/json"
                },
                "body_format": "json",
                "timeout": 10000,
                "retry": 3
            }
        }
    })
    
    # Test 5: Test HTTP config with different methods
    print("\n5. Testing HTTP config with PUT method...")
    await tester.send_command({
        "op": "update",
        "type": "server_config",
        "config": {
            "communication": {
                "mode": "WIFI",
                "connection_mode": "Automatic",
                "ip_address": "192.168.1.100",
                "mac_address": "00:1A:2B:3C:4D:5E",
                "wifi": {
                    "ssid": "TestNetwork",
                    "password": "TestPassword123"
                }
            },
            "protocol": "http",
            "data_interval": {
                "value": 3000,
                "unit": "ms"
            },
            "mqtt_config": {
                "enabled": false,
                "broker_address": "mqtt.test.com",
                "broker_port": 1883,
                "client_id": "test_client",
                "username": "",
                "password": "",
                "topic_publish": "test/data",
                "topic_subscribe": "test/control",
                "keep_alive": 60,
                "clean_session": true,
                "use_tls": false
            },
            "http_config": {
                "enabled": true,
                "endpoint_url": "https://httpbin.org/put",
                "method": "PUT",
                "headers": {
                    "Authorization": "Bearer put_test_token",
                    "Content-Type": "application/json",
                    "X-HTTP-Method": "PUT",
                    "X-Test-Case": "method_testing"
                },
                "body_format": "json",
                "timeout": 20000,
                "retry": 5
            }
        }
    })
    
    print("\n=== HTTP Configuration Tests Completed ===")
    print("Note: Device will restart after configuration updates to apply changes.")
    
    if tester.client:
        await tester.client.disconnect()

if __name__ == "__main__":
    asyncio.run(test_http_configuration())