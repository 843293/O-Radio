#pragma once
#include <stdint.h>
// Called with debounced presses. Single click waits for the double-click window.
class ButtonClick {
 public:
  enum { None=0, Single=1, Double=2, Long=3 };
  int press(uint32_t now){
    if(pending_&&uint32_t(now-start_)<350){pending_=false;longArmed_=false;return Double;}
    const int event=pending_?Single:None;
    pending_=true;start_=now;longArmed_=true;return event;
  }
  int tick(uint32_t now,bool down){
    if(down&&longArmed_&&uint32_t(now-start_)>=3000){pending_=false;longArmed_=false;return Long;}
    if(!down)longArmed_=false;
    if(pending_&&!down&&uint32_t(now-start_)>=350){pending_=false;return Single;}
    return None;
  }
 private:
  bool pending_=false,longArmed_=false;
  uint32_t start_=0;
};
