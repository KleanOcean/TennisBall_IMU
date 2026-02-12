/**
 * Tennis Ball Spin Tracker - WebSocket Streaming Firmware
 * M5Stack ATOM S3 (ESP32-S3)
 *
 * Creates a WiFi Access Point and serves a web-based dashboard
 * that visualizes real-time IMU data via WebSocket. Tracks ball
 * orientation with quaternion integration and streams at 50Hz.
 *
 * Screen:  128x128 GC9107 IPS - shows WiFi info, client count, RPM
 * Button:  BtnA short press = reset quaternion, long press 3s = Light Sleep
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
#include "esp_sleep.h"
#include "driver/gpio.h"
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

// --- Sleep mode ---
static bool sleepPending = false;
static uint32_t btnPressStartMs = 0;
static bool btnWasDown = false;
static const uint32_t SLEEP_HOLD_MS = 3000; // 3 seconds to trigger sleep

// --- Gyro bias estimation (auto-calibration when stationary) ---
static float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;

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

// ==================== Light Sleep ====================

void enterLightSleep() {
    // Show "SLEEPING..." on screen
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(0xCE40);
    canvas.setTextDatum(MC_DATUM);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.drawString("SLEEPING", CX, 50);
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(0x6B4D);
    canvas.drawString("Press to wake", CX, 80);
    canvas.pushSprite(0, 0);
    delay(500);

    // Turn off display backlight
    M5.Display.setBrightness(0);

    // Stop WiFi (frees ~80mA)
    wsServer.close();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    // Configure GPIO wakeup on BtnA (GPIO 41 on ATOM S3)
    // BtnA is active LOW (pressed = LOW)
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable(GPIO_NUM_41, GPIO_INTR_LOW_LEVEL);

    // Enter Light Sleep - CPU halts here, RAM preserved
    esp_light_sleep_start();

    // === Execution resumes here after wake ===
}

void playSunriseAnimation() {
    // Sunrise animation: ~1.2 seconds, sun (yellow-green ball) rises from sea
    const int FRAMES = 36;        // 36 frames at ~33ms = ~1.2s
    const int16_t SEA_Y = 90;     // sea surface Y position
    const int16_t SUN_START_Y = SEA_Y + 30;  // sun starts below sea
    const int16_t SUN_END_Y = BALL_CY;       // sun ends at normal ball center
    const int16_t SUN_R_START = 15;
    const int16_t SUN_R_END = BALL_R;

    for (int f = 0; f < FRAMES; f++) {
        float t = (float)f / (float)(FRAMES - 1);  // 0.0 → 1.0

        // Ease-out curve: fast start, gentle arrival
        float ease = 1.0f - (1.0f - t) * (1.0f - t);

        canvas.fillSprite(TFT_BLACK);

        // --- Sky gradient (dark blue → slightly lighter at horizon) ---
        for (int16_t y = 0; y < SEA_Y; y++) {
            float skyT = (float)y / (float)SEA_Y;
            // Blend from deep navy (top) to slightly brighter blue (horizon)
            uint8_t r = (uint8_t)(skyT * 8);
            uint8_t g = (uint8_t)(4 + skyT * 16);
            uint8_t b = (uint8_t)(16 + skyT * 40);
            uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            canvas.drawFastHLine(0, y, W, col);
        }

        // --- Sea (dark teal, below SEA_Y) ---
        for (int16_t y = SEA_Y; y < H; y++) {
            int depth = y - SEA_Y;
            uint16_t col;
            if (depth < 2) col = 0x0597;      // bright sea surface
            else if (depth < 10) col = 0x0293; // mid teal
            else col = 0x0172;                  // deep dark
            canvas.drawFastHLine(0, y, W, col);
        }
        // Sea surface highlight
        canvas.drawFastHLine(0, SEA_Y, W, 0x0597);

        // --- Sun position and size ---
        int16_t sunY = SUN_START_Y + (int16_t)(ease * (SUN_END_Y - SUN_START_Y));
        int16_t sunR = SUN_R_START + (int16_t)(ease * (SUN_R_END - SUN_R_START));

        // --- Light rays / glow (drawn before sun, behind it) ---
        // Only above sea level
        if (sunY < SEA_Y + sunR) {
            for (int ring = 3; ring >= 0; ring--) {
                int16_t glowR = sunR + 8 + ring * 6;
                // Fade out with distance
                uint8_t alpha = (3 - ring) * 2 + 1;  // 7,5,3,1
                uint16_t glowCol = ((alpha) << 11) | ((alpha * 3) << 5) | 0;
                // Only draw the part above sea
                for (int16_t dy = -glowR; dy <= 0; dy++) {
                    int16_t py = sunY + dy;
                    if (py < 0 || py >= SEA_Y) continue;
                    int16_t halfW = (int16_t)sqrtf((float)(glowR * glowR - dy * dy));
                    int16_t x1 = CX - halfW; if (x1 < 0) x1 = 0;
                    int16_t x2 = CX + halfW; if (x2 >= W) x2 = W - 1;
                    canvas.drawFastHLine(x1, py, x2 - x1 + 1, glowCol);
                }
            }
        }

        // --- Sun disc (clipped at sea level — only show part above sea) ---
        // Draw the sun as a filled circle, but only pixels above SEA_Y
        for (int16_t dy = -sunR; dy <= sunR; dy++) {
            int16_t py = sunY + dy;
            if (py < 0 || py >= H) continue;
            int16_t halfW = (int16_t)sqrtf((float)(sunR * sunR - dy * dy));
            int16_t x1 = CX - halfW;
            int16_t x2 = CX + halfW;
            if (x1 < 0) x1 = 0;
            if (x2 >= W) x2 = W - 1;

            if (py < SEA_Y) {
                // Above sea: bright yellow-green sun
                uint16_t sunCol = 0xCE40;  // brand yellow-green
                // Lighter near top of disc for highlight
                if (dy < -sunR / 2) sunCol = 0xDF00;
                canvas.drawFastHLine(x1, py, x2 - x1 + 1, sunCol);
            } else {
                // Below sea: dim reflection (every other pixel)
                for (int16_t x = x1; x <= x2; x++) {
                    if ((x + py) % 3 == 0) {
                        canvas.drawPixel(x, py, 0x4B00);  // dim yellow
                    }
                }
            }
        }

        // --- Reflection shimmer on sea surface ---
        if (sunY < SEA_Y + sunR) {
            int16_t refW = sunR + (int16_t)(t * 10);
            for (int16_t x = CX - refW; x <= CX + refW; x++) {
                if (x < 0 || x >= W) continue;
                if ((x + f) % 3 == 0) {
                    canvas.drawPixel(x, SEA_Y, 0xCE40);
                    if (SEA_Y + 1 < H) canvas.drawPixel(x, SEA_Y + 1, 0x4B00);
                }
            }
        }

        // Increase brightness gradually
        int brightness = 10 + (int)(ease * 70);
        M5.Display.setBrightness(brightness);

        canvas.pushSprite(0, 0);
        delay(33);
    }
}

void wakeFromSleep() {
    // Small delay to debounce button
    delay(200);

    // Start display at low brightness for sunrise effect
    M5.Display.setBrightness(10);

    // Play sunrise animation while WiFi restarts in background
    // Start WiFi first (it takes ~500ms to come up, overlaps with animation)
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    // Play the sunrise animation (~1.2s)
    playSunriseAnimation();

    // By now WiFi should be up — start servers
    httpServer.begin();
    wsServer.begin();
    wsServer.onEvent(onWsEvent);

    // Re-initialize IMU
    M5.Imu.init();

    // Reset timing to avoid huge dt jump
    lastUs = micros();
    lastWsSendMs = millis();

    // Full brightness
    M5.Display.setBrightness(80);

    // Reset client count since all were disconnected
    clientCount = 0;
}

// ==================== Main loop ====================

void loop() {
    M5.update();
    httpServer.handleClient();
    wsServer.loop();

    // --- Button handling: short press = reset quaternion, long press 3s = Light Sleep ---
    {
        uint32_t btnNowMs = millis();
        bool btnDown = M5.BtnA.isPressed();

        if (btnDown && !btnWasDown) {
            // Button just pressed down
            btnPressStartMs = btnNowMs;
            btnWasDown = true;
            sleepPending = false;
        }

        if (btnDown && btnWasDown) {
            uint32_t held = btnNowMs - btnPressStartMs;

            // Show sleep progress on screen while holding (after 1 second)
            if (held >= 1000 && held < SLEEP_HOLD_MS) {
                sleepPending = true;
            }

            // Trigger sleep after 3 seconds
            if (held >= SLEEP_HOLD_MS) {
                enterLightSleep();
                // After waking up, execution continues here
                wakeFromSleep();
                btnWasDown = false;
                sleepPending = false;
                return; // Skip rest of loop() this iteration
            }
        }

        if (!btnDown && btnWasDown) {
            // Button released
            uint32_t held = btnNowMs - btnPressStartMs;
            btnWasDown = false;
            sleepPending = false;

            if (held < 1000) {
                // Short press: reset quaternion (existing behavior)
                orient = {1, 0, 0, 0};
            }
            // If held 1-3s, just cancel - do nothing
        }
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
    float gxRaw = d.gyro.x * (M_PI / 180.0f);
    float gyRaw = d.gyro.y * (M_PI / 180.0f);
    float gzRaw = d.gyro.z * (M_PI / 180.0f);

    // Adaptive gyro bias estimation: when angular velocity is low
    // (ball likely stationary), slowly learn the zero-rate offset.
    float rawMag = sqrtf(gxRaw * gxRaw + gyRaw * gyRaw + gzRaw * gzRaw);
    if (rawMag < 0.15f) {  // < ~8.6 deg/s → likely stationary
        const float biasAlpha = 0.002f;  // very slow adaptation
        gyroBiasX += biasAlpha * (gxRaw - gyroBiasX);
        gyroBiasY += biasAlpha * (gyRaw - gyroBiasY);
        gyroBiasZ += biasAlpha * (gzRaw - gyroBiasZ);
    }

    // Subtract estimated bias
    float gx = gxRaw - gyroBiasX;
    float gy = gyRaw - gyroBiasY;
    float gz = gzRaw - gyroBiasZ;

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
    // Dead zone 0.1 rad/s (~5.7 deg/s) to reject residual gyro drift after bias removal
    float wmag = sqrtf(gx * gx + gy * gy + gz * gz);
    if (wmag > 0.10f) {
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

        // Sea-level rise sleep countdown overlay
        if (sleepPending && btnWasDown) {
            uint32_t held = nowMs - btnPressStartMs;

            // Progress: 0.0 at 1s held → 1.0 at 3s held
            float progress = (float)(held - 1000) / (float)(SLEEP_HOLD_MS - 1000);
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;

            // Sea level rises from bottom (y=127) to top (y=0)
            int16_t seaTop = H - 1 - (int16_t)(progress * (H - 1));

            // Semi-transparent sea fill: dark teal overlay
            // Draw horizontal lines with alternating shading for wave texture
            for (int16_t y = seaTop; y < H; y++) {
                // Deeper = more opaque teal; near surface = brighter
                int depth = y - seaTop;
                uint16_t col;
                if (depth < 3) {
                    col = 0x07FF;  // bright cyan — wave crest
                } else if (depth < 8) {
                    col = 0x0597;  // medium teal
                } else {
                    col = 0x0293;  // deep dark teal
                }
                // Blend: draw every other pixel for semi-transparency
                for (int16_t x = 0; x < W; x++) {
                    if ((x + y) % 2 == 0) {
                        canvas.drawPixel(x, y, col);
                    }
                }
            }

            // Wave crest highlight: thin bright line at the surface
            if (seaTop >= 0 && seaTop < H) {
                canvas.drawFastHLine(0, seaTop, W, 0x07FF);  // cyan line
            }

            // Countdown text floating above the sea level
            int remaining = 3 - (int)(held / 1000);
            if (remaining < 1) remaining = 1;
            int16_t textY = seaTop - 14;
            if (textY < 2) textY = 2;
            char sleepBuf[8];
            snprintf(sleepBuf, sizeof(sleepBuf), "%d", remaining);
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(MC_DATUM);
            canvas.setTextColor(0x07FF);  // cyan
            canvas.drawString(sleepBuf, CX, textY);
        }

        canvas.pushSprite(0, 0);
    }
}
