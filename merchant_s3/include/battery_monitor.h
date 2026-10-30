#pragma once
#include <Arduino.h>
#include "battery_logic.h"
class BatteryMonitor {
 public:
  void begin();
  void tick();
  bool valid=false;
  int raw=0,millivolts=0,percent=0;
 private:
  bool sampled_=false;
  uint32_t last_=0;
};
