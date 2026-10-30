#include "alarm_button.h"
#include "alarm_editor.h"
#include <cassert>
int main(){
 AlarmButton a;a.press(10);assert(a.release(40)==0);assert(a.tick(389)==0);assert(a.tick(390)==1);assert(a.tick(500)==0);
 AlarmButton b;b.press(10);assert(b.release(40)==0);b.press(200);assert(b.release(240)==3);assert(b.tick(1000)==0);
 AlarmButton c;c.press(10);assert(c.tick(1509)==0);assert(c.tick(1510)==2);assert(c.tick(2000)==0);assert(c.release(2100)==0);assert(c.tick(3000)==0);
 AlarmButton d;d.press(10);d.suppressRelease();assert(d.release(20)==0);assert(d.tick(900)==0);
 AlarmButton e;e.press(0xfffffff0u);assert(e.release(0xfffffffau)==0);e.press(100);assert(e.release(120)==3);
 AlarmButton f;f.press(0xfffffff0u);assert(f.tick(1490)==2);assert(f.release(1500)==0);
 AlarmButton g;g.press(0);g.release(10);g.press(200);assert(g.tick(1700)==2);assert(g.release(1800)==0);assert(g.tick(2200)==0);
 assert(alarmWrap(-1,24)==23&&alarmWrap(24,24)==0&&alarmWrap(-61,60)==59);
 assert(alarmRepeatStep(0,1)==127&&alarmRepeatStep(127,1)==62&&alarmRepeatStep(62,1)==65&&alarmRepeatStep(65,1)==0);
 assert(alarmRepeatStep(0,-1)==65&&alarmRepeatStep(42,1)==0);
}
