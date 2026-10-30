#pragma once
#include <stdint.h>
// Signed multiplication is defined for the entire int16 range, unlike left shift.
constexpr int32_t pcm16To32(int16_t sample) { return int32_t(sample)*65536; }
