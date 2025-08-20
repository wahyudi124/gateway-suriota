"""
Server Configuration Test Client
Tests server configuration read and update operations via BLE
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

# BLE Configuration
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

class ServerConfigClient:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        self.connected = False
        
    async def connect(self):
        """Connect to BLE service"""
        try:
            devices = await BleakScanner.discover()
            device = next((d for d in devices if d.name == SERVICE_NAME), None)
            
            if not device:
                print(f"Service {SERVICE_NAME} not found")
                return False
            
            self.client = BleakClient(device.address)
            await self.client.connect()
            await self.client.start_notify(RESPONSE_CHAR_UUID, self._notification_handler)
            
            self.connected = True
            print(f"Connected to {device.name}")
            return True
            
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    async def disconnect(self):
        """Disconnect from BLE service"""
        if self.client and self.connected:
            await self.client.disconnect()
            self.connected = False
            print("Disconnected")
    
    def _notification_handler(self, sender, data):
        """Handle incoming response fragments"""
        fragment = data.decode('utf-8')
        print(f"Received fragment: '{fragment}'")
        
        if fragment == "<END>":
            print(f"\nComplete response: {self.response_buffer}")
            try:
                response = json.loads(self.response_buffer)
                print(f"Parsed response: {json.dumps(response, indent=2)}")
            except json.JSONDecodeError:
                print(f"Failed to parse response: {self.response_buffer}")
            finally:
                self.response_buffer = ""
        else:
            self.response_buffer += fragment
    
    async def send_command(self, command):
        """Send command with automatic fragmentation"""
        if not self.connected:
            raise RuntimeError("Not connected to BLE service")
        
        json_str = json.dumps(command, separators=(',', ':'))
        print(f"Sending command: {json_str}")
        
        # Send with fragmentation
        chunk_size = 18
        for i in range(0, len(json_str), chunk_size):
            chunk = json_str[i:i+chunk_size]
            print(f"Fragment: '{chunk}'")
            await self.client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode())
            await asyncio.sleep(0.1)
        
        # Send end marker
        print("Sending: <END>")
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        await asyncio.sleep(2.0)
    
    async def read_server_config(self):
        """Read current server configuration"""
        print("\n=== Reading Server Configuration ===")
        await self.send_command({
            "op": "read",
            "type": "server_config"
        })
    
    async def update_wifi_config(self):
        """Update WiFi configuration"""
        print("\n=== Updating WiFi Configuration ===")
        await self.send_command({
            "op": "update",
            "type": "server_config",
            "config": {
                "communication": {
                    "mode": "WIFI",
                    "connection_mode": "Automatic",
                    "ip_address": "192.168.1.150",
                    "mac_address": "00:1A:2B:3C:4D:5E",
                    "wifi": {
                        "ssid": "UpdatedNetwork",
                        "password": "NewPassword123"
                    }
                },
                "protocol": "http",
                "data_interval": {
                    "value": 2000,
                    "unit": "ms"
                },
                "mqtt_config": {
                    "enabled": False,
                    "broker_address": "mqtt.example.com",
                    "broker_port": 1883,
                    "client_id": "device001",
                    "username": "mqtt_user",
                    "password": "mqtt_password",
                    "topic_publish": "device/data",
                    "topic_subscribe": "device/control",
                    "keep_alive": 60,
                    "clean_session": True,
                    "use_tls": False
                },
                "http_config": {
                    "enabled": True,
                    "endpoint_url": "https://api.updated.com/data",
                    "method": "POST",
                    "headers": {
                        "Authorization": "Bearer updated-token",
                        "X-Custom-Header": "updated-value",
                        "Content-Type": "application/json"
                    },
                    "body_format": "json",
                    "timeout": 8000,
                    "retry": 5
                },
                "logging_config": {
                    "logging_ret": "1w",
                    "logging_interval": "10m"
                }
            }
        })
    
    async def update_mqtt_config(self):
        """Update MQTT configuration"""
        print("\n=== Updating MQTT Configuration ===")
        await self.send_command({
            "op": "update",
            "type": "server_config",
            "config": {
                "communication": {
                    "mode": "WIFI",
                    "connection_mode": "Automatic",
                    "ip_address": "192.168.1.200",
                    "mac_address": "00:1A:2B:3C:4D:5E",
                    "wifi": {
                        "ssid": "MQTTNetwork",
                        "password": "MQTTPassword"
                    }
                },
                "protocol": "mqtt",
                "data_interval": {
                    "value": 5000,
                    "unit": "ms"
                },
                "mqtt_config": {
                    "enabled": True,
                    "broker_address": "mqtt.production.com",
                    "broker_port": 8883,
                    "client_id": "production_device",
                    "username": "prod_user",
                    "password": "prod_password",
                    "topic_publish": "production/data",
                    "topic_subscribe": "production/control",
                    "keep_alive": 120,
                    "clean_session": False,
                    "use_tls": True
                },
                "http_config": {
                    "enabled": False,
                    "endpoint_url": "https://api.example.com/data",
                    "method": "POST",
                    "headers": {
                        "Authorization": "Bearer token",
                        "Content-Type": "application/json"
                    },
                    "body_format": "json",
                    "timeout": 5000,
                    "retry": 3
                },
                "logging_config": {
                    "logging_ret": "3m",
                    "logging_interval": "30m"
                }
            }
        })
    
    async def update_logging_config(self):
        """Update logging configuration"""
        print("\n=== Updating Logging Configuration ===")
        await self.send_command({
            "op": "update",
            "type": "server_config",
            "config": {
                "communication": {
                    "mode": "ETH",
                    "connection_mode": "Automatic",
                    "ip_address": "192.168.1.200",
                    "mac_address": "00:1A:2B:3C:4D:5E",
                    "wifi": {
                        "ssid": "MQTTNetwork",
                        "password": "MQTTPassword"
                    }
                },
                "protocol": "mqtt",
                "data_interval": {
                    "value": 5000,
                    "unit": "ms"
                },
                "mqtt_config": {
                    "enabled": True,
                    "broker_address": "demo.thingsboard.io",
                    "broker_port": 1883,
                    "client_id": "a4c45qrvwl0uaugb3b3s",
                    "username": "uspwke410mo6ou04gh56",
                    "password": "v3nzs8889mq1yzf73c33",
                    "topic_publish": "v1/devices/me/telemetry",
                    "topic_subscribe": "pproduction/control",
                    "keep_alive": 120,
                    "clean_session": False,
                    "use_tls": False
                },
                "http_config": {
                    "enabled": False,
                    "endpoint_url": "https://api.example.com/data",
                    "method": "POST",
                    "headers": {
                        "Authorization": "Bearer token",
                        "Content-Type": "application/json"
                    },
                    "body_format": "json",
                    "timeout": 5000,
                    "retry": 3
                },
                "logging_config": {
                    "logging_ret": "1m",
                    "logging_interval": "5m"
                }
            }
        })

async def main():
    """Main test function"""
    client = ServerConfigClient()
    
    try:
        # Connect to BLE service
        if not await client.connect():
            return
        
        print("=== Server Configuration Test ===")
        
        # Test 1: Read current configuration
        await client.read_server_config()
        await asyncio.sleep(3)
        
        # Test 6: Update logging configuration
        await client.update_logging_config()
        await asyncio.sleep(3)
        
        # Test 7: Final read after logging update
        await client.read_server_config()
        
        print("\n=== Server Configuration Test Completed ===")
        
    except Exception as e:
        print(f"Test failed: {e}")
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())