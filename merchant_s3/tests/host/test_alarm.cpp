#include "alarm_logic.h"
#include <cassert>
int main(){
 tm t{};t.tm_year=126;t.tm_mon=9;t.tm_mday=29;t.tm_wday=4;t.tm_hour=7;
 RadioAlarm a;a.enabled=true;
 assert(radioAlarmDue(a,t));a.lastDay=20261029;assert(!radioAlarmDue(a,t));
 a.lastDay=0;a.days=2;assert(!radioAlarmDue(a,t));a.days=16;assert(radioAlarmDue(a,t));
 a.days=0;assert(radioAlarmDue(a,t));t.tm_min=1;assert(!radioAlarmDue(a,t));t.tm_min=0;
 t.tm_year=70;assert(!radioAlarmDue(a,t));t.tm_year=126;a.enabled=false;assert(!radioAlarmDue(a,t));
 assert(radioDayKey(t)==20261029);
}
