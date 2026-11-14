#pragma once

#include <Adafruit_ADS1X15.h>

// Copy this file to BattmonConfig.h, then edit the values for your setup.
namespace BattmonConfig {
constexpr uint8_t kSdaPin = 20;
constexpr uint8_t kSclPin = 19;
constexpr uint32_t kSampleIntervalMs = 1000;
// Dashboard history is sampled at this interval; the chart retains one hour.
constexpr uint32_t kHistorySampleIntervalMs = 10000;
constexpr uint32_t kSerialBaud = 115200;

constexpr char kApSsid[] = "Battmon-ESP32C6";
constexpr char kApPassword[] = "change-this-password";  // At least 8 characters.
constexpr char kOtaUsername[] = "admin";

constexpr uint8_t kAds48Address = 0x48;
constexpr uint8_t kAds49Address = 0x49;
constexpr uint8_t kAds4AAddress = 0x4A;
constexpr auto kAdcGain = GAIN_TWO;
constexpr char kReadingName0[] = "Battery 1 · ADS1015 0x48 A0-A1";
constexpr char kReadingName1[] = "Battery 2 · ADS1015 0x48 A2-A3";
constexpr char kReadingName2[] = "Battery 3 · ADS1015 0x49 A0-A1";
constexpr char kReadingName3[] = "Battery 4 · ADS1015 0x49 A2-A3";
constexpr char kReadingName4[] = "Battery 5 · ADS1015 0x4A A0-A1";
constexpr char kReadingName5[] = "Current sensor · ADS1015 0x4A A2-A3";

// Calibrated value = (measured differential voltage - offset) × factor.
constexpr float kAds48A0A1VoltageFactor = 10.0f;
constexpr float kAds48A2A3VoltageFactor = 10.0f;
constexpr float kAds49A0A1VoltageFactor = 10.0f;
constexpr float kAds49A2A3VoltageFactor = 10.0f;
constexpr float kAds4AA0A1VoltageFactor = 10.0f;
constexpr float kAds4AA2A3CurrentOffsetVolts = 2.5f;
constexpr float kAds4AA2A3AmpsPerVolt = 100.0f;
}  // namespace BattmonConfig
