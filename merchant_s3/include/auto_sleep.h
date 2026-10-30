#pragma once
#include <stdint.h>
class AutoSleep {
 public:
  static constexpr uint32_t kDelayMs=30u*60u*1000u;
  void activity(uint32_t now){since_=now;}
  bool tick(uint32_t now,bool busy){
    if(busy){activity(now);return false;}
    if(uint32_t(now-since_)<kDelayMs)return false;
    // A rejected request must not be retried every main-loop iteration.
    activity(now);return true;
  }
  uint32_t idleMs(uint32_t now)const{return uint32_t(now-since_);}
 private:
  uint32_t since_=0;
};
