# YoTB — 智能网球 IMU 运动分析系统

基于 **M5Stack ATOM S3**（ESP32-S3 + MPU6886 六轴 IMU）的嵌入式网球运动分析平台，将一颗 24mm 的微型传感器变成你的私人击球教练。

## 核心亮点

- **四元数姿态追踪** — 无万向锁的 SO(3) 旋转积分，亚毫秒级时间步进
- **实时旋转分类** — 上旋 / 下旋 / 侧旋 / 切削 / 平击 / 混合，六种旋转类型自动识别
- **击球检测** — 加速度阈值触发 + 200ms 防抖 + 100ms 峰值追踪，记录每一拍的 RPM 与 G 力
- **WiFi 直连仪表盘** — AP 模式无需路由器，手机/电脑直接连接，50Hz WebSocket 实时推流
- **嵌入式 Web 应用** — 3D 网球渲染、三轴实时曲线、击球时间轴、CSV 数据导出，全部烧录在 8MB Flash 中
- **航空 HUD 显示** — 俯仰/横滚/G 力指示器，飞行仪表风格的实时姿态显示
- **72 点参数化球缝渲染** — 在 128x128 像素 LCD 上呈现带深度透视的 3D 网球

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
├── imu_logger/          # 200Hz 高频采集 + CSV 日志 + 冲击检测
├── imu_visualizer/      # 航空 HUD 姿态仪（俯仰/横滚/G 力）
├── ball_spin/           # 独立 3D 网球渲染（四元数驱动）
├── ball_spin_webapp/    # WiFi AP + HTTP/WebSocket 实时仪表盘
└── observer/            # Python 观测客户端（规划中）
```

## 四个固件应用

### 1. IMU Logger — 高频数据采集器

以 200Hz 采样率记录原始 IMU 数据，通过串口输出 CSV 格式。当加速度矢量幅值超过 8g 时触发冲击检测，屏幕闪红。按键可暂停/恢复记录。

### 2. IMU Visualizer — 航空姿态仪

飞行仪表风格的实时姿态显示器：人工地平线、俯仰刻度尺、G 力色条（绿 < 1.5g / 黄 1.5-3g / 红 > 3g）、动态十字准星。EMA 低通滤波（α=0.15）平滑噪声，按键零点校准。

### 3. Ball Spin — 3D 网球可视化

在 128×128 屏幕上渲染带球缝的 3D 网球。陀螺仪数据经四元数积分驱动球体旋转，72 点参数化曲线绘制球缝，Phong 高光 + 投影阴影增强立体感。实时显示 RPM、旋转轴、旋转类型。

### 4. Ball Spin WebApp — WiFi 实时仪表盘

**这是 YoTB 的旗舰应用。**

设备以 AP 模式创建热点（SSID: `TennisBall_IMU`，密码: `tennis123`），同时运行 HTTP 服务器（端口 80）和 WebSocket 服务器（端口 81），支持最多 4 台设备同时连接。

**Web 仪表盘功能：**

- 3D 网球实时渲染（Canvas + 四元数驱动球缝旋转）
- 陀螺仪/加速度计三轴滚动曲线（250 样本窗口 ≈ 5 秒）
- 大字体 RPM 显示 + 旋转类型标签
- 击球时间轴 — 按颜色标记旋转类型，点击查看详细数据
- 击球统计 — 总拍数、平均/最大 RPM、最大 G 力
- CSV 一键导出全部采集数据
- 响应式布局，移动端自适应

**WebSocket 数据协议（50Hz JSON）：**

```json
{
  "t": 12345,
  "ax": 0.01, "ay": 0.02, "az": 1.00,
  "gx": 120.5, "gy": -45.2, "gz": 890.1,
  "qw": 0.99, "qx": 0.01, "qy": 0.02, "qz": 0.03,
  "rpm": 150, "spin": "TOPSPIN", "imp": 0
}
```

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

# 烧录其他固件
cd imu_logger && pio run -t upload        # 数据采集
cd imu_visualizer && pio run -t upload    # 姿态仪
cd ball_spin && pio run -t upload         # 3D 网球
```

### 连接仪表盘

1. 烧录 `ball_spin_webapp` 固件
2. 手机/电脑连接 WiFi：`TennisBall_IMU`（密码 `tennis123`）
3. 浏览器打开 `http://192.168.4.1`
4. 开始挥拍，实时查看数据

## 技术栈

| 层级 | 技术 |
|------|------|
| 构建系统 | PlatformIO + Arduino 框架 |
| 核心库 | M5Unified ≥ 0.1.16 |
| 3D 数学 | 四元数积分、参数化曲面、欧拉角 |
| 信号处理 | EMA 低通滤波、阈值检测、峰值追踪 |
| 通信协议 | Serial 115200bps / WiFi AP / HTTP / WebSocket |
| Web 前端 | 原生 Canvas + JavaScript（零依赖） |
| 渲染 | 双缓冲 Sprite（SPI 显示 30-60fps） |

## 应用场景

- **训练分析** — 教练用手机实时监控学员击球数据
- **自主练习** — 球员即时查看 RPM 和旋转类型反馈
- **技术研究** — 记录完整运动数据集用于生物力学研究
- **器材测试** — 通过 IMU 特征对比不同球拍/球的表现
- **性能追踪** — 跟踪旋转一致性和击球力度随时间的变化

## 许可证

MIT
