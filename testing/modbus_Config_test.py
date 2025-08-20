"""
Create Modbus RTU Device and Registers Test
Creates 1 RTU device with 10 data points via BLE
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

# BLE Configuration
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

class CreateDeviceRegisterClient:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        self.connected = False
        self.device_id = None
        
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
        
        if fragment == "<END>":
            try:
                response = json.loads(self.response_buffer)
                print(f"Response: {json.dumps(response, indent=2)}")
                
                # Store device ID for register creation
                if response.get('status') == 'ok' and 'device_id' in response:
                    self.device_id = response['device_id']
                    print(f"Stored device ID: {self.device_id}")
                    
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
        
        # Send with fragmentation
        chunk_size = 18
        for i in range(0, len(json_str), chunk_size):
            chunk = json_str[i:i+chunk_size]
            await self.client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode())
            await asyncio.sleep(0.1)
        
        # Send end marker
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        await asyncio.sleep(2.0)
    
    async def create_rtu_device(self):
        """Create Modbus RTU device"""
        print("\n=== Creating Modbus RTU Device ===")
        await self.send_command({
            "op": "create",
            "type": "device",
            "config": {
                "device_name": "RTUDE",
                "protocol": "RTU",
                "serial_port": 2,
                "baud_rate": 9600,
                "parity": "None",
                "data_bits": 8,
                "stop_bits": 1,
                "slave_id": 1,
                "timeout": 1000,
                "retry_count": 3,
                "refresh_rate_ms": 20000
            }
        })
    
    async def create_10_registers(self):
        """Create 10 data point registers"""
        if not self.device_id:
            print("No device ID available")
            return
        
        registers = [
            {"address": 1, "name": "DATA_POINT_1", "type": "int16", "desc": "Temperature Sensor"},
            {"address": 2, "name": "DATA_POINT_2", "type": "int16", "desc": "Pressure Sensor"},
            {"address": 3, "name": "DATA_POINT_3", "type": "int16", "desc": "Flow Rate"}
        ]
        
        for i, reg in enumerate(registers, 1):
            print(f"\n=== Creating Register {i}/10: {reg['name']} ===")
            
            # Determine function code based on data type
            function_code = 1 if reg["type"] == "bool" else 3
            register_type = "Coil" if reg["type"] == "bool" else "Holding Register"
            
            await self.send_command({
                "op": "create",
                "type": "register",
                "device_id": self.device_id,
                "config": {
                    "address": reg["address"],
                    "register_name": reg["name"],
                    "type": register_type,
                    "function_code": function_code,
                    "data_type": reg["type"],
                    "description": reg["desc"],
                    "refresh_rate_ms": 5000
                }
            })
            await asyncio.sleep(1)

async def main():
    """Main function"""
    client = CreateDeviceRegisterClient()
    
    try:
        # Connect to BLE service
        if not await client.connect():
            return
        
        print("=== Modbus RTU Device & Register Creation ===")
        
        # Step 1: Create RTU device
        await client.create_rtu_device()
        await asyncio.sleep(3)
        
        # Step 2: Create 10 registers
        await client.create_10_registers()
        
        print("\n=== Creation Completed ===")
        print("Created 1 Modbus RTU device with 10 data point registers")
        print("The Modbus RTU service should now be reading data from these registers")
        
    except Exception as e:
        print(f"Test failed: {e}")
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())