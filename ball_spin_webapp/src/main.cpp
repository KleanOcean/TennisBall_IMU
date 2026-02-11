/**
 * Tennis Ball Spin Tracker - WebSocket Streaming Firmware
 * M5Stack ATOM S3 (ESP32-S3)
 *
 * Creates a WiFi Access Point and serves a web-based dashboard
 * that visualizes real-time IMU data via WebSocket. Tracks ball
 * orientation with quaternion integration and streams at 50Hz.
 *
 * Screen:  128x128 GC9107 IPS - shows WiFi info, client count, RPM
 * Button:  BtnA = reset quaternion orientation to identity
 * WiFi AP: "TennisBall_IMU" / "tennis123"
 * Web UI:  http://192.168.4.1
 * WS:      ws://192.168.4.1:81
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <math.h>
#include "webpage.h"  // contains: const char index_html[] PROGMEM = R"rawliteral(...)rawliteral";

// --- WiFi AP Config ---
const char* AP_SSID = "TennisBall_IMU";
const char* AP_PASS = "tennis123";

// --- Servers ---
WebServer httpServer(80);
WebSocketsServer wsServer(81);

// --- Screen ---
static const int16_t W  = 128;
static const int16_t H  = 128;
static const int16_t CX = 64;
static const int16_t CY = 64;

static M5Canvas canvas(&M5.Display);

// --- 3D types ---
struct Vec3 { float x, y, z; };
struct Quat { float w, x, y, z; };

// --- Orientation state ---
static Quat orient = {1, 0, 0, 0};

// --- Filtered sensor values ---
static float filtGx  = 0, filtGy = 0, filtGz = 0;
static float filtRPM = 0;

// --- Timing ---
static uint32_t lastUs       = 0;
static uint32_t lastWsSendMs = 0;

// --- WebSocket client tracking ---
static uint8_t clientCount = 0;

// ==================== Quaternion math ====================

static Quat qmul(Quat a, Quat b) {
    return {
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
    };
}

static void qnorm(Quat &q) {
    float len = sqrtf(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    if (len > 0.0001f) {
        float inv = 1.0f / len;
        q.w *= inv; q.x *= inv; q.y *= inv; q.z *= inv;
    }
}

// Optimized quaternion-vector rotation: q * v * q^-1
// Uses the cross-product form (no full quaternion multiply needed)
static Vec3 qrot(Quat q, Vec3 v) {
    float tx = 2.0f * (q.y * v.z - q.z * v.y);
    float ty = 2.0f * (q.z * v.x - q.x * v.z);
    float tz = 2.0f * (q.x * v.y - q.y * v.x);
    return {
        v.x + q.w * tx + (q.y * tz - q.z * ty),
        v.y + q.w * ty + (q.z * tx - q.x * tz),
        v.z + q.w * tz + (q.x * ty - q.y * tx)
    };
}

// ==================== WebSocket event handler ====================

void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            clientCount++;
            break;
        case WStype_DISCONNECTED:
            if (clientCount > 0) clientCount--;
            break;
        case WStype_TEXT:
            // Handle commands from web page
            if (strcmp((char*)payload, "reset") == 0) {
                orient = {1, 0, 0, 0};
            }
            break;
        default:
            break;
    }
}

// ==================== Setup ====================

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    // Check IMU availability
    if (!M5.Imu.isEnabled()) {
        M5.Display.fillScreen(TFT_RED);
        M5.Display.setCursor(0, 0);
        M5.Display.println("IMU FAIL!");
        while (1) { delay(1000); }
    }

    // Double-buffered canvas for flicker-free screen updates
    canvas.createSprite(W, H);
    canvas.setSwapBytes(true);

    // Start WiFi Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    // Default AP IP is 192.168.4.1

    // HTTP server - serve the web dashboard
    httpServer.on("/", HTTP_GET, []() {
        httpServer.send_P(200, "text/html", index_html);
    });
    httpServer.begin();

    // WebSocket server for real-time IMU streaming
    wsServer.begin();
    wsServer.onEvent(onWsEvent);

    lastUs = micros();
}

// ==================== Main loop ====================

void loop() {
    M5.update();
    httpServer.handleClient();
    wsServer.loop();

    // BtnA: reset quaternion to identity
    if (M5.BtnA.wasPressed()) {
        orient = {1, 0, 0, 0};
    }

    // Read IMU data
    // Note: M5Unified@0.1.17 getImuData() returns void, not bool
    M5.Imu.update();
    m5::imu_data_t d;
    M5.Imu.getImuData(&d);

    // Delta time calculation
    uint32_t nowUs = micros();
    float dt = (nowUs - lastUs) * 1e-6f;
    if (dt > 0.1f) dt = 0.033f;  // clamp on overflow / first frame
    lastUs = nowUs;

    // Gyro in rad/s for quaternion integration
    float gx = d.gyro.x * (M_PI / 180.0f);
    float gy = d.gyro.y * (M_PI / 180.0f);
    float gz = d.gyro.z * (M_PI / 180.0f);

    // Filtered gyro (deg/s) for display and streaming
    filtGx += 0.15f * (d.gyro.x - filtGx);
    filtGy += 0.15f * (d.gyro.y - filtGy);
    filtGz += 0.15f * (d.gyro.z - filtGz);

    // RPM (heavily smoothed)
    float rawRPM = sqrtf(d.gyro.x * d.gyro.x +
                         d.gyro.y * d.gyro.y +
                         d.gyro.z * d.gyro.z) / 6.0f;
    filtRPM += 0.08f * (rawRPM - filtRPM);

    // Integrate quaternion from angular velocity
    float wmag = sqrtf(gx * gx + gy * gy + gz * gz);
    if (wmag > 0.01f) {
        float angle = wmag * dt;
        float ha    = angle * 0.5f;
        float sha   = sinf(ha);
        float invW  = 1.0f / wmag;
        Quat dq = {
            cosf(ha),
            gx * invW * sha,
            gy * invW * sha,
            gz * invW * sha
        };
        orient = qmul(orient, dq);
        qnorm(orient);
    }

    // --- Send WebSocket data at 50Hz (every 20ms) ---
    uint32_t nowMs = millis();
    if (nowMs - lastWsSendMs >= 20 && clientCount > 0) {
        lastWsSendMs = nowMs;

        char json[256];
        snprintf(json, sizeof(json),
            "{\"t\":%lu,\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,"
            "\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,"
            "\"qw\":%.4f,\"qx\":%.4f,\"qy\":%.4f,\"qz\":%.4f,"
            "\"rpm\":%.0f}",
            nowMs, d.accel.x, d.accel.y, d.accel.z,
            filtGx, filtGy, filtGz,
            orient.w, orient.x, orient.y, orient.z,
            filtRPM);

        wsServer.broadcastTXT(json);
    }

    // --- Update ATOM S3 screen (every 200ms to save CPU) ---
    static uint32_t lastScreenMs = 0;
    if (nowMs - lastScreenMs >= 200) {
        lastScreenMs = nowMs;

        canvas.fillSprite(TFT_BLACK);
        canvas.setTextColor(TFT_WHITE);
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(top_center);

        // Title
        canvas.drawString("SPIN", CX, 2);

        canvas.setFont(&fonts::Font0);
        canvas.setTextDatum(top_left);

        char buf[32];

        // WiFi SSID
        canvas.setTextColor(TFT_CYAN);
        canvas.drawString("WiFi:", 4, 28);
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString(AP_SSID, 4, 40);

        // IP address
        canvas.setTextColor(TFT_CYAN);
        canvas.drawString("IP:", 4, 56);
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString(WiFi.softAPIP().toString().c_str(), 4, 68);

        // Client count
        canvas.setTextColor(TFT_CYAN);
        canvas.drawString("Clients:", 4, 84);
        canvas.setTextColor(clientCount > 0 ? TFT_GREEN : TFT_DARKGREY);
        snprintf(buf, sizeof(buf), "%d", clientCount);
        canvas.drawString(buf, 60, 84);

        // RPM display at bottom
        canvas.setTextColor(TFT_YELLOW);
        snprintf(buf, sizeof(buf), "%d RPM", (int)filtRPM);
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(bottom_center);
        canvas.drawString(buf, CX, H - 4);

        canvas.pushSprite(0, 0);
    }
}
