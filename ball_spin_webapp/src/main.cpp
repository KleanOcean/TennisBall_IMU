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
#include <string.h>
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

// --- Seam curve ---
static const float SEAM_AMP = 0.44f;
static const int   SEAM_N   = 72;
static Vec3 seamPts[SEAM_N];

// --- Ball rendering on ATOM screen ---
static const int16_t BALL_CY = 52;   // ball center Y (above screen center)
static const int16_t BALL_R  = 30;   // ball radius
static const uint16_t COL_BALL    = 0xCE40;  // tennis optic yellow
static const uint16_t COL_BALL_HI = 0xDF00;  // highlight
static const uint16_t COL_SEAM    = 0xFFFF;  // white seam
static const uint16_t COL_SEAM_DIM= 0x4208;  // gray back seam

// --- Filtered sensor values ---
static float filtGx  = 0, filtGy = 0, filtGz = 0;
static float filtRPM = 0;

// --- Timing ---
static uint32_t lastUs       = 0;
static uint32_t lastWsSendMs = 0;

// --- WebSocket client tracking ---
static uint8_t clientCount = 0;

// --- Impact detection ---
static const float IMPACT_THRESH = 4.0f;  // g threshold
static const uint32_t IMPACT_COOLDOWN_MS = 200; // debounce
static uint32_t lastImpactMs = 0;
static bool impactFlag = false;  // set true on impact, cleared after WS send

// --- Shot tracking ---
static const int MAX_SHOTS = 50;
struct ShotEvent {
    uint32_t timestamp;
    float peakRPM;
    float peakG;
    float gx, gy, gz;  // gyro at impact for classification
    char spinType[12];
};
static ShotEvent shots[MAX_SHOTS];
static int shotCount = 0;

// --- Peak tracking after impact ---
static bool trackingPeak = false;
static uint32_t peakTrackStartMs = 0;
static float peakRPMval = 0;
static float peakGval = 0;
static float peakGx = 0, peakGy = 0, peakGz = 0;

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

// ==================== Spin classification ====================

