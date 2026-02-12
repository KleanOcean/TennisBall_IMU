# YoTB — 智能网球 IMU 运动分析系统

基于 **M5Stack ATOM S3**（ESP32-S3 + MPU6886 六轴 IMU）的嵌入式网球运动分析平台，将一颗 24mm 的微型传感器变成你的私人击球教练。

## 核心亮点

- **四元数姿态追踪** — 无万向锁的 SO(3) 旋转积分，自适应陀螺仪偏置校准 + 静止自动回正
- **实时旋转分类** — 上旋 / 下旋 / 侧旋 / 切削 / 平击 / 混合，六种旋转类型自动识别
- **击球检测** — 加速度 >4g 阈值触发 + 200ms 防抖 + 100ms 峰值追踪，记录每一拍的 RPM 与 G 力
- **WiFi 直连仪表盘** — AP 模式无需路由器，手机/电脑直接连接，50Hz WebSocket 实时推流
- **YoTB 品牌仪表盘** — 深色科技风 UI，测地线球体线框渲染，RPM 弧形仪表，实时图表，击球时间轴
- **原始传感器面板** — 三轴加速度/陀螺仪折线图、G 力仪表、欧拉角水平仪，可折叠展开
- **Observer 观测站** — Python 客户端实时录制数据到 SQLite，旋转轴极坐标分析，历史会话管理
- **低功耗休眠** — 长按 3 秒进入 Light Sleep（0.8mA），海水淹没动画；按键唤醒，日出升起动画
- **72 点参数化球缝渲染** — 在 128x128 像素 LCD 和 Web Canvas 上呈现带深度透视的 3D 网球

## 硬件规格

| 项目 | 规格 |
|------|------|
| MCU | ESP32-S3 双核 Xtensa LX7 @ 240MHz |
| 内存 | 8MB Flash + 8MB PSRAM + 512KB SRAM |
| IMU | MPU6886（加速度 ±16g / 陀螺仪 ±2000°/s） |
| 屏幕 | 128×128 GC9107 IPS 彩色 LCD |
| 通信 | WiFi 802.11 b/g/n、蓝牙 5.0 LE |
| 尺寸 | 24 × 24 × 13 mm |
| 供电 | USB Type-C / 锂电池 |

## 项目结构

```
TennisBall_IMU/
├── ball_spin_webapp/    # 🏆 旗舰应用：WiFi AP + YoTB 仪表盘 + 击球检测 + 休眠模式
│   ├── src/main.cpp     #    固件主程序
│   ├── src/webpage.h    #    嵌入式 Web 仪表盘（PROGMEM ~32KB）
│   └── PRD.md           #    产品需求文档
├── observer/            # 🖥️  Python 观测站：实时录制 + SQLite + 分析仪表盘
│   ├── observer.py      #    WebSocket 客户端 + 主程序入口
│   ├── db.py            #    SQLite 数据库操作
│   ├── dashboard.py     #    aiohttp REST API
│   ├── spin_analysis.py #    旋转轴极坐标计算
│   └── static/dashboard.html  # 分析仪表盘（实时 WS + REST API 混合架构）
├── imu_logger/          # 📊 200Hz 高频采集 + CSV 日志 + 冲击检测
├── imu_visualizer/      # ✈️  航空 HUD 姿态仪（俯仰/横滚/G 力）
└── ball_spin/           # 🎾 独立 3D 网球渲染（四元数驱动）
```

## 旗舰应用：Ball Spin WebApp

设备以 AP 模式创建热点（SSID: `TennisBall_IMU`，密码: `tennis123`），同时运行 HTTP 服务器（端口 80）和 WebSocket 服务器（端口 81）。

### Web 仪表盘功能

- **3D 网球** — 测地线球体（icosphere 二次细分 ~480 边）+ 缝线渲染，线框/实体模式切换
- **RPM 弧形仪表** — 270° 半圆弧，荧光黄绿色填充，实时更新
- **实时 RPM 折线图** — 250 点滚动窗口，渐变填充
- **击球时间轴** — 彩色圆点按旋转类型着色，点击查看详情
- **原始数据面板**（可折叠）:
  - 三轴加速度折线图（ax/ay/az）
  - 三轴陀螺仪折线图（gx/gy/gz）
  - G 力圆弧仪表 + 峰值保持
  - 欧拉角显示 + 水平仪气泡指示器
- **ATOM S3 屏幕** — 3D 网球 + RPM + WiFi 信息 + 击球计数

### 按键操作

| 操作 | 功能 |
|------|------|
| 短按屏幕（<1秒） | 重置四元数姿态 |
| 长按屏幕（3秒） | 进入 Light Sleep 休眠（海水上升动画） |
| 休眠中按屏幕 | 唤醒恢复（日出升起动画） |

### WebSocket 数据协议（50Hz JSON）

```json
{
  "t": 12345,
  "ax": 0.01, "ay": 0.02, "az": 1.00,
  "gx": 120.5, "gy": -45.2, "gz": 890.1,
  "qw": 0.99, "qx": 0.01, "qy": 0.02, "qz": 0.03,
  "rpm": 150, "spin": "TOPSPIN", "imp": 0
}
```

## Observer 观测站

