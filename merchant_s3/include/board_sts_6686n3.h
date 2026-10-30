#pragma once

#include <Arduino.h>

namespace board {

constexpr uint8_t kTftSclk = 8;
constexpr uint8_t kTftMiso = 18;
constexpr uint8_t kTftMosi = 4;
constexpr uint8_t kTftCs = 38;
constexpr uint8_t kTftDc = 16;
constexpr uint8_t kTftReset = 17;
constexpr uint8_t kTftBacklight = 9;

// Merchant ADC battery object: GPIO3, 20 samples, raw * 1.79 mV.
constexpr uint8_t kBatteryAdc = 3;

constexpr uint8_t kTefSda = 6;
constexpr uint8_t kTefScl = 5;
constexpr uint8_t kTefAddress = 0x64;

constexpr uint8_t kEncoderAClock = 42;
constexpr uint8_t kEncoderAData = 41;
constexpr uint8_t kEncoderAButton = 40;

constexpr uint8_t kEncoderBClock = 10;
constexpr uint8_t kEncoderBData = 11;
constexpr uint8_t kEncoderBButton = 12;

constexpr uint8_t kI2sBclk = 2;
constexpr uint8_t kI2sLrclk = 43;
constexpr uint8_t kI2sDout = 1;
constexpr uint8_t kI2sMclk = 44;

// The merchant firmware holds this high during initialization, then drives it low.
constexpr uint8_t kAmplifierMute = 45;

}  // namespace board
