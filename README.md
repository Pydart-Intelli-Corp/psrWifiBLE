# ESP32-S3 WiFi & BLE Multi-Function Device

## Overview

This project is a comprehensive IoT solution designed for the ESP32-S3 WROOM-1-U module. It combines WiFi, Bluetooth Low Energy (BLE), SD card storage, and LittleFS functionality to create a versatile data logging and communication device. The system appears to be specifically designed for agricultural applications, particularly milk quality analysis (Lactosure device).

## Hardware Requirements

### Core Components
- **ESP32-S3 WROOM-1-U** module
- **SD Card Slot** with SPI interface
- **LED indicator** (Pin 16)
- **SD Card Detection Pin** (Pin 11)

### Pin Configuration
```
LED Pin:              16
SD Card Detect Pin:   11
SPI HSPI Configuration:
├── SCK Pin:          14
├── MISO Pin:         12
├── MOSI Pin:         13
└── Chip Select:      10

Serial Communication:
└── TX Pin:           43
```

## Features

### 🌐 WiFi Connectivity
- **Network Scanning**: Automatic detection and listing of available WiFi networks
- **Smart Connection**: Connects to strongest available networks with retry mechanism
- **HTTP Client**: Download files from web servers
- **Network Time Protocol (NTP)**: Automatic time synchronization
- **Signal Strength Monitoring**: Real-time RSSI monitoring

### 📶 Bluetooth Low Energy (BLE)
- **Nordic UART Service**: Uses standard UART service UUIDs
- **Bidirectional Communication**: Send and receive binary data
- **Device Naming**: Dynamic device naming with serial number
- **Connection Status Monitoring**: Real-time connection status tracking

### 💾 Dual Storage System
- **SD Card Storage**: External removable storage with hot-swap detection
- **LittleFS**: Internal flash storage for critical data
- **File Management**: Complete CRUD operations on both storage systems
- **Data Migration**: Transfer files between SD card and LittleFS

### 📊 Data Processing
- **CSV File Processing**: Parse and query milk quality data (CLR, Fat, SNF values)
- **Rate Lookup System**: Find pricing based on quality parameters
- **Binary Data Handling**: Process binary sensor data

### 🔧 System Management
- **LED Status Indicators**: Different blink patterns for various states
- **Serial Command Interface**: AT-style command processing
- **Hot-swap SD Card Support**: Automatic detection of card insertion/removal
- **Preferences Storage**: Persistent configuration storage

## Software Dependencies

### Arduino Libraries
```cpp
#include <WiFi.h>           // ESP32 WiFi functionality
#include <HTTPClient.h>     // HTTP client for web requests
#include <LittleFS.h>       // Internal flash file system
#include <BLEDevice.h>      // Bluetooth Low Energy
#include <BLEServer.h>      // BLE server functionality
#include <BLEUtils.h>       // BLE utilities
#include <BLE2902.h>        // BLE descriptor
#include <HardwareSerial.h> // Serial communication
#include <SPI.h>            // SPI interface for SD card
#include <SD.h>             // SD card file system
#include <Preferences.h>    // Non-volatile storage
```

## Installation & Setup

### 1. Hardware Setup
1. Connect the ESP32-S3 WROOM-1-U to your development board
2. Wire the SD card module using the SPI pins specified above
3. Connect the LED to pin 16 with appropriate current limiting resistor
4. Connect the SD card detect switch to pin 11

### 2. Software Setup
1. Install the Arduino IDE with ESP32 board support
2. Install all required libraries listed above
3. Select "ESP32S3 Dev Module" as your board
4. Configure the following board settings:
   ```
   CPU Frequency: 240MHz
   Flash Mode: QIO
   Flash Size: 4MB (or your module's flash size)
   Partition Scheme: Default 4MB with spiffs
   PSRAM: Enabled (if available)
   ```

### 3. Upload the Code
1. Open the `wifi_ble.ino` file in Arduino IDE
2. Select the correct COM port
3. Upload the code to your ESP32-S3

## Configuration

### WiFi Settings
The device automatically scans for available networks. Use serial commands to connect:
```
Scan                    // Scan for available networks
Connect,SSID,PASSWORD   // Connect to specific network
```

