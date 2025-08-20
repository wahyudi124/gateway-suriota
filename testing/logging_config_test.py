"""
Logging Configuration Test Client
Tests logging configuration read and update operations via BLE
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

# BLE Configuration
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

class LoggingConfigClient:
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
    
    async def read_logging_config(self):
        """Read current logging configuration"""
        print("\n=== Reading Logging Configuration ===")
        await self.send_command({
            "op": "read",
            "type": "logging_config"
        })
    
    async def update_logging_1w_5m(self):
        """Update logging to 1 week retention, 5 minute interval"""
        print("\n=== Updating Logging: 1w retention, 5m interval ===")
        await self.send_command({
            "op": "update",
            "type": "logging_config",
            "config": {
                "logging_ret": "1w",
                "logging_interval": "5m"
            }
        })
    
    async def update_logging_1m_10m(self):
        """Update logging to 1 month retention, 10 minute interval"""
        print("\n=== Updating Logging: 1m retention, 10m interval ===")
        await self.send_command({
            "op": "update",
            "type": "logging_config",
            "config": {
                "logging_ret": "1m",
                "logging_interval": "10m"
            }
        })
    
    async def update_logging_3m_30m(self):
        """Update logging to 3 month retention, 30 minute interval"""
        print("\n=== Updating Logging: 3m retention, 30m interval ===")
        await self.send_command({
            "op": "update",
            "type": "logging_config",
            "config": {
                "logging_ret": "3m",
                "logging_interval": "30m"
            }
        })
    
    async def test_invalid_config(self):
        """Test invalid logging configuration"""
        print("\n=== Testing Invalid Logging Configuration ===")
        await self.send_command({
            "op": "update",
            "type": "logging_config",
            "config": {
                "logging_ret": "6m",  # Invalid value
                "logging_interval": "1h"  # Invalid value
            }
        })

async def main():
    """Main test function"""
    client = LoggingConfigClient()
    
    try:
        # Connect to BLE service
        if not await client.connect():
            return
        
        print("=== Logging Configuration Test ===")
        
        # Test 1: Read current logging configuration
        await client.read_logging_config()
        await asyncio.sleep(3)
        
        # Test 2: Update to 1 week retention, 5 minute interval
        await client.update_logging_1w_5m()
        await asyncio.sleep(3)
        
        # Test 3: Read after first update
        await client.read_logging_config()
        await asyncio.sleep(3)
        
        # Test 4: Update to 1 month retention, 10 minute interval
        await client.update_logging_1m_10m()
        await asyncio.sleep(3)
        
        # Test 5: Read after second update
        await client.read_logging_config()
        await asyncio.sleep(3)
        
        # Test 6: Update to 3 month retention, 30 minute interval
        await client.update_logging_3m_30m()
        await asyncio.sleep(3)
        
        # Test 7: Read after third update
        await client.read_logging_config()
        await asyncio.sleep(3)
        
        # Test 8: Test invalid configuration
        await client.test_invalid_config()
        await asyncio.sleep(3)
        
        # Test 9: Final read to confirm no changes from invalid config
        await client.read_logging_config()
        
        print("\n=== Logging Configuration Test Completed ===")
        
    except Exception as e:
        print(f"Test failed: {e}")
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())