static void classifySpin(float gx, float gy, float gz, float rpm, char* out) {
    if (rpm < 5.0f) { strcpy(out, "FLAT"); return; }
    float agx = fabsf(gx), agy = fabsf(gy), agz = fabsf(gz);
    float total = agx + agy + agz;
    if (total < 1.0f) { strcpy(out, "FLAT"); return; }
    float rx = agx / total, ry = agy / total, rz = agz / total;
    if (rx > 0.5f) {
        strcpy(out, gx > 0 ? "TOPSPIN" : "BACKSPIN");
    } else if (ry > 0.5f) {
        strcpy(out, gy > 0 ? "SIDE_R" : "SIDE_L");
    } else if (rz > 0.5f) {
        strcpy(out, "SLICE");
    } else {
        strcpy(out, "MIXED");
    }
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
            if (strcmp((char*)payload, "clear_shots") == 0) {
                shotCount = 0;
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

    // Pre-compute seam points on unit sphere
    for (int i = 0; i < SEAM_N; i++) {
        float t   = 2.0f * M_PI * i / SEAM_N;
        float lat = SEAM_AMP * sinf(2.0f * t);
        seamPts[i] = {
            cosf(lat) * cosf(t),
            cosf(lat) * sinf(t),
            sinf(lat)
        };
    }

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

    uint32_t nowMs = millis();

    // --- Impact detection ---
    float accelMag = sqrtf(d.accel.x * d.accel.x + d.accel.y * d.accel.y + d.accel.z * d.accel.z);

    if (accelMag > IMPACT_THRESH && (nowMs - lastImpactMs) > IMPACT_COOLDOWN_MS) {
        lastImpactMs = nowMs;
        impactFlag = true;
        trackingPeak = true;
        peakTrackStartMs = nowMs;
        peakRPMval = filtRPM;
        peakGval = accelMag;
        peakGx = filtGx; peakGy = filtGy; peakGz = filtGz;
    }

    // Track peak values for 100ms after impact
    if (trackingPeak) {
        if (filtRPM > peakRPMval) peakRPMval = filtRPM;
        if (accelMag > peakGval) peakGval = accelMag;
        if (fabsf(filtGx) + fabsf(filtGy) + fabsf(filtGz) > fabsf(peakGx) + fabsf(peakGy) + fabsf(peakGz)) {
            peakGx = filtGx; peakGy = filtGy; peakGz = filtGz;
        }

        if (nowMs - peakTrackStartMs > 100) {
            trackingPeak = false;
            // Record shot event
            if (shotCount < MAX_SHOTS) {
                ShotEvent &s = shots[shotCount];
                s.timestamp = lastImpactMs;
                s.peakRPM = peakRPMval;
                s.peakG = peakGval;
                s.gx = peakGx; s.gy = peakGy; s.gz = peakGz;
                classifySpin(peakGx, peakGy, peakGz, peakRPMval, s.spinType);

                // Send shot event via WebSocket
                char shotJson[200];
                snprintf(shotJson, sizeof(shotJson),
                    "{\"event\":\"shot\",\"id\":%d,\"t\":%lu,\"rpm\":%.0f,\"peakG\":%.1f,"
                    "\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,\"type\":\"%s\"}",
                    shotCount, s.timestamp, s.peakRPM, s.peakG,
                    s.gx, s.gy, s.gz, s.spinType);
                wsServer.broadcastTXT(shotJson);
                shotCount++;
            }
        }
    }

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
    if (nowMs - lastWsSendMs >= 20 && clientCount > 0) {
        lastWsSendMs = nowMs;

        char spinLabel[12];
        classifySpin(filtGx, filtGy, filtGz, filtRPM, spinLabel);

        char json[320];
        snprintf(json, sizeof(json),
            "{\"t\":%lu,\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,"
            "\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,"
            "\"qw\":%.4f,\"qx\":%.4f,\"qy\":%.4f,\"qz\":%.4f,"
            "\"rpm\":%.0f,\"spin\":\"%s\",\"imp\":%d}",
            nowMs, d.accel.x, d.accel.y, d.accel.z,
            filtGx, filtGy, filtGz,
            orient.w, orient.x, orient.y, orient.z,
            filtRPM, spinLabel, impactFlag ? 1 : 0);

        if (impactFlag) impactFlag = false;  // clear after sending

        wsServer.broadcastTXT(json);
    }

    // --- Update ATOM S3 screen (~30fps for smooth ball rotation) ---
    static uint32_t lastScreenMs = 0;
    if (nowMs - lastScreenMs >= 33) {
        lastScreenMs = nowMs;

        canvas.fillSprite(TFT_BLACK);
        char buf[32];

        // --- Tennis ball ---
        // Shadow
        canvas.fillCircle(CX + 2, BALL_CY + 2, BALL_R, 0x1082);
        // Body
        canvas.fillCircle(CX, BALL_CY, BALL_R, COL_BALL);
        // Highlight
        canvas.fillCircle(CX - 6, BALL_CY - 6, BALL_R * 2 / 3, COL_BALL_HI);

        // Seam
        for (int i = 0; i < SEAM_N; i++) {
            int j = (i + 1) % SEAM_N;
            Vec3 p1 = qrot(orient, seamPts[i]);
            Vec3 p2 = qrot(orient, seamPts[j]);
            int16_t sx1 = CX + (int16_t)(p1.x * BALL_R);
            int16_t sy1 = BALL_CY - (int16_t)(p1.y * BALL_R);
            int16_t sx2 = CX + (int16_t)(p2.x * BALL_R);
            int16_t sy2 = BALL_CY - (int16_t)(p2.y * BALL_R);
            if (p1.z > 0.05f && p2.z > 0.05f) {
                canvas.drawLine(sx1, sy1, sx2, sy2, COL_SEAM);
            } else if (p1.z > -0.15f && p2.z > -0.15f) {
                canvas.drawLine(sx1, sy1, sx2, sy2, COL_SEAM_DIM);
            }
        }
        // Outline
        canvas.drawCircle(CX, BALL_CY, BALL_R, 0x6B4D);

        // --- RPM (large, top) ---
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(top_center);
        canvas.setTextColor(TFT_WHITE);
        if (filtRPM < 1.0f) {
            canvas.drawString("READY", CX, 0);
        } else {
            snprintf(buf, sizeof(buf), "%d RPM", (int)filtRPM);
            canvas.drawString(buf, CX, 0);
        }

        // --- WiFi info (small, bottom area) ---
        canvas.setFont(&fonts::Font0);
        canvas.setTextDatum(top_left);

        // Connection status dot
        uint16_t dotCol = clientCount > 0 ? TFT_GREEN : 0x4208;
        canvas.fillCircle(4, 90, 3, dotCol);
        canvas.setTextColor(clientCount > 0 ? TFT_GREEN : 0x8410);
        snprintf(buf, sizeof(buf), "%d connected", clientCount);
        canvas.drawString(buf, 10, 87);

        // Shot count
        if (shotCount > 0) {
            canvas.setTextColor(0xFD20);  // orange
            snprintf(buf, sizeof(buf), "%d shots", shotCount);
            canvas.drawString(buf, 70, 87);
        }

        canvas.setTextColor(0x8410);  // dim gray
        canvas.drawString(AP_SSID, 4, 100);
        snprintf(buf, sizeof(buf), "pw: %s", AP_PASS);
        canvas.drawString(buf, 4, 110);
        canvas.setTextColor(TFT_CYAN);
        canvas.drawString(WiFi.softAPIP().toString().c_str(), 4, 120);

        canvas.pushSprite(0, 0);
    }
}
