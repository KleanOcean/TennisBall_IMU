/**
 * IMU Sport Motion HUD - M5Stack ATOM S3
 *
 * Aviation-style attitude indicator with sports aesthetics.
 * Full-screen artificial horizon + pitch ladder + G-force bar.
 *
 * Screen: 128x128 GC9107 IPS
 * Button: BtnA = zero-calibrate current orientation
 */

#include <M5Unified.h>
#include <math.h>

// --- Screen ---
static const int16_t W  = 128;
static const int16_t H  = 128;
static const int16_t CX = 64;
static const int16_t CY = 64;

// --- Filter ---
static const float ALPHA = 0.15f;

// --- Colors (RGB565) ---
static const uint16_t COL_SKY      = 0x08A6;  // deep navy RGB(10,20,48)
static const uint16_t COL_GROUND   = 0x28C1;  // dark amber RGB(40,24,8)
static const uint16_t COL_HORIZON  = 0x07FF;  // cyan
static const uint16_t COL_PITCH    = 0x7BCF;  // medium gray
static const uint16_t COL_RETICLE  = 0xFFFF;  // white
static const uint16_t COL_LEVEL    = 0x07EC;  // sports green
static const uint16_t COL_NEAR     = 0x05FA;  // teal
static const uint16_t COL_TEXT     = 0xFFFF;  // white
static const uint16_t COL_SHADOW   = 0x0000;  // black
static const uint16_t COL_GBAR_BG  = 0x18C3;  // dark gray
static const uint16_t COL_GBAR_G   = 0x07E0;  // green
static const uint16_t COL_GBAR_Y   = 0xFFE0;  // yellow
static const uint16_t COL_GBAR_R   = 0xF800;  // red

// --- Pitch scale ---
static const float PX_PER_DEG = 1.5f;

// --- G-bar geometry ---
static const int16_t GBAR_X = W - 5;
static const int16_t GBAR_W = 4;
static const float   GBAR_MAX_G = 4.0f;

// --- State ---
static float filtPitch = 0.0f;
static float filtRoll  = 0.0f;
static float offsPitch = 0.0f;
static float offsRoll  = 0.0f;
static float accelMag  = 1.0f;

static M5Canvas canvas(&M5.Display);

static void accelToAngles(float ax, float ay, float az,
                          float &pitch, float &roll) {
    pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    roll  = atan2f( ay, sqrtf(ax * ax + az * az)) * 180.0f / M_PI;
}

// --- Artificial Horizon ---
// Fills sky, then overlays ground polygon below the tilted horizon line.
static void drawHorizon(float pitch, float roll) {
    float rollRad = roll * M_PI / 180.0f;
    float sinR = sinf(rollRad);
    float cosR = cosf(rollRad);
    float hcy  = CY + pitch * PX_PER_DEG;

    // Sky fill
    canvas.fillSprite(COL_SKY);

    // Ground polygon: two triangles covering everything below the horizon
    float ext = 300.0f;
    float lx = CX - ext * cosR;
    float ly = hcy - ext * sinR;
    float rx = CX + ext * cosR;
    float ry = hcy + ext * sinR;

    // Perpendicular "down" offset (into ground)
    float dx = -300.0f * sinR;
    float dy =  300.0f * cosR;

    canvas.fillTriangle(
        (int16_t)lx, (int16_t)ly,
        (int16_t)rx, (int16_t)ry,
        (int16_t)(rx + dx), (int16_t)(ry + dy),
        COL_GROUND);
    canvas.fillTriangle(
        (int16_t)lx, (int16_t)ly,
        (int16_t)(rx + dx), (int16_t)(ry + dy),
        (int16_t)(lx + dx), (int16_t)(ly + dy),
        COL_GROUND);

    // Horizon line (cyan)
    int16_t hx1 = (int16_t)(CX - 100 * cosR);
    int16_t hy1 = (int16_t)(hcy - 100 * sinR);
    int16_t hx2 = (int16_t)(CX + 100 * cosR);
    int16_t hy2 = (int16_t)(hcy + 100 * sinR);
    canvas.drawLine(hx1, hy1, hx2, hy2, COL_HORIZON);
}

// --- Pitch Ladder ---
// Short tick marks at ±10° and ±20° that tilt with the horizon.
static void drawPitchLadder(float pitch, float roll) {
    float rollRad = roll * M_PI / 180.0f;
    float sinR = sinf(rollRad);
    float cosR = cosf(rollRad);
    float hcy  = CY + pitch * PX_PER_DEG;

    static const int marks[] = {-20, -10, 10, 20};
    for (int i = 0; i < 4; i++) {
        float offset = (float)marks[i] * PX_PER_DEG;

        // Mark center, rotated around horizon center
        float mcx = CX  + offset * sinR;
        float mcy = hcy - offset * cosR;

        float halfLen = (marks[i] % 20 == 0) ? 14.0f : 9.0f;

        int16_t x1 = (int16_t)(mcx - halfLen * cosR);
        int16_t y1 = (int16_t)(mcy - halfLen * sinR);
        int16_t x2 = (int16_t)(mcx + halfLen * cosR);
        int16_t y2 = (int16_t)(mcy + halfLen * sinR);

        // Only draw if roughly on screen
        if (y1 > -20 && y1 < H + 20 && y2 > -20 && y2 < H + 20) {
            canvas.drawLine(x1, y1, x2, y2, COL_PITCH);

            // Small down-ticks at ends for negative pitch (below horizon)
            if (marks[i] < 0) {
                float tickLen = 3.0f;
                canvas.drawLine(x1, y1,
                    (int16_t)(x1 - tickLen * sinR),
                    (int16_t)(y1 + tickLen * cosR), COL_PITCH);
                canvas.drawLine(x2, y2,
                    (int16_t)(x2 - tickLen * sinR),
                    (int16_t)(y2 + tickLen * cosR), COL_PITCH);
            }
        }
    }
}