### Time Zone Configuration
Modify these constants for your local time zone:
```cpp
const long gmtOffset_sec = 5.5 * 3600;   // GMT+5:30 (India)
const int daylightOffset_sec = 0;        // No daylight saving
```

### BLE Configuration
The device advertises as "Lactosure - Sl.No - XXXX" where XXXX is the device serial number.

## Serial Command Interface

The device uses an AT-style command interface over Serial (115200 baud). All commands follow the pattern:
```
COMMAND[,PARAMETER1,PARAMETER2,...]
```

### System Commands
| Command | Description | Response |
|---------|-------------|----------|
| `AT` | Check device status | `ACK` |
| `RST` | Restart device | `ACK` + restart |
| `IP` | Get IP address | `ACK` + IP address |
| `NTime` | Get network time | `ACK` + formatted time |
| `Version` | Get firmware version | `ACK` + version number |

### WiFi Commands
| Command | Description | Example |
|---------|-------------|---------|
| `Scan` | Scan for networks | `Scan` |
| `Connect,SSID,PASS` | Connect to WiFi | `Connect,MyWiFi,password123` |
| `ConStatus` | Connection status | `ConStatus` |
| `RSSI` | Signal strength | `RSSI` |
| `SendHttpGet,URL` | HTTP GET request | `SendHttpGet,http://api.example.com/data` |

### SD Card Commands
| Command | Description | Example |
|---------|-------------|---------|
| `InitSD` | Initialize SD card | `InitSD` |
| `ListSD` | List SD card files | `ListSD` |
| `Open,SD,filename,mode` | Open file (0=read, 1=write) | `Open,SD,data.txt,0` |
| `Read,SD,filename,start,end` | Read file bytes | `Read,SD,data.txt,1,100` |
| `Write,SD,filename,new,start,end,newline,data` | Write to file | `Write,SD,data.txt,1,1,10,0,Hello` |
| `Close,SD,filename` | Close file | `Close,SD,data.txt` |
| `Delete,SD,filename` | Delete file | `Delete,SD,old_data.txt` |
| `Size,SD,filename` | Get file size | `Size,SD,data.txt` |
| `FormatSD` | Format SD card | `FormatSD` |

### LittleFS Commands
| Command | Description | Example |
|---------|-------------|---------|
| `InitLFS` | Initialize LittleFS | `InitLFS` |
| `ListLFS` | List LittleFS files | `ListLFS` |
| `Open,LFS,filename,mode` | Open file | `Open,LFS,config.txt,0` |
| `Read,LFS,filename,start,end` | Read file bytes | `Read,LFS,config.txt,1,50` |
| `Write,LFS,filename,new,start,end,newline,data` | Write to file | `Write,LFS,config.txt,1,1,5,0,test` |
| `Close,LFS,filename` | Close file | `Close,LFS,config.txt` |
| `Delete,LFS,filename` | Delete file | `Delete,LFS,old_config.txt` |
| `Size,LFS,filename` | Get file size | `Size,LFS,config.txt` |
| `FormatLFS` | Format LittleFS | `FormatLFS` |

### File Transfer Commands
| Command | Description | Example |
|---------|-------------|---------|
| `MOVtoLFS,filename` | Copy SD file to LittleFS | `MOVtoLFS,data.csv` |
| `MOVtoSD,filename` | Copy LittleFS file to SD | `MOVtoSD,config.txt` |

### BLE Commands
| Command | Description | Example |
|---------|-------------|---------|
| `Start_ble` | Start BLE service | `Start_ble` |
| `BLE_Address` | Get BLE MAC address | `BLE_Address` |
| `BLE_Status` | Get connection status | `BLE_Status` |
| `BLE_Read` | Read received data | `BLE_Read` |
| `BLE_Send,type,data` | Send data via BLE | `BLE_Send,1,Hello World` |

