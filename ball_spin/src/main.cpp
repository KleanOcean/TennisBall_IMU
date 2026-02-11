/**
 * Tennis Ball Spin Visualizer - M5Stack ATOM S3
 *
 * Renders a 3D tennis ball that rotates in real-time based on
 * gyroscope data. Shows the seam curve, spin axis, and RPM.
 * Uses quaternion integration for drift-free 3D orientation.
 *
 * Screen: 128x128 GC9107 IPS
 * Button: BtnA = reset ball orientation to identity
 */

#include <M5Unified.h>
#include <math.h>

// --- Screen ---
static const int16_t W      = 128;
static const int16_t H      = 128;
static const int16_t CX     = 64;
static const int16_t CY     = 54;   // ball center, above screen center
static const int16_t BALL_R = 34;

// --- Seam curve ---
static const float SEAM_AMP = 0.44f; // ~25 deg wobble amplitude
static const int   SEAM_N   = 72;    // sample points along curve

// --- Colors (RGB565) ---
static const uint16_t COL_BG       = 0x0000;  // black
static const uint16_t COL_BALL     = 0xCE40;  // tennis optic yellow
static const uint16_t COL_BALL_HI  = 0xDF00;  // highlight
static const uint16_t COL_SHADOW   = 0x1082;  // drop shadow
static const uint16_t COL_OUTLINE  = 0x6B4D;  // ball edge
static const uint16_t COL_SEAM     = 0xFFFF;  // white (front seam)
static const uint16_t COL_SEAM_DIM = 0x4208;  // gray  (back seam)
static const uint16_t COL_AXIS     = 0xF800;  // red spin axis
static const uint16_t COL_TEXT     = 0xFFFF;  // white
static const uint16_t COL_DIM      = 0x8410;  // gray text

// --- 3D types ---
struct Vec3 { float x, y, z; };
struct Quat { float w, x, y, z; };

// --- State ---
static Quat  orient  = {1, 0, 0, 0};
static Vec3  seamPts[SEAM_N];
static float filtGx  = 0, filtGy = 0, filtGz = 0;
static float filtRPM = 0;
static uint32_t lastUs = 0;

static M5Canvas canvas(&M5.Display);

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

// ==================== Rendering ====================

static void drawBall() {
    // Drop shadow
    canvas.fillCircle(CX + 3, CY + 3, BALL_R, COL_SHADOW);
    // Ball body
    canvas.fillCircle(CX, CY, BALL_R, COL_BALL);
    // Phong-ish highlight (upper-left)
    canvas.fillCircle(CX - 8, CY - 8, BALL_R * 2 / 3, COL_BALL_HI);
}

static void drawSeam() {
    for (int i = 0; i < SEAM_N; i++) {
        int j = (i + 1) % SEAM_N;
        Vec3 p1 = qrot(orient, seamPts[i]);
        Vec3 p2 = qrot(orient, seamPts[j]);

        int16_t sx1 = CX + (int16_t)(p1.x * BALL_R);
        int16_t sy1 = CY - (int16_t)(p1.y * BALL_R);
        int16_t sx2 = CX + (int16_t)(p2.x * BALL_R);
        int16_t sy2 = CY - (int16_t)(p2.y * BALL_R);

        if (p1.z > 0.05f && p2.z > 0.05f) {
            // Front face: bright white seam
            canvas.drawLine(sx1, sy1, sx2, sy2, COL_SEAM);
        } else if (p1.z > -0.15f && p2.z > -0.15f) {
            // Near-edge: dim seam for depth
            canvas.drawLine(sx1, sy1, sx2, sy2, COL_SEAM_DIM);
        }
    }
}

