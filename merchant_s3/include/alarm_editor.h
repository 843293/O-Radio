#pragma once
#include <stdint.h>
inline int alarmWrap(int value,int size){return (value%size+size)%size;}
inline uint8_t alarmRepeatStep(uint8_t days,int delta){
  const uint8_t masks[]={0,127,62,65};
  int at=-1;for(int i=0;i<4;++i)if(days==masks[i])at=i;
  return masks[alarmWrap(at+delta,4)];
}
