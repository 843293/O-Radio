#include "battery_monitor.h"
#include "board_sts_6686n3.h"
void BatteryMonitor::begin(){
  analogReadResolution(12);analogSetPinAttenuation(board::kBatteryAdc,ADC_11db);tick();
}
void BatteryMonitor::tick(){
  if(sampled_&&millis()-last_<5000)return;
  last_=millis();sampled_=true;
  int total=0,lowest=4095,highest=0;
  for(int i=0;i<20;++i){const int value=analogRead(board::kBatteryAdc);total+=value;if(value<lowest)lowest=value;if(value>highest)highest=value;}
  raw=(total+10)/20;const int reading=batteryMillivoltsFromRaw(raw);
  const bool plausible=batteryVoltagePlausible(reading)&&highest-lowest<=300;
  if(!plausible){valid=false;millivolts=0;percent=0;return;}
  millivolts=valid?(millivolts*3+reading+2)/4:reading;
  percent=batteryPercentEstimate(millivolts);valid=true;
}
