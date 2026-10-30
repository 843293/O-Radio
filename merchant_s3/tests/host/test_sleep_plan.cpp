#include "sleep_plan.h"
#include <cassert>
#include <cstdlib>
time_t makeTime(int day,int hour,int minute){tm t{};t.tm_year=126;t.tm_mon=9;t.tm_mday=day;t.tm_hour=hour;t.tm_min=minute;t.tm_isdst=-1;return mktime(&t);}
int main(){
 setenv("TZ","UTC-8",1);tzset();
 RadioAlarm a;a.enabled=true;auto now=makeTime(30,6,0);
 assert(nextRadioAlarmEpoch(a,now)==makeTime(30,7,0));
 assert(nextRadioAlarmEpoch(a,makeTime(30,7,1))==makeTime(31,7,0));
 a.days=1<<1;tm t{};auto next=nextRadioAlarmEpoch(a,now);localtime_r(&next,&t);assert(t.tm_mon==10&&t.tm_mday==2&&t.tm_wday==1&&t.tm_hour==7);
 a.days=1<<5;next=nextRadioAlarmEpoch(a,makeTime(30,8,0));localtime_r(&next,&t);assert(t.tm_mon==10&&t.tm_mday==6);
 a.days=127;a.lastDay=20261030;assert(nextRadioAlarmEpoch(a,now)==makeTime(31,7,0));
 a.days=0;a.lastDay=0;assert(nextRadioAlarmEpoch(a,now)==makeTime(30,7,0));
 a.enabled=false;assert(nextRadioAlarmEpoch(a,now)==0);a.enabled=true;assert(nextRadioAlarmEpoch(a,0)==0);
 assert(radioAlarmWakeDelayUs(100,100)==0&&radioAlarmWakeDelayUs(99,100)==0);
 assert(radioAlarmWakeDelayUs(102,100)==0&&radioAlarmWakeDelayUs(110,100)==5000000);
 assert(radioAlarmWakeDelayUs(220,100)==60000000&&radioAlarmWakeDelayUs(400,100)==240000000);
 assert(!radioWakeAlarmDue(200,199)&&radioWakeAlarmDue(200,200)&&radioWakeAlarmDue(200,800)&&!radioWakeAlarmDue(200,801));
 RadioAlarm b=a;assert(!radioAlarmScheduleChanged(a,b));b.volume=80;assert(!radioAlarmScheduleChanged(a,b));
 b.minute=(a.minute+1)%60;assert(radioAlarmScheduleChanged(a,b));
 RadioAlarm used;used.enabled=true;used.hour=7;used.minute=0;used.lastDay=20261030;
 auto later=used;later.hour=8;if(radioAlarmScheduleChanged(used,later))later.lastDay=0;
 assert(nextRadioAlarmEpoch(used,makeTime(30,7,30))==makeTime(31,7,0));
 assert(nextRadioAlarmEpoch(later,makeTime(30,7,30))==makeTime(30,8,0));
}
