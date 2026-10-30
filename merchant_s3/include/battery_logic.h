#pragma once
#include <stdint.h>
inline int batteryMillivoltsFromRaw(int raw){return (raw*179+50)/100;}
inline bool batteryVoltagePlausible(int mv){return mv>=2700&&mv<=4400;}
// Rough 1S Li-ion voltage estimate, not coulomb counting or charge detection.
inline int batteryPercentEstimate(int mv){
  const int volts[]={3300,3500,3600,3700,3800,3900,4000,4100,4200};
  const int percents[]={0,10,20,40,60,75,85,95,100};
  if(mv<=volts[0])return 0;
  for(unsigned i=1;i<9;++i)if(mv<=volts[i])return percents[i-1]+(mv-volts[i-1])*(percents[i]-percents[i-1])/(volts[i]-volts[i-1]);
  return 100;
}
