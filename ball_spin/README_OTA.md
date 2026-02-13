# Ball Spin - BLE OTA Setup

This project now supports wireless firmware updates via BLE OTA.

## Quick Start

### Install Python Dependencies

```bash
pip install bleak
```

### Build Firmware

```bash
cd /home/tmac/Desktop/projects/TennisBall_IMU/ball_spin
pio run
```

### Update via OTA

**Interactive Menu:**
```bash
./ota.sh
```

**Command Line:**
```bash
# Scan for devices
./ota.sh -s

# Quick update (auto-confirm)
./ota.sh -u -y

# Build and update
./ota.sh -b -y
```

## Project Structure

```
ball_spin/
├── lib/
│   └── ESP32_BLE_OTA/       # Standalone OTA library
│       └── src/
│           ├── ESP32_BLE_OTA.h/.cpp        # Core OTA handler
│           ├── ESP32_BLE_OTA_NimBLE.h/.cpp  # NimBLE integration
│           ├── ota_config.h                # Configuration
│           └── ota_logger.h                # Logging
├── scripts/
│   └── ota_updater.py        # Python OTA tool
├── src/
│   └── main.cpp              # Firmware with OTA support
├── ota.sh                    # Interactive OTA script
├── partitions.csv            # OTA partition table
└── platformio.ini            # Updated with OTA config
```

## Features

- **Zero dependencies** - Only Arduino framework + NimBLE
- **Modular logging** - Configurable log levels
- **Safety hook** - `OTA_preUpdateSafetyHook()` called before update
- **BLE status indicator** - "BLE" shown on display when connected
- **Auto-reboot** - Device reboots after successful OTA

## Configuration

Edit `platformio.ini` build_flags:

```ini
build_flags =
    -D OTA_DEVICE_NAME="BallSpin"     # BLE device name
    -D OTA_FIRMWARE_VERSION="1.0.0"    # Firmware version
    -D OTA_LOG_LEVEL=3              # 1=Error, 2=Warn, 3=Info, 4=Debug
```

## BLE Service UUIDs

| Service | UUID |
|---------|-------|
| OTA Service | fb1e4001-54ae-4a28-9f74-dfccb248601d |
| Control | fb1e4002-54ae-4a28-9f74-dfccb248601d |
| Data | fb1e4003-54ae-4a28-9f74-dfccb248601d |
| Status | fb1e4004-54ae-4a28-9f74-dfccb248601d |
| Version | fb1e4005-54ae-4a28-9f74-dfccb248601d |

## Updating Firmware Version

Before releasing, update the version in `platformio.ini`:

```ini
-D OTA_FIRMWARE_VERSION="1.1.0"
```

Or set via environment variable:

```bash
export FIRMWARE_VERSION="1.1.0"
```

## Troubleshooting

**Device not found:**
- Make sure device is powered on
- Try reducing scan distance (move closer)
- Check BLE is enabled on your computer

**Update fails:**
- Check firmware size < 1.875 MB
- Verify partition table is correct
- Check Serial Monitor for error messages

**After failed update:**
- Device automatically rolls back to previous firmware
- Power cycle and try again
