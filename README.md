# ESP32 BLE CRUD Manager

Arduino IDE project converted from Toit implementation with FreeRTOS support.

## Features

- ✅ **BLE Communication** with automatic fragmentation (18-byte chunks)
- ✅ **CRUD Operations** for devices and registers
- ✅ **SPIFFS Storage** for persistence
- ✅ **FreeRTOS Tasks** for command processing
- ✅ **Production Ready** with error handling

## Hardware Requirements

- **ESP32** with BLE support
- **Arduino IDE** with ESP32 board package
- **Libraries**: ArduinoJson, ESP32 BLE Arduino

## Installation

1. Install required libraries in Arduino IDE:
   - ArduinoJson (v6.x)
   - ESP32 BLE Arduino (included with ESP32 board package)

2. Open `main.ino` in Arduino IDE

3. Select ESP32 board and upload

## BLE Service Configuration

- **Service UUID**: `00001830-0000-1000-8000-00805f9b34fb`
- **Command Characteristic**: `11111111-1111-1111-1111-111111111101` (Write)
- **Response Characteristic**: `11111111-1111-1111-1111-111111111102` (Notify)
- **Service Name**: `"Toit CRUD Service"`

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   BLE Client    │◄──►│   BLE Manager    │◄──►│  CRUD Handler   │
│   (Python)      │    │  (FreeRTOS Task) │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
                       ┌──────────────────┐    ┌─────────────────┐
                       │  ESP32 BLE       │    │ Config Manager  │
                       │  Hardware        │    │ (SPIFFS)        │
                       └──────────────────┘    └─────────────────┘
```

## FreeRTOS Implementation

- **Command Processing Task**: Handles incoming BLE commands asynchronously
- **Queue-based Communication**: Commands queued for processing
- **Non-blocking Operations**: Main loop remains responsive

## API Compatibility

Maintains full API compatibility with original Toit implementation:
- Same JSON request/response format
- Same BLE UUIDs and characteristics
- Same fragmentation protocol
- Same CRUD operations

## Testing

Use the existing Python test clients from the original project:
- `testing/ble_client.py`
- `testing/create_device_register.py`
- `testing/modbus_test.py`

## Memory Usage

- **SPIFFS**: Used for persistent storage
- **JSON Documents**: Dynamic allocation with size limits
- **Command Buffer**: 2KB reserved for incoming commands
- **FreeRTOS**: Minimal task overhead (4KB stack per task)