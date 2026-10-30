#pragma once
#include <stdint.h>
#include <time.h>
struct RadioAlarm {
  bool enabled=false;
  uint8_t hour=7,minute=0,days=127,volume=50;
  uint32_t stationId=0;
  int lastDay=0;
};
inline bool radioClockValid(const tm& t){return t.tm_year>=124&&t.tm_year<200;}
inline int radioDayKey(const tm& t){return (t.tm_year+1900)*10000+(t.tm_mon+1)*100+t.tm_mday;}
inline bool radioAlarmDue(const RadioAlarm& a,const tm& t){
  return radioClockValid(t)&&a.enabled&&a.hour==t.tm_hour&&a.minute==t.tm_min&&
    a.lastDay!=radioDayKey(t)&&(a.days==0||(a.days&(1u<<t.tm_wday)));
}