// --- Center Reticle (HUD-style) ---
static void drawReticle(float totalAngle) {
    uint16_t col = COL_RETICLE;
    if (totalAngle < 1.0f)      col = COL_LEVEL;
    else if (totalAngle < 3.0f) col = COL_NEAR;

    // Center dot
    canvas.fillCircle(CX, CY, 2, col);
    // Left wing
    canvas.drawLine(CX - 22, CY, CX - 6, CY, col);
    // Right wing
    canvas.drawLine(CX + 6,  CY, CX + 22, CY, col);
    // Wing tips (down)
    canvas.drawLine(CX - 22, CY, CX - 22, CY + 5, col);
    canvas.drawLine(CX + 22, CY, CX + 22, CY + 5, col);
    // Inner drops
    canvas.drawLine(CX - 6, CY, CX - 6, CY + 3, col);
    canvas.drawLine(CX + 6, CY, CX + 6, CY + 3, col);
}

// --- Roll reference triangle (fixed at top center) ---
static void drawRollPointer() {
    canvas.fillTriangle(CX, 3, CX - 5, 11, CX + 5, 11, COL_HORIZON);
}

// --- G-Force Bar (right edge) ---
static void drawGBar(float gForce) {
    // Background
    canvas.fillRect(GBAR_X, 0, GBAR_W, H, COL_GBAR_BG);

    // Fill height
    float ratio = gForce / GBAR_MAX_G;
    if (ratio > 1.0f) ratio = 1.0f;
    int16_t fillH = (int16_t)(ratio * H);
    if (fillH < 1) fillH = 1;

    // Color by magnitude
    uint16_t col = COL_GBAR_G;
    if (gForce > 3.0f)      col = COL_GBAR_R;
    else if (gForce > 1.5f) col = COL_GBAR_Y;

    canvas.fillRect(GBAR_X, H - fillH, GBAR_W, fillH, col);

    // 1g reference tick
    int16_t refY = H - (int16_t)(H / GBAR_MAX_G);
    canvas.drawLine(GBAR_X, refY, GBAR_X + GBAR_W - 1, refY, COL_TEXT);
}

// --- Text with 1px shadow for readability ---
static void drawTextShadow(const char *str, int16_t x, int16_t y) {
    canvas.setTextColor(COL_SHADOW);
    canvas.drawString(str, x + 1, y + 1);
    canvas.setTextColor(COL_TEXT);
    canvas.drawString(str, x, y);
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

    // Seed filter
    M5.Imu.update();
    m5::imu_data_t d;
    M5.Imu.getImuData(&d);
    accelToAngles(d.accel.x, d.accel.y, d.accel.z, filtPitch, filtRoll);
}

void loop() {
    M5.update();

    // Calibration
    if (M5.BtnA.wasPressed()) {
        offsPitch = filtPitch;
        offsRoll  = filtRoll;
    }

    // Read IMU
    M5.Imu.update();
    m5::imu_data_t d;
    M5.Imu.getImuData(&d);

    float rawPitch, rawRoll;
    accelToAngles(d.accel.x, d.accel.y, d.accel.z, rawPitch, rawRoll);

    // Low-pass filter
    filtPitch += ALPHA * (rawPitch - filtPitch);
    filtRoll  += ALPHA * (rawRoll  - filtRoll);

    // G-force magnitude
    accelMag = sqrtf(d.accel.x * d.accel.x +
                     d.accel.y * d.accel.y +
                     d.accel.z * d.accel.z);

    // Apply calibration
    float pitch = filtPitch - offsPitch;
    float roll  = filtRoll  - offsRoll;
    float totalAngle = sqrtf(pitch * pitch + roll * roll);

    // === Render ===
    drawHorizon(pitch, roll);
    drawPitchLadder(pitch, roll);
    drawReticle(totalAngle);
    drawRollPointer();
    drawGBar(accelMag);

    // --- Text overlays ---
    char buf[24];

    // Total angle (large, top-left)
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(top_left);
    snprintf(buf, sizeof(buf), "%.1f%c", totalAngle, 0xB0);
    drawTextShadow(buf, 3, 14);

    // G-force (small, top-right)
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(top_right);
    snprintf(buf, sizeof(buf), "%.1fg", accelMag);
    drawTextShadow(buf, W - 8, 3);

    // P/R values (small, bottom-left)
    canvas.setTextDatum(bottom_left);
    snprintf(buf, sizeof(buf), "P:%.1f R:%.1f", pitch, roll);
    drawTextShadow(buf, 3, H - 3);

    // Push to screen
    canvas.pushSprite(0, 0);

    delay(33);  // ~30 fps
}
