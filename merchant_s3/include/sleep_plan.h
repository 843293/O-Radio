#pragma once
#include "alarm_logic.h"
inline time_t nextRadioAlarmEpoch(const RadioAlarm& a,time_t now){
  tm today{};if(!a.enabled||!localtime_r(&now,&today)||!radioClockValid(today))return 0;
  for(int day=0;day<=7;++day){
    tm candidate=today;candidate.tm_mday+=day;candidate.tm_hour=a.hour;candidate.tm_min=a.minute;candidate.tm_sec=0;candidate.tm_isdst=-1;
    const time_t epoch=mktime(&candidate);
    if(epoch<=now||radioDayKey(candidate)==a.lastDay)continue;
    if(a.days==0||(a.days&(1u<<candidate.tm_wday)))return epoch;
  }
  return 0;
}
// Short test alarms are allowed too: reserve half the remaining time for Wi-Fi.
inline uint64_t radioAlarmWakeDelayUs(time_t target,time_t now){
  const int64_t remaining=int64_t(target)-int64_t(now);
  if(remaining<=2)return 0;
  const int64_t lead=remaining/2<60?remaining/2:60;
  return uint64_t(remaining-lead)*1000000ULL;
}
inline bool radioAlarmScheduleChanged(const RadioAlarm& before,const RadioAlarm& after){
  return before.hour!=after.hour||before.minute!=after.minute||before.days!=after.days;
}
inline bool radioWakeAlarmDue(time_t target,time_t now){return target>0&&now>=target&&int64_t(now)-int64_t(target)<=600;}
