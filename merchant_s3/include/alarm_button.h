#pragma once
#include <stdint.h>
// Release-based double click; long hold never emits a trailing short click.
class AlarmButton {
 public:
  enum {None=0,Short=1,Long=2,Double=3};
  void press(uint32_t now){down_=true;held_=false;start_=now;second_=pending_&&uint32_t(now-released_)<350;if(second_)pending_=false;}
  int release(uint32_t now){if(!down_)return None;down_=false;if(held_)return None;if(second_){second_=false;return Double;}pending_=true;released_=now;return None;}
  int tick(uint32_t now){
    if(down_&&!held_&&uint32_t(now-start_)>=1500){held_=true;pending_=false;second_=false;return Long;}
    if(!down_&&pending_&&uint32_t(now-released_)>=350){pending_=false;return Short;}return None;
  }
  void suppressRelease(){held_=true;pending_=false;second_=false;}
 private:
  bool down_=false,held_=false,pending_=false,second_=false;
  uint32_t start_=0,released_=0;
};