Python 客户端连接设备 WebSocket，将所有数据持久化到 SQLite，并在 `localhost:8080` 启动分析仪表盘。

```bash
cd observer
pip install -r requirements.txt
python observer.py
# 浏览器打开 http://localhost:8080
```

### 功能

- **实时录制** — 50Hz IMU 数据 + 击球事件写入 SQLite
- **混合数据架构** — 仪表盘直连 `ws://192.168.4.1:81` 获取实时数据，同时用 REST API 查询历史会话
- **旋转轴极坐标图** — SVG 可视化每次击球的旋转轴方向
- **训练会话管理** — 自动分段、统计、导出 CSV/JSON
- **自动重连** — 断线 2 秒重试，30 秒以上断线自动创建新会话

## 旋转分类算法

计算各轴占比（`ratio = |axis| / (|gx|+|gy|+|gz|)`），按 60% 主导阈值判断：

| RPM 范围 | X 轴主导 | Y 轴主导 | Z 轴主导 | 多轴混合 |
|----------|---------|---------|---------|---------|
| < 300 | FLAT | FLAT | FLAT | FLAT |
| ≥ 300, gx > 0 | TOPSPIN | — | — | — |
| ≥ 300, gx < 0 | BACKSPIN | — | — | — |
| ≥ 300 | — | SIDE_R / SIDE_L | SLICE | MIXED |

## 快速开始

### 环境准备

1. 安装 [PlatformIO](https://platformio.org/)（VS Code 插件或 CLI）
2. 准备 M5Stack ATOM S3 开发板
3. USB Type-C 数据线

### 编译与烧录

```bash
# 烧录旗舰应用（WiFi 仪表盘）
cd ball_spin_webapp
pio run -t upload

# 启动 Observer 观测站
cd observer
pip install -r requirements.txt
python observer.py
```

### 连接仪表盘

1. 烧录 `ball_spin_webapp` 固件
2. 手机/电脑连接 WiFi：`TennisBall_IMU`（密码 `tennis123`）
3. 浏览器打开 `http://192.168.4.1`
4. 开始挥拍，实时查看数据

## IMU 传感器升级路径

### 当前限制

MPU6886 陀螺仪最大量程 ±2000°/s（≈333 RPM）。职业网球上旋可达 3000+ RPM（18000°/s），远超传感器量程。当前数据在高速旋转时会饱和截断。

### 推荐升级传感器

| 排名 | 传感器 | 陀螺仪 | 加速度计 | 价格 | 优势 |
|------|--------|--------|---------|------|------|
| 🥇 | **ICM-20649** | ±4000°/s | ±30g | ~$15 | 量程最大，Adafruit 有成品模块，I2C 即插即用 |
| 🥈 | **LSM6DSV16X** | ±4000°/s | ±16g | ~$25 | 噪声最低，内置传感器融合，精度最佳 |
| 🥉 | **ICM-42688-P** | ±2000°/s | ±16g | ~$18 | 同量程下零偏最小（±0.5°/s），漂移最少 |

### 传感器对比

| 规格 | MPU6886 (当前) | ICM-20649 | LSM6DSV16X | ICM-42688-P |
|------|---------------|-----------|------------|-------------|
| 陀螺仪量程 | ±2000°/s | **±4000°/s** | **±4000°/s** | ±2000°/s |
| 可测最大 RPM | 333 | **667** | **667** | 333 |
| 加速度量程 | ±16g | **±30g** | ±16g | ±16g |
| 陀螺仪零偏 | ±3-5°/s | ±5°/s | **±1°/s** | **±0.5°/s** |
| 噪声密度 | 4.0 mdps/√Hz | 10 mdps/√Hz | **3.8 mdps/√Hz** | **2.8 mdps/√Hz** |
| 最大采样率 | 1 kHz | 1.125 kHz | **7.68 kHz** | **32 kHz** |

### 关于磁力计（9 轴）

网球旋转时磁力计采样率（~100Hz）跟不上旋转频率（50Hz @3000RPM），数据无意义。**不推荐 9 轴传感器**用于此场景。当前固件的静止自动回正算法已替代磁力计的漂移校正功能。

### 未来关注

| 传感器 | 陀螺仪 | 加速度计 | 状态 |
|--------|--------|---------|------|
| **BMI423** (Bosch) | ±4000°/s | ±32g | 新品，暂无成品模块 |
| **LSM6DSV80X** (ST) | ±4000°/s | ±80g（双加速度计） | 新品，专为高冲击运动设计 |

## 技术栈

| 层级 | 技术 |
|------|------|
| 构建系统 | PlatformIO + Arduino 框架 |
| 核心库 | M5Unified ≥ 0.1.16 |
| 3D 数学 | 四元数积分、参数化曲面、欧拉角、测地线球体细分 |
| 信号处理 | EMA 低通滤波、自适应偏置估计、阈值检测、峰值追踪 |
| 通信协议 | WiFi AP / HTTP / WebSocket 50Hz |
| Web 前端 | 原生 Canvas + JavaScript（零依赖，PROGMEM ~32KB） |
| Observer 后端 | Python asyncio + websockets + aiohttp + SQLite |
| 渲染 | 双缓冲 Sprite（SPI 显示 30fps） |

## 许可证

MIT
