#pragma once
#include <stdint.h>
class DisplayIdle {
 public:
  bool activity(uint32_t now){
    const bool woke=asleep_;asleep_=false;last_=now;
    if(woke){guarded_=true;wakeAt_=now;}return woke;
  }
  void tick(uint32_t now,bool busy){if(busy)activity(now);else if(uint32_t(now-last_)>=30000)asleep_=true;}
  bool asleep()const{return asleep_;}
  bool wakeGuard(uint32_t now){if(guarded_&&uint32_t(now-wakeAt_)<500)return true;guarded_=false;return false;}
 private:
  bool asleep_=false,guarded_=false;
  uint32_t last_=0,wakeAt_=0;
};
