/**
 * Tennis Ball IMU Test - M5Stack ATOM S3
 *
 * Reads MPU6886 6-axis IMU data (accelerometer + gyroscope)
 * at high frequency for analyzing tennis ball motion dynamics.
 *
 * Output format (CSV via Serial):
 *   timestamp_ms, accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, impact_flag
 */

#include <M5Unified.h>
#include <math.h>

// --- Configuration ---
static const uint32_t SAMPLE_INTERVAL_MS = 5;    // 200Hz sampling rate
static const float IMPACT_THRESHOLD_G    = 8.0f; // Impact detection threshold (g)
static const float G_TO_MS2              = 9.80665f;

// --- State ---
static uint32_t lastSampleTime = 0;
static uint32_t sampleCount    = 0;
static float    peakAccelG     = 0.0f;
static bool     recording      = true;

// Compute total acceleration magnitude in g
static float accelMagnitudeG(float ax, float ay, float az) {
    return sqrtf(ax * ax + ay * ay + az * az);
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    // Initialize IMU
    if (!M5.Imu.isEnabled()) {
        M5.Display.fillScreen(TFT_RED);
        M5.Display.setCursor(0, 0);
        M5.Display.setTextSize(1);
        M5.Display.println("IMU FAIL!");
        Serial.println("ERROR: IMU not found!");
        while (1) { delay(1000); }
    }

    // Display startup info
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.println("Tennis IMU");
    M5.Display.println("Ready!");

    // Print CSV header
    Serial.println("timestamp_ms,accel_x_g,accel_y_g,accel_z_g,gyro_x_dps,gyro_y_dps,gyro_z_dps,accel_mag_g,impact");

    Serial.println("# Tennis Ball IMU Logger Started");
    Serial.print("# Sample rate: ");
    Serial.print(1000 / SAMPLE_INTERVAL_MS);
    Serial.println(" Hz");
    Serial.print("# Impact threshold: ");
    Serial.print(IMPACT_THRESHOLD_G);
    Serial.println(" g");

    lastSampleTime = millis();
}

void loop() {
    M5.update();

    // Button press toggles recording on/off
    if (M5.BtnA.wasPressed()) {
        recording = !recording;
        M5.Display.fillScreen(recording ? TFT_BLACK : TFT_BLUE);
        M5.Display.setCursor(0, 0);
        M5.Display.println(recording ? "REC ON" : "PAUSED");
        Serial.println(recording ? "# RECORDING RESUMED" : "# RECORDING PAUSED");
    }

    if (!recording) return;

    uint32_t now = millis();
    if (now - lastSampleTime < SAMPLE_INTERVAL_MS) return;
    lastSampleTime = now;

    // Read IMU data
    m5::imu_data_t imuData;
    M5.Imu.update();
    M5.Imu.getImuData(&imuData);

    float ax = imuData.accel.x;  // g
    float ay = imuData.accel.y;
    float az = imuData.accel.z;
    float gx = imuData.gyro.x;   // degrees per second
    float gy = imuData.gyro.y;
    float gz = imuData.gyro.z;

    float mag = accelMagnitudeG(ax, ay, az);
    bool impact = (mag > IMPACT_THRESHOLD_G);

    if (mag > peakAccelG) peakAccelG = mag;

    // CSV output
    Serial.printf("%lu,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.4f,%d\n",
                  now, ax, ay, az, gx, gy, gz, mag, impact ? 1 : 0);

    sampleCount++;

    // Update display every 500ms
    if (sampleCount % (1000 / SAMPLE_INTERVAL_MS / 2) == 0) {
        M5.Display.fillScreen(impact ? TFT_RED : TFT_BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setTextSize(1);
        M5.Display.printf("Acc:%.1fg\n", mag);
        M5.Display.printf("Pk :%.1fg\n", peakAccelG);
        M5.Display.printf("Gx:%.0f\n", gx);
        M5.Display.printf("Gy:%.0f\n", gy);
        M5.Display.printf("N:%lu", sampleCount);
    }
}
