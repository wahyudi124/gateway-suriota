# BLE CRUD API Documentation

Complete guide for implementing BLE-based CRUD operations for device and register management.

## Table of Contents

1. [Overview](#overview)
2. [Required Libraries](#required-libraries)
3. [System Features](#system-features)
4. [BLE Connection Setup](#ble-connection-setup)
5. [Communication Protocol](#communication-protocol)
6. [CRUD Operations](#crud-operations)
7. [Complete Configuration Examples](#complete-configuration-examples)
8. [Implementation Examples](#implementation-examples)
9. [Error Handling](#error-handling)
10. [Best Practices](#best-practices)

## Overview

This BLE CRUD API provides a standardized way to manage Modbus devices and registers through Bluetooth Low Energy communication. The system uses JSON-based commands with automatic fragmentation to handle BLE's 20-byte packet limitation.

### Key Features
- **RESTful-style operations**: CREATE, READ, UPDATE, DELETE
- **Automatic fragmentation**: Handles large JSON payloads
- **Persistent storage**: Flash-based configuration storage
- **Real-time responses**: Immediate feedback via BLE notifications
- **Production-ready**: Error handling, validation, and logging

## Required Libraries

### Arduino IDE Libraries
Install these libraries through Arduino IDE Library Manager:

#### Core Libraries
- **ESP32 Arduino Core** (v2.0.0+) - ESP32 board support
- **ArduinoJson** (v6.21.0+) - JSON parsing and serialization
- **SPIFFS** (Built-in) - Flash file system for configuration storage

#### BLE Libraries
- **ESP32 BLE Arduino** (Built-in) - Bluetooth Low Energy support

#### Network Libraries
- **WiFi** (Built-in) - WiFi connectivity
- **Ethernet** (v2.0.0+) - Ethernet W5500 support
- **PubSubClient** (v2.8.0+) - MQTT client library
- **HTTPClient** (Built-in) - HTTP client for REST APIs

#### Modbus Libraries
- **ModbusMaster** (v2.0.1+) - Modbus RTU master library
- **HardwareSerial** (Built-in) - Serial communication for RTU

#### Time Libraries
- **NTPClient** (v3.2.1+) - Network Time Protocol client
- **Time** (v1.6.1+) - Time management utilities

### Installation Commands
```bash
# Using Arduino CLI
arduino-cli lib install "ArduinoJson@6.21.0"
arduino-cli lib install "PubSubClient@2.8.0"
arduino-cli lib install "ModbusMaster@2.0.1"
arduino-cli lib install "NTPClient@3.2.1"
arduino-cli lib install "Time@1.6.1"
arduino-cli lib install "Ethernet@2.0.0"
```

### Hardware Requirements
- **ESP32 Development Board** (with PSRAM recommended)
- **W5500 Ethernet Module** (for Ethernet connectivity)
- **RS485 Modules** (for Modbus RTU communication)
- **External PSRAM** (optional, for large configurations)

## System Features

### Hardware Platform
- **ESP32 Microcontroller**: Dual-core Xtensa LX6 with WiFi and Bluetooth
- **PSRAM Support**: External PSRAM for large configuration storage
- **Dual Serial Buses**: Independent RTU communication on two serial ports
- **Memory Management**: Automatic PSRAM allocation with internal RAM fallback

### Communication Protocols
- **Modbus TCP**: Ethernet/WiFi-based Modbus communication
- **Modbus RTU**: Dual serial bus support (Bus 1: GPIO 15/16/39, Bus 2: GPIO 17/18/40)
- **BLE Service**: Configuration management via Bluetooth Low Energy
- **MQTT Client**: Real-time data publishing and command subscription
- **HTTP Client**: RESTful API integration for cloud services

### Network Connectivity
- **WiFi Manager**: Automatic connection with credential management
- **Ethernet Support**: Wired network connectivity option
- **Network Auto-switching**: Seamless failover between WiFi and Ethernet
- **Static/DHCP IP**: Configurable IP addressing modes

### Data Management
- **Flash Storage**: Persistent configuration storage in ESP32 flash
- **JSON Configuration**: Human-readable device and register configurations
- **Queue Management**: Asynchronous data processing with FreeRTOS queues
- **Real-time Clock**: NTP synchronization for accurate timestamps
- **Logging System**: Configurable retention and interval logging

### Modbus Capabilities
- **Function Codes**: Support for FC1, FC2, FC3, FC4, FC5, FC16
- **Data Types**: bool, int16, int32, float32, string
- **Register Types**: Coils, Discrete Inputs, Input Registers, Holding Registers
- **Multi-device**: Up to 100 devices per gateway (with PSRAM)
- **Parallel Processing**: Asynchronous RTU bus operations
- **Error Recovery**: Automatic retry with configurable timeouts

### Configuration Features
- **Device Management**: Create, read, update, delete Modbus devices
- **Register Management**: Individual register configuration and monitoring
- **Server Configuration**: Network, protocol, and logging settings
- **Summary Views**: Compact device and register overviews
- **Validation**: Input validation and error reporting

### Real-time Operations
- **FreeRTOS Tasks**: Multi-threaded operation for concurrent processing
- **Configurable Refresh Rates**: Per-device and per-register polling intervals
- **Live Data Streaming**: Real-time data via MQTT or HTTP
- **Command Processing**: Remote control via MQTT subscriptions
- **Status Monitoring**: Device health and communication status

### Security & Reliability
- **Memory Protection**: Safe allocation with cleanup on failures
- **Error Handling**: Comprehensive error reporting and recovery
- **Watchdog Support**: System monitoring and automatic recovery
- **Configuration Backup**: Persistent storage with integrity checks
- **Input Sanitization**: Validation of all configuration parameters

## BLE Connection Setup

### Service Configuration
```
Service UUID:     00001830-0000-1000-8000-00805f9b34fb
Service Name:     "SURIOTA CRUD Service"
```

### Characteristics
```
Command (Write):    11111111-1111-1111-1111-111111111101
Response (Notify):  11111111-1111-1111-1111-111111111102
```

### Connection Flow
1. **Scan** for BLE devices with service name "SURIOTA CRUD Service"
2. **Connect** to the device
3. **Subscribe** to response characteristic notifications
4. **Send commands** to command characteristic
5. **Receive responses** via notifications

## Communication Protocol

### Request Structure
All requests follow this JSON format:
```json
{
  "op": "create|read|update|delete",
  "type": "device|register|devices|registers|devices_summary|registers_summary|server_config",
  "device_id": "D123ABC",  // Required for register operations
  "register_id": "R456DEF", // Required for register updates/deletes
  "config": { ... }         // Required for create/update operations
}
```

### Response Structure
All responses follow this JSON format:
```json
{
  "status": "ok|error",
  "message": "Description for errors",
  "device_id": "D123ABC",     // For create device
  "register_id": "R456DEF",   // For create register
  "data": { ... },            // For read operations
  "devices": [ ... ],         // For list operations
  "registers": [ ... ],       // For register list operations
  "devices_summary": [ ... ], // For device summary
  "registers_summary": [ ... ], // For register summary
  "server_config": { ... }    // For server configuration
}
```

### Fragmentation Protocol
- **Chunk Size**: 18 bytes per fragment
- **End Marker**: `<END>` indicates message completion
- **Automatic**: Both client and server handle fragmentation transparently

## CRUD Operations

### Device Operations

#### 1. Create Device

**Purpose**: Create a new Modbus device configuration

**Request**:
```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "Production Gateway",
    "protocol": "Modbus TCP",
    "ip": "192.168.1.100",
    "port": 502,
    "slave_id": 1,
    "timeout": 1000,
    "retry_count": 3,
    "refresh_rate_ms": 1000
  }
}
```

**Response**:
```json
{
  "status": "ok",
  "device_id": "D7F2A9B"
}
```

#### 2. Read All Devices

**Purpose**: Get list of all device IDs

**Request**:
```json
{
  "op": "read",
  "type": "devices"
}
```

**Response**:
```json
{
  "status": "ok",
  "devices": ["D7F2A9B", "D3E8C1F", "D9A4B2E"]
}
```

#### 3. Read Devices Summary

**Purpose**: Get compact summary of all devices

**Request**:
```json
{
  "op": "read",
  "type": "devices_summary"
}
```

**Response**:
```json
{
  "status": "ok",
  "devices_summary": [
    {
      "device_id": "D7F2A9B",
      "device_name": "Production Gateway",
      "protocol": "Modbus TCP",
      "register_count": 12
    },
    {
      "device_id": "D3E8C1F",
      "device_name": "RTU Device",
      "protocol": "Modbus RTU",
      "register_count": 8
    }
  ]
}
```

#### 4. Read Specific Device

**Purpose**: Get complete device configuration

**Request**:
```json
{
  "op": "read",
  "type": "device",
  "device_id": "D7F2A9B"
}
```

**Response**:
```json
{
  "status": "ok",
  "data": {
    "device_id": "D7F2A9B",
    "device_name": "Production Gateway",
    "protocol": "Modbus TCP",
    "ip": "192.168.1.100",
    "port": 502,
    "slave_id": 1,
    "timeout": 1000,
    "retry_count": 3,
    "refresh_rate_ms": 1000,
    "registers": [
      {
        "register_id": "R8C3F2A",
        "address": 40001,
        "register_name": "TEMP_SENSOR",
        "type": "Holding Register",
        "function_code": 3,
        "data_type": "float32",
        "description": "Temperature Sensor",
        "refresh_rate_ms": 1000
      }
    ]
  }
}
```

#### 5. Delete Device

**Purpose**: Remove device and all its registers

**Request**:
```json
{
  "op": "delete",
  "type": "device",
  "device_id": "D7F2A9B"
}
```

**Response**:
```json
{
  "status": "ok",
  "message": "Device deleted"
}
```

### Register Operations

#### 1. Create Register

**Purpose**: Add a new register to an existing device

**Request**:
```json
{
  "op": "create",
  "type": "register",
  "device_id": "D7F2A9B",
  "config": {
    "address": 40001,
    "register_name": "TEMP_SENSOR",
    "type": "Holding Register",
    "function_code": 3,
    "data_type": "float32",
    "description": "Temperature Sensor",
    "refresh_rate_ms": 1000
  }
}
```

**Response**:
```json
{
  "status": "ok",
  "register_id": "R8C3F2A"
}
```

#### 2. Read Device Registers

**Purpose**: Get all registers for a specific device

**Request**:
```json
{
  "op": "read",
  "type": "registers",
  "device_id": "D7F2A9B"
}
```

**Response**:
```json
{
  "status": "ok",
  "registers": [
    {
      "register_id": "R8C3F2A",
      "address": 40001,
      "register_name": "TEMP_SENSOR",
      "type": "Holding Register",
      "function_code": 3,
      "data_type": "float32",
      "description": "Temperature Sensor",
      "refresh_rate_ms": 1000
    },
    {
      "register_id": "R5B9E4D",
      "address": 40002,
      "register_name": "PRESSURE_SENSOR",
      "type": "Holding Register",
      "function_code": 3,
      "data_type": "int16",
      "description": "Pressure Sensor",
      "refresh_rate_ms": 1000
    }
  ]
}
```

#### 3. Read Registers Summary

**Purpose**: Get compact summary of device registers

**Request**:
```json
{
  "op": "read",
  "type": "registers_summary",
  "device_id": "D7F2A9B"
}
```

**Response**:
```json
{
  "status": "ok",
  "registers_summary": [
    {
      "register_id": "R8C3F2A",
      "register_name": "TEMP_SENSOR",
      "address": 40001,
      "data_type": "float32",
      "description": "Temperature Sensor"
    },
    {
      "register_id": "R5B9E4D",
      "register_name": "PRESSURE_SENSOR",
      "address": 40002,
      "data_type": "int16",
      "description": "Pressure Sensor"
    }
  ]
}
```

#### 4. Delete Register

**Purpose**: Remove a specific register from a device

**Request**:
```json
{
  "op": "delete",
  "type": "register",
  "device_id": "D7F2A9B",
  "register_id": "R8C3F2A"
}
```

**Response**:
```json
{
  "status": "ok",
  "message": "Register deleted"
}
```

### Server Configuration Operations

#### 1. Read Server Configuration

**Request**:
```json
{
  "op": "read",
  "type": "server_config"
}
```

**Response**:
```json
{
  "status": "ok",
  "server_config": {
    "communication": {
      "mode": "WIFI",
      "connection_mode": "Automatic",
      "ip_address": "192.168.1.100",
      "mac_address": "00:1A:2B:3C:4D:5E",
      "wifi": {
        "ssid": "IndustrialNetwork",
        "password": "SecurePassword123"
      }
    },
    "protocol": "mqtt",
    "data_interval": {
      "value": 1000,
      "unit": "ms"
    },
    "mqtt_config": {
      "enabled": true,
      "broker_address": "mqtt.industrial.com",
      "broker_port": 1883,
      "client_id": "gateway_001",
      "username": "gateway_user",
      "password": "mqtt_secure_pass",
      "topic_publish": "industrial/data",
      "topic_subscribe": "industrial/control",
      "keep_alive": 60,
      "clean_session": true,
      "use_tls": true
    },
    "http_config": {
      "enabled": false,
      "endpoint_url": "https://api.industrial.com/data",
      "method": "POST",
      "headers": {
        "Authorization": "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
        "Content-Type": "application/json",
        "X-Device-ID": "gateway_001"
      },
      "body_format": "json",
      "timeout": 10000,
      "retry": 3
    }
  }
}
```

#### 2. Update Server Configuration

**Request**:
```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "communication": {
      "mode": "WIFI",
      "connection_mode": "Manual",
      "ip_address": "192.168.10.100",
      "mac_address": "00:1A:2B:3C:4D:5E",
      "wifi": {
        "ssid": "ProductionNetwork",
        "password": "NewSecurePassword456"
      }
    },
    "protocol": "http",
    "data_interval": {
      "value": 5000,
      "unit": "ms"
    },
    "mqtt_config": {
      "enabled": false,
      "broker_address": "mqtt.backup.com",
      "broker_port": 8883,
      "client_id": "gateway_001_backup",
      "username": "backup_user",
      "password": "backup_pass",
      "topic_publish": "backup/data",
      "topic_subscribe": "backup/control",
      "keep_alive": 120,
      "clean_session": false,
      "use_tls": true
    },
    "http_config": {
      "enabled": true,
      "endpoint_url": "https://api.production.com/v2/data",
      "method": "POST",
      "headers": {
        "Authorization": "Bearer newTokenHere123...",
        "Content-Type": "application/json",
        "X-Gateway-Version": "2.1.0"
      },
      "body_format": "json",
      "timeout": 15000,
      "retry": 5
    }
  }
}
```

**Response**:
```json
{
  "status": "ok",
  "message": "Server configuration updated"
}
```

## Complete Configuration Examples

### Modbus TCP Device Examples

#### Industrial Gateway Configuration
```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "Industrial Gateway 1",
    "protocol": "TCP",
    "ip": "192.168.1.100",
    "port": 502,
    "slave_id": 1,
    "timeout": 2000,
    "retry_count": 3,
    "refresh_rate_ms": 1000
  }
}
```

#### PLC Connection Configuration
```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "Siemens PLC S7-1200",
    "protocol": "TCP",
    "ip": "192.168.10.50",
    "port": 502,
    "slave_id": 1,
    "timeout": 1500,
    "retry_count": 5,
    "refresh_rate_ms": 500
  }
}
```

#### Energy Meter Configuration
```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "Schneider Energy Meter",
    "protocol": "TCP",
    "ip": "192.168.1.200",
    "port": 502,
    "slave_id": 2,
    "timeout": 3000,
    "retry_count": 2,
    "refresh_rate_ms": 2000
  }
}
```

### Modbus RTU Device Examples (Dual Bus Support)

#### RTU Bus 1 Configuration
```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "RTU Device Bus 1",
    "protocol": "RTU",
    "serial_port": 1,
    "baud_rate": 9600,
    "parity": "None",
    "data_bits": 8,
    "stop_bits": 1,
    "slave_id": 1,
    "timeout": 1000,
    "retry_count": 3,
    "refresh_rate_ms": 2000
  }
}
```

#### RTU Bus 2 High-Speed Configuration
```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "High Speed RTU Bus 2",
    "protocol": "RTU",
    "serial_port": 2,
    "baud_rate": 115200,
    "parity": "Even",
    "data_bits": 8,
    "stop_bits": 1,
    "slave_id": 3,
    "timeout": 500,
    "retry_count": 2,
    "refresh_rate_ms": 100
  }
}
```

#### Legacy RTU Bus 1 Configuration
```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "Legacy RTU Device",
    "protocol": "RTU",
    "serial_port": 1,
    "baud_rate": 19200,
    "parity": "Odd",
    "data_bits": 7,
    "stop_bits": 2,
    "slave_id": 5,
    "timeout": 5000,
    "retry_count": 5,
    "refresh_rate_ms": 5000
  }
}
```

### Dual RTU Bus Hardware Configuration

- **Bus 1**: GPIO 15(RX), 16(TX), 39(RTS) - `"serial_port": 1`
- **Bus 2**: GPIO 17(RX), 18(TX), 40(RTS) - `"serial_port": 2`
- **Asynchronous Operation**: Each bus runs in separate task
- **Independent Baud Rates**: Each bus can have different baud rates
- **Parallel Processing**: No blocking between buses

### Register Configuration Examples

#### Temperature Sensor Register
```json
{
  "op": "create",
  "type": "register",
  "device_id": "D7F2A9B",
  "config": {
    "address": 40001,
    "register_name": "TEMP_SENSOR_1",
    "type": "Holding Register",
    "function_code": 3,
    "data_type": "float32",
    "description": "Main Temperature Sensor (°C)",
    "refresh_rate_ms": 1000
  }
}
```

#### Pressure Sensor Register
```json
{
  "op": "create",
  "type": "register",
  "device_id": "D7F2A9B",
  "config": {
    "address": 40002,
    "register_name": "PRESSURE_SENSOR",
    "type": "Holding Register",
    "function_code": 3,
    "data_type": "int16",
    "description": "System Pressure (bar)",
    "refresh_rate_ms": 1000
  }
}
```

#### Digital Input Register
```json
{
  "op": "create",
  "type": "register",
  "device_id": "D7F2A9B",
  "config": {
    "address": 1,
    "register_name": "PUMP_STATUS",
    "type": "Coil",
    "function_code": 1,
    "data_type": "bool",
    "description": "Main Pump Running Status",
    "refresh_rate_ms": 500
  }
}
```

#### Analog Input Register
```json
{
  "op": "create",
  "type": "register",
  "device_id": "D7F2A9B",
  "config": {
    "address": 4,
    "register_name": "FLOW_RATE",
    "type": "Input Register",
    "function_code": 4,
    "data_type": "float32",
    "description": "Water Flow Rate (L/min)",
    "refresh_rate_ms": 2000
  }
}
```

#### Discrete Input Register
```json
{
  "op": "create",
  "type": "register",
  "device_id": "D7F2A9B",
  "config": {
    "address": 2,
    "register_name": "ALARM_STATUS",
    "type": "Discrete Input",
    "function_code": 2,
    "data_type": "bool",
    "description": "System Alarm Status",
    "refresh_rate_ms": 100
  }
}
```

## Implementation Examples

### Python Implementation

```python
import asyncio
import json
from bleak import BleakClient, BleakScanner

class BLECRUDClient:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        
    async def connect(self):
        devices = await BleakScanner.discover()
        device = next((d for d in devices if d.name == "SURIOTA CRUD Service"), None)
        
        if device:
            self.client = BleakClient(device.address)
            await self.client.connect()
            await self.client.start_notify(
                "11111111-1111-1111-1111-111111111102", 
                self._notification_handler
            )
            return True
        return False
    
    def _notification_handler(self, sender, data):
        fragment = data.decode('utf-8')
        if fragment == "<END>":
            response = json.loads(self.response_buffer)
            print(f"Response: {response}")
            self.response_buffer = ""
        else:
            self.response_buffer += fragment
    
    async def send_command(self, command):
        json_str = json.dumps(command, separators=(',', ':'))
        
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
        await asyncio.sleep(1)

# Usage example
async def main():
    client = BLECRUDClient()
    if await client.connect():
        # Create Modbus TCP device
        await client.send_command({
            "op": "create",
            "type": "device",
            "config": {
                "device_name": "Industrial Gateway",
                "protocol": "TCP",
                "ip": "192.168.1.100",
                "port": 502,
                "slave_id": 1,
                "timeout": 2000,
                "retry_count": 3,
                "refresh_rate_ms": 1000
            }
        })
        
        # Create RTU device on Bus 1
        await client.send_command({
            "op": "create",
            "type": "device",
            "config": {
                "device_name": "RTU Device Bus 1",
                "protocol": "RTU",
                "serial_port": 1,
                "baud_rate": 9600,
                "slave_id": 2,
                "timeout": 1000,
                "retry_count": 3,
                "refresh_rate_ms": 2000
            }
        })

if __name__ == "__main__":
    asyncio.run(main())
```

## Protocol Specifications

### Device Protocols
- **TCP**: Modbus TCP over Ethernet/WiFi
- **RTU**: Modbus RTU over dual serial buses

### RTU Bus Configuration
- **Bus 1**: `serial_port: 1` → GPIO 15/16/39
- **Bus 2**: `serial_port: 2` → GPIO 17/18/40
- **Asynchronous Tasks**: Each bus operates independently
- **Dynamic Baud Rates**: Per-device baud rate configuration

### Register Types
- **Holding Register**: Read/Write (Function Code 3/16)
- **Input Register**: Read Only (Function Code 4)
- **Coil**: Read/Write Boolean (Function Code 1/5)
- **Discrete Input**: Read Only Boolean (Function Code 2)

### Data Types
- **int16**: 16-bit signed integer
- **int32**: 32-bit signed integer  
- **float32**: 32-bit floating point
- **bool**: Boolean value
- **string**: String value

## Error Handling

### Common Error Responses
```json
{
  "status": "error",
  "message": "Device not found"
}
```

### Error Types
- **Invalid JSON**: Malformed request
- **Device not found**: Invalid device_id
- **Register not found**: Invalid register_id
- **Validation failed**: Invalid configuration
- **Operation failed**: System error

## Best Practices

### Performance Optimization
1. **Use appropriate refresh rates** (100ms-5000ms)
2. **Batch register operations** when possible
3. **Monitor memory usage** with PSRAM
4. **Use summary APIs** for efficient data retrieval

### Configuration Guidelines
1. **TCP devices**: Use for Ethernet/WiFi connections
2. **RTU devices**: Distribute across both buses for parallel processing
3. **Baud rates**: Match device specifications
4. **Timeouts**: Adjust based on network/serial latency

### Memory Management
1. **Limit devices**: ~100 devices max without PSRAM
2. **Limit registers**: ~200 registers per device max
3. **Use PSRAM**: For large configurations (500+ devices)
4. **Monitor heap**: Regular memory usage checks

### Dual RTU Bus Optimization
1. **Load Balancing**: Distribute devices evenly across buses
2. **Baud Rate Matching**: Group devices with similar baud rates on same bus
3. **Refresh Rate Coordination**: Use appropriate refresh rates per bus
4. **Error Isolation**: Bus failures don't affect other bus operation

This documentation provides comprehensive coverage of the BLE CRUD API with dual RTU bus support, simplified protocol naming, and asynchronous operation capabilities.      "device_name": "Production Gateway",
                "protocol": "Modbus TCP",
                "ip": "192.168.1.100",
                "port": 502,
                "slave_id": 1,
                "timeout": 1000,
                "retry_count": 3,
                "refresh_rate_ms": 1000
            }
        })
        
        await asyncio.sleep(2)
        
        # List all devices
        await client.send_command({
            "op": "read",
            "type": "devices"
        })

if __name__ == "__main__":
    asyncio.run(main())
```

### JavaScript Implementation

```javascript
class BLECRUDClient {
    constructor() {
        this.device = null;
        this.commandChar = null;
        this.responseChar = null;
        this.responseBuffer = "";
    }
    
    async connect() {
        try {
            this.device = await navigator.bluetooth.requestDevice({
                filters: [{ name: "SURIOTA CRUD Service" }],
                optionalServices: ["00001830-0000-1000-8000-00805f9b34fb"]
            });
            
            const server = await this.device.gatt.connect();
            const service = await server.getPrimaryService("00001830-0000-1000-8000-00805f9b34fb");
            
            this.commandChar = await service.getCharacteristic("11111111-1111-1111-1111-111111111101");
            this.responseChar = await service.getCharacteristic("11111111-1111-1111-1111-111111111102");
            
            await this.responseChar.startNotifications();
            this.responseChar.addEventListener('characteristicvaluechanged', this.handleNotification.bind(this));
            
            return true;
        } catch (error) {
            console.error('Connection failed:', error);
            return false;
        }
    }
    
    handleNotification(event) {
        const value = new TextDecoder().decode(event.target.value);
        
        if (value === "<END>") {
            const response = JSON.parse(this.responseBuffer);
            console.log('Response:', response);
            this.responseBuffer = "";
        } else {
            this.responseBuffer += value;
        }
    }
    
    async sendCommand(command) {
        const jsonStr = JSON.stringify(command);
        const encoder = new TextEncoder();
        
        // Send with fragmentation
        const chunkSize = 18;
        for (let i = 0; i < jsonStr.length; i += chunkSize) {
            const chunk = jsonStr.slice(i, i + chunkSize);
            await this.commandChar.writeValue(encoder.encode(chunk));
            await new Promise(resolve => setTimeout(resolve, 100));
        }
        
        // Send end marker
        await this.commandChar.writeValue(encoder.encode("<END>"));
        await new Promise(resolve => setTimeout(resolve, 1000));
    }
}

// Usage example
async function main() {
    const client = new BLECRUDClient();
    if (await client.connect()) {
        // Create device and register
        await client.sendCommand({
            op: "create",
            type: "device",
            config: {
                device_name: "Web Gateway",
                protocol: "Modbus TCP",
                ip: "192.168.1.101",
                port: 502,
                slave_id: 1,
                timeout: 1000,
                retry_count: 3,
                refresh_rate_ms: 1000
            }
        });
    }
}
```

## Data Type Reference

### Supported Data Types
| Type | Description | Size | Range |
|------|-------------|------|-------|
| `bool` | Boolean value | 1 bit | true/false |
| `int16` | 16-bit signed integer | 2 bytes | -32,768 to 32,767 |
| `int32` | 32-bit signed integer | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| `float32` | 32-bit floating point | 4 bytes | ±3.4E±38 (7 digits) |
| `string` | Text string | Variable | UTF-8 encoded |

### Register Types
| Type | Function Code | Access | Description |
|------|---------------|--------|-------------|
| `Coil` | 1 (Read), 5 (Write) | Read/Write | Digital outputs |
| `Discrete Input` | 2 | Read Only | Digital inputs |
| `Input Register` | 4 | Read Only | Analog inputs |
| `Holding Register` | 3 (Read), 16 (Write) | Read/Write | Analog outputs/storage |

### Protocol Parameters
| Parameter | TCP Default | RTU Default | Description |
|-----------|-------------|-------------|-------------|
| `timeout` | 1000ms | 1000ms | Response timeout |
| `retry_count` | 3 | 3 | Number of retries |
| `refresh_rate_ms` | 1000ms | 1000ms | Data polling interval |
| `port` | 502 | N/A | TCP port number |
| `baud_rate` | N/A | 9600 | Serial baud rate |
| `parity` | N/A | "None" | Serial parity (None/Even/Odd) |
| `data_bits` | N/A | 8 | Serial data bits (7/8) |
| `stop_bits` | N/A | 1 | Serial stop bits (1/2) |

## Error Handling

### Error Response Format
```json
{
  "status": "error",
  "message": "Detailed error description"
}
```

### Common Error Messages

#### JSON/Protocol Errors
- `"Invalid command: INVALID_JSON_CHARACTER"` - Malformed JSON syntax
- `"Missing required field: op"` - Missing operation field
- `"Invalid operation: xyz"` - Unsupported operation type
- `"Operation not supported"` - Invalid operation/type combination

#### Device Errors
- `"Device not found"` - Device ID doesn't exist
- `"Invalid device ID: xyz"` - Malformed device ID format
- `"Device creation failed"` - Validation or storage error
- `"Maximum devices (100) reached"` - Device limit exceeded
- `"Missing TCP configuration (ip/port)"` - TCP parameters missing
- `"Missing RTU field: baud_rate"` - RTU parameters missing

#### Register Errors
- `"No registers found"` - Device has no registers
- `"Register not found"` - Register ID doesn't exist
- `"Register address 40001 already exists"` - Duplicate address
- `"Maximum registers (200) reached"` - Register limit exceeded
- `"Invalid register ID format: xyz"` - Malformed register ID

#### Server Configuration Errors
- `"Server configuration update failed"` - Invalid server config
- `"Missing required section: communication"` - Config section missing
- `"Invalid communication configuration"` - Malformed comm config

#### Storage Errors
- `"Storage not initialized"` - Storage system not ready
- `"Failed to save configuration"` - Storage write error
- `"Configuration validation failed"` - Invalid config data

### Error Handling Best Practices

1. **Always check response status**:
```python
response = await get_response()
if response['status'] == 'error':
    print(f"Error: {response['message']}")
    return
```

2. **Implement retry logic**:
```python
max_retries = 3
for attempt in range(max_retries):
    response = await send_command(command)
    if response['status'] == 'ok':
        break
    await asyncio.sleep(1)
```

3. **Validate data before sending**:
```python
def validate_device_config(config):
    required_fields = ['device_name', 'protocol']
    for field in required_fields:
        if field not in config:
            raise ValueError(f"Missing field: {field}")
```

## Best Practices

### Connection Management

#### Connection Establishment
```python
class RobustBLEClient:
    async def connect_with_retry(self, max_attempts=3):
        for attempt in range(max_attempts):
            try:
                if await self.connect():
                    return True
                await asyncio.sleep(2 ** attempt)  # Exponential backoff
            except Exception as e:
                print(f"Connection attempt {attempt + 1} failed: {e}")
        return False
    
    async def ensure_connected(self):
        if not self.is_connected():
            return await self.connect_with_retry()
        return True
```

#### Connection Health Monitoring
```python
async def monitor_connection(client):
    while True:
        if not client.is_connected():
            print("Connection lost, attempting reconnection...")
            await client.connect_with_retry()
        await asyncio.sleep(10)  # Check every 10 seconds
```

### Command Sequencing

#### Sequential Command Execution
```python
class CommandQueue:
    def __init__(self, client):
        self.client = client
        self.queue = asyncio.Queue()
        self.processing = False
    
    async def add_command(self, command):
        await self.queue.put(command)
        if not self.processing:
            await self.process_queue()
    
    async def process_queue(self):
        self.processing = True
        while not self.queue.empty():
            command = await self.queue.get()
            await self.client.send_command(command)
            await asyncio.sleep(0.5)  # Delay between commands
        self.processing = False
```

#### Batch Operations
```python
async def create_multiple_registers(client, device_id, registers):
    """Create multiple registers with proper sequencing"""
    results = []
    for register in registers:
        command = {
            "op": "create",
            "type": "register",
            "device_id": device_id,
            "config": register
        }
        result = await client.send_command_with_response(command)
        results.append(result)
        await asyncio.sleep(0.5)  # Prevent overwhelming the device
    return results
```

### Error Recovery

#### Exponential Backoff Implementation
```python
import random

async def send_with_retry(client, command, max_retries=3):
    for attempt in range(max_retries):
        try:
            response = await client.send_command_with_response(command)
            if response['status'] == 'ok':
                return response
            
            # Wait with exponential backoff + jitter
            delay = (2 ** attempt) + random.uniform(0, 1)
            await asyncio.sleep(delay)
            
        except Exception as e:
            if attempt == max_retries - 1:
                raise e
            await asyncio.sleep(2 ** attempt)
    
    raise Exception(f"Command failed after {max_retries} attempts")
```

### Performance Optimization

#### Local Caching Strategy
```python
class CachedBLEClient:
    def __init__(self):
        self.device_cache = {}
        self.register_cache = {}
        self.cache_ttl = 300  # 5 minutes
    
    async def get_devices_cached(self):
        cache_key = 'devices'
        if self.is_cache_valid(cache_key):
            return self.device_cache[cache_key]['data']
        
        devices = await self.get_devices_from_server()
        self.device_cache[cache_key] = {
            'data': devices,
            'timestamp': time.time()
        }
        return devices
    
    def is_cache_valid(self, key):
        if key not in self.device_cache:
            return False
        return time.time() - self.device_cache[key]['timestamp'] < self.cache_ttl
```

#### Efficient Data Retrieval
```python
# Use summary operations when full data isn't needed
async def get_device_overview(client):
    """Get quick overview without full device configs"""
    return await client.send_command({
        "op": "read",
        "type": "devices_summary"
    })

# Batch register creation
async def setup_temperature_monitoring(client, device_id):
    """Setup multiple temperature sensors efficiently"""
    temp_registers = [
        {
            "address": 40001 + i,
            "register_name": f"TEMP_SENSOR_{i+1}",
            "type": "Holding Register",
            "function_code": 3,
            "data_type": "float32",
            "description": f"Temperature Sensor {i+1} (°C)",
            "refresh_rate_ms": 1000
        }
        for i in range(10)  # Create 10 temperature sensors
    ]
    
    return await create_multiple_registers(client, device_id, temp_registers)
```

### Security Considerations

#### Input Validation
```python
def validate_device_id(device_id):
    """Validate device ID format"""
    if not isinstance(device_id, str):
        raise ValueError("Device ID must be a string")
    if len(device_id) != 7:  # D + 6 characters
        raise ValueError("Device ID must be 7 characters long")
    if not device_id.startswith('D'):
        raise ValueError("Device ID must start with 'D'")
    if not device_id[1:].isalnum():
        raise ValueError("Device ID must contain only alphanumeric characters")

def sanitize_config(config):
    """Sanitize configuration data"""
    # Remove any potentially dangerous fields
    dangerous_fields = ['__proto__', 'constructor', 'prototype']
    for field in dangerous_fields:
        config.pop(field, None)
    
    # Validate string lengths
    for key, value in config.items():
        if isinstance(value, str) and len(value) > 1000:
            raise ValueError(f"Field {key} exceeds maximum length")
    
    return config
```

This comprehensive documentation provides everything needed to implement and use the BLE CRUD API effectively, with real-world examples and production-ready code patterns.