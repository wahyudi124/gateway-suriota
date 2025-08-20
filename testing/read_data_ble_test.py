import asyncio
import json
from bleak import BleakClient, BleakScanner

class BLEDataReader:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        self.SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
        self.COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
        self.RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
        
    async def connect(self):
        print("Scanning for BLE devices...")
        devices = await BleakScanner.discover()
        device = next((d for d in devices if d.name and "SURIOTA" in d.name), None)
        
        if device:
            print(f"Found device: {device.name} ({device.address})")
            self.client = BleakClient(device.address)
            await self.client.connect()
            await self.client.start_notify(self.RESPONSE_CHAR_UUID, self._notification_handler)
            print("Connected and notifications enabled")
            return True
        else:
            print("Device not found")
            return False
    
    def _notification_handler(self, sender, data):
        fragment = data.decode('utf-8')
        if fragment == "<END>":
            try:
                response = json.loads(self.response_buffer)
                print(f"📨 Response: {json.dumps(response, indent=2)}")
            except:
                print(f"📨 Raw data: {self.response_buffer}")
            self.response_buffer = ""
        else:
            self.response_buffer += fragment
    
    async def send_command(self, command):
        json_str = json.dumps(command, separators=(',', ':'))
        
        # Fragment if needed
        if len(json_str) <= 18:
            await self.client.write_gatt_char(self.COMMAND_CHAR_UUID, json_str.encode())
        else:
            for i in range(0, len(json_str), 18):
                fragment = json_str[i:i+18]
                await self.client.write_gatt_char(self.COMMAND_CHAR_UUID, fragment.encode())
                await asyncio.sleep(0.1)
        
        # Send end marker
        await self.client.write_gatt_char(self.COMMAND_CHAR_UUID, "<END>".encode())
        print(f"📤 Sent: {json_str}")
    
    async def start_streaming(self, device_id):
        command = {
            "op": "read",
            "type": "data",
            "device": device_id
        }
        await self.send_command(command)
    
    async def stop_streaming(self):
        command = {
            "op": "read",
            "type": "data",
            "device": "stop"
        }
        await self.send_command(command)
    
    async def disconnect(self):
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("Disconnected")

async def main():
    reader = BLEDataReader()
    
    if not await reader.connect():
        return
    
    try:
        print("\n=== BLE Data Streaming Test ===")
        
        # Get device ID from user
        device_id = input("Enter device ID to stream (or press Enter for D123ABC): ").strip()
        if not device_id:
            device_id = "D123ABC"
        
        print(f"\n🚀 Starting data streaming for device: {device_id}")
        await reader.start_streaming(device_id)
        
        print("📡 Streaming data... Press Enter to stop")
        input()
        
        print("\n🛑 Stopping data streaming...")
        await reader.stop_streaming()
        
        print("Waiting 2 seconds for final responses...")
        await asyncio.sleep(2)
        
    except KeyboardInterrupt:
        print("\n🛑 Stopping streaming...")
        await reader.stop_streaming()
        await asyncio.sleep(1)
    
    finally:
        await reader.disconnect()

if __name__ == "__main__":
    asyncio.run(main())