static void drawSpinAxis() {
    if (filtRPM < 3.0f) return;

    float mag = sqrtf(filtGx * filtGx + filtGy * filtGy + filtGz * filtGz);
    if (mag < 1.0f) return;

    // Spin axis in body frame → rotate to world frame
    Vec3 bodyAxis = {filtGx / mag, filtGy / mag, filtGz / mag};
    Vec3 worldAxis = qrot(orient, bodyAxis);

    float dispLen = (float)(BALL_R + 12);
    int16_t ax1 = CX + (int16_t)(worldAxis.x * dispLen);
    int16_t ay1 = CY - (int16_t)(worldAxis.y * dispLen);
    int16_t ax2 = CX - (int16_t)(worldAxis.x * dispLen);
    int16_t ay2 = CY + (int16_t)(worldAxis.y * dispLen);

    canvas.drawLine(ax1, ay1, ax2, ay2, COL_AXIS);
    canvas.fillCircle(ax1, ay1, 3, COL_AXIS);  // direction dot
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    if (!M5.Imu.isEnabled()) {
        M5.Display.fillScreen(TFT_RED);
        M5.Display.setCursor(0, 0);
        M5.Display.println("IMU FAIL!");
        while (1) { delay(1000); }
    }

    canvas.createSprite(W, H);
    canvas.setSwapBytes(true);

    // Pre-compute seam points on unit sphere
    // The tennis ball seam: latitude wobbles ±SEAM_AMP as longitude sweeps 0..2π
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

void loop() {
    M5.update();

    // Reset orientation
    if (M5.BtnA.wasPressed()) {
        orient = {1, 0, 0, 0};
    }

    // Read IMU
    M5.Imu.update();
    m5::imu_data_t d;
    M5.Imu.getImuData(&d);

    // Delta time
    uint32_t nowUs = micros();
    float dt = (nowUs - lastUs) * 1e-6f;
    if (dt > 0.1f) dt = 0.033f;  // clamp on overflow / first frame
    lastUs = nowUs;

    // Gyro → rad/s (raw, for quaternion integration)
    float gx = d.gyro.x * (M_PI / 180.0f);
    float gy = d.gyro.y * (M_PI / 180.0f);
    float gz = d.gyro.z * (M_PI / 180.0f);

    // Filtered gyro (°/s) for display
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

    // ==================== Render ====================
    canvas.fillSprite(COL_BG);

    drawBall();
    drawSeam();
    canvas.drawCircle(CX, CY, BALL_R, COL_OUTLINE);
    drawSpinAxis();

    // --- Text ---
    char buf[28];

    // RPM (large, top)
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(top_center);
    canvas.setTextColor(COL_TEXT);
    if (filtRPM < 1.0f) {
        canvas.drawString("READY", CX, 0);
    } else {
        snprintf(buf, sizeof(buf), "%d RPM", (int)filtRPM);
        canvas.drawString(buf, CX, 0);
    }

    // Gyro axis values (bottom area)
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(COL_DIM);
    snprintf(buf, sizeof(buf), "%.0f  %.0f  %.0f dps",
             filtGx, filtGy, filtGz);
    canvas.drawString(buf, CX, H - 10);

    // Axis color legend
    canvas.setTextDatum(bottom_left);
    canvas.setTextColor(0xF800);
    canvas.drawString("X", 10, H - 1);
    canvas.setTextColor(0x07E0);
    canvas.drawString("Y", 36, H - 1);
    canvas.setTextColor(0x001F);
    canvas.drawString("Z", 60, H - 1);

    // Spin type (when spinning)
    if (filtRPM > 5.0f) {
        float ax = fabsf(filtGx), ay = fabsf(filtGy), az = fabsf(filtGz);
        const char *label = "SPIN";
        if (ax > ay && ax > az)      label = "TOPSPIN";
        else if (ay > ax && ay > az) label = "SIDESPIN";
        else                         label = "GYRO";

        canvas.setTextColor(COL_TEXT);
        canvas.setTextDatum(bottom_right);
        canvas.drawString(label, W - 4, H - 1);
    }

    canvas.pushSprite(0, 0);
    delay(16);  // ~60 fps for responsive rotation
}
