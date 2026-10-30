#include "battery_monitor.h"
#include <cassert>
int main(){
 assert(batteryMillivoltsFromRaw(2200)==3938);
 assert(!batteryVoltagePlausible(0)&&!batteryVoltagePlausible(4500)&&batteryVoltagePlausible(4200));
 assert(batteryPercentEstimate(3000)==0&&batteryPercentEstimate(4200)==100&&batteryPercentEstimate(3800)==60);
 for(int v=2701;v<=4400;++v)assert(batteryPercentEstimate(v)>=batteryPercentEstimate(v-1));
 BatteryMonitor b;b.begin();assert(testAdcPin==3&&testAdcResolution==12&&testAdcAtten==ADC_11db);
 assert(b.valid&&b.raw==2200&&b.millivolts==3938);assert(testAdcReads==20);
 testAdcRaw=0;testMillis=4999;b.tick();assert(b.valid&&testAdcReads==20);
 testMillis=5000;b.tick();assert(!b.valid);testAdcRaw=2350;testMillis=10000;b.tick();assert(b.valid&&b.percent==100);
 testAdcNoisy=true;testMillis=15000;b.tick();assert(!b.valid);
}
