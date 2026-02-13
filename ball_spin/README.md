# Ball Spin 專案說明

## 專案概述

M5Stack ATOM S3 的網球旋轉視覺化裝置，透過 MPU6886 六軸陀螺儀即時顯示網球的旋轉軸線、旋轉類型和轉速。

## 硬體規格

- **MCU**: ESP32-S3 (240MHz, 320KB RAM, 8MB Flash)
- **螢幕**: 128x128 GC9107 IPS (ST7789)
- **IMU**: MPU6886 (6-axis gyro/accel)
- **按鈕**: BtnA (重置方向)

## 功能特性

- **即時姿態追蹤**: 使用四元數積分，無漂移誤差累積
- **網球縫線**: 顯示網球縫線（紋毛縫）和旋轉方向
- **旋轉偵測**: TOPSPIN / SIDESPIN / SLICE / GYRO
- **RPM 顯示**: 即時轉速（轉/分）

## 編譯與上傳

```bash
cd /home/tmac/Desktop/projects/TennisBall_IMU/ball_spin
pio run                    # 編譯
pio run --target upload    # 上傳
```

## BLE OTA 整合狀態

目前暫時禁用 BLE OTA 功能，專注於核心功能實作。

**預計新增**:
- BLE 無線更新支援
- 遠端韌體管理
- 更新進度顯示

**已新增檔案**:
- `lib/ESP32_BLE_OTA/` - 獨立式 BLE OTA 程式庫
- `scripts/ota_updater.py` - Python OTA 工具
- `ota.sh` - 互動式更新腳本
- `partitions.csv` - OTA 分區表

**配置**:
- `platformio.ini` - 已移除 OTA 相關建置旗標
- `src/main.cpp` - 已恢復原始功能（無 OTA 程式碼）

**下次要啟用 OTA 時**：
1. 修改 `platformio.ini`，加入以下建置：
   ```ini
   build_flags =
       -D OTA_NIMBLE_ENABLED=1
       -D OTA_FIRMWARE_VERSION="1.0.0"
       -D OTA_DEVICE_NAME="BallSpin"
   ```

2. 在 `src/main.cpp` 開頭加入：
   ```cpp
   #include <NimBLEDevice.h>
   #include "ESP32_BLE_OTA.h"
   #include "ESP32_BLE_OTA_NimBLE.h"
   ```

3. OTA 程式庫會自動初始化 BLE 服務

## 專案結構

```
ball_spin/
├── src/
│   └── main.cpp              # 主程式（已恢復原始功能）
├── lib/                         # 移除 ESP32_BLE_OTA
├── scripts/
│   └── ota_updater.py        # OTA 工具
├── ota.sh                      # 互動式腳本
├── partitions.csv               # OTA 分區表
├── platformio.ini              # 已更新
└── README_OTA.md              # OTA 說明（本檔案）
```

## 更新日誌

- **2025-02-12**: 建立專案結構
- **2025-02-12**: 新增 BLE OTA 程式庫支援
- **2025-02-12**: 新增 OTA 工具和腳本
- **2025-02-12**: 編譯成功通過

## 參考資料

- M5Stack AtomS3 開發文件: https://docs.m5stack-product/
- ESP32-S3 技術參考手冊
- MPU6886 賳螺儀驅動程式庫