### Data Query Commands (Agriculture Specific)
| Command | Description | Example |
|---------|-------------|---------|
| `RateCFS,storage,file,clr,fat,snf` | Get rate by CLR/Fat/SNF | `RateCFS,LFS,cow.csv,32.5,4.2,8.1` |
| `RateF,storage,file,fat` | Get rate by Fat only | `RateF,SD,prices.csv,4.2` |
| `RateC,storage,file,clr` | Get rate by CLR only | `RateC,LFS,rates.csv,32.5` |
| `RateFC,storage,file,fat,clr` | Get rate by Fat/CLR | `RateFC,SD,cow.csv,4.2,32.5` |
| `RateFS,storage,file,fat,snf` | Get rate by Fat/SNF | `RateFS,LFS,buffalo.csv,4.2,8.1` |

### Download Configuration
| Command | Description | Response |
|---------|-------------|----------|
| `Download,SD` | Set download target to SD | `Set-Download,SD` |
| `Download,LFS` | Set download target to LittleFS | `Set-Download,LFS` |

## LED Status Indicators

The device uses different LED blink patterns to indicate system status:

- **Fast Blink (100ms)**: Not connected to WiFi
- **Heartbeat Blink (1500ms)**: Connected to WiFi
- **Solid ON/OFF**: Various file operations

## Error Handling

The system provides comprehensive error reporting:

- `SD,ERROR`: SD card operation failed
- `LFS,ERROR`: LittleFS operation failed
- `WiFi,ERROR`: WiFi connection failed
- `ERROR,No Connection`: BLE not connected
- `Pointer,ERROR`: Invalid file position parameters
- `ERROR,Filename already-exist`: File creation conflict

## Data Format

### CSV Files
The system processes CSV files with milk quality data in the format:
```
CLR,Fat,SNF,Rate
32.5,4.2,8.1,45.50
30.0,3.8,7.9,42.00
```

### Binary Data
BLE communication supports binary data transfer for sensor readings and control commands.

## Use Cases

### Agricultural Applications
- **Milk Quality Testing**: Store and retrieve milk fat, SNF, and CLR data
- **Price Calculation**: Automatic pricing based on quality parameters
- **Data Logging**: Record test results with timestamps
- **Remote Monitoring**: BLE connectivity for mobile apps

### IoT Data Logger
- **Sensor Data Storage**: Log data from multiple sensors
- **Remote Configuration**: Update settings via WiFi
- **Data Synchronization**: Transfer data between local and cloud storage
- **Real-time Monitoring**: Live data streaming via BLE

## Troubleshooting

### Common Issues

1. **SD Card Not Detected**
   - Check physical connections
   - Ensure card is properly formatted (FAT32)
   - Verify SD card detect pin wiring

2. **WiFi Connection Failed**
   - Check SSID and password
   - Verify signal strength with `RSSI` command
   - Restart device with `RST` command

3. **BLE Not Working**
   - Ensure BLE is started with `Start_ble`
   - Check device compatibility (BLE 4.0+ required)
   - Verify no interference from other 2.4GHz devices

4. **File Operation Errors**
   - Check available space with `Size` commands
   - Ensure files are properly closed after operations
   - Verify file permissions and naming conventions

### Memory Considerations
- SD card files can be large (limited by card capacity)
- LittleFS is limited by ESP32-S3 flash size
- Keep file operations chunked for large files (512-byte buffer used)

## Development Notes

### Code Structure
- **Modular Design**: Separate functions for each feature
- **State Management**: Uses global variables for connection states
- **Error Handling**: Comprehensive error checking and reporting
- **Memory Management**: Efficient buffer usage for file operations

### Performance Optimization
- **Chunked File Reading**: 512-byte buffers prevent memory overflow
- **Async Operations**: Non-blocking WiFi and BLE operations
- **Smart Reconnection**: Automatic retry mechanisms with timeouts

## License

This project appears to be proprietary software for agricultural/dairy industry applications. Please respect any copyright or licensing restrictions.

## Version History

- **V-3**: Current version (as indicated in startup message)
- Features comprehensive WiFi, BLE, and dual storage functionality
- Optimized for milk quality testing applications

## Support

For technical support or questions about this implementation:
1. Check the troubleshooting section above
2. Verify all hardware connections
3. Test with simple commands first (AT, IP, Version)
4. Monitor serial output for error messages

---

**Note**: This device is specifically designed for agricultural applications with emphasis on milk quality testing and data management. The command interface is optimized for integration with mobile applications and laboratory equipment.
