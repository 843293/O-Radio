#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <algorithm>
#define PROGMEM
using String=std::string;
using std::min;
inline uint32_t testMillis=0;
inline uint32_t millis(){return testMillis;}
inline void delay(uint32_t n){testMillis+=n;}
template<class T> T constrain(T x,T lo,T hi){return std::clamp(x,lo,hi);}

inline int testAdcRaw=2200,testAdcResolution=0,testAdcPin=-1,testAdcAtten=-1,testAdcReads=0;
inline bool testAdcNoisy=false;
constexpr int ADC_11db=3;
inline void analogReadResolution(int bits){testAdcResolution=bits;}
inline void analogSetPinAttenuation(int pin,int atten){testAdcPin=pin;testAdcAtten=atten;}
inline int analogRead(int){++testAdcReads;return testAdcRaw+(testAdcNoisy?(testAdcReads%2?500:-500):0);}
