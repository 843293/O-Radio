#pragma once
#include <stdint.h>
#include <cmath>
struct FmQuality {
  bool valid=false;
  uint16_t status=0,usn=1000,wam=1000;
  int level=-200,offset=0;
};
inline bool fmFrequencyValid(uint32_t khz){return khz>=65000&&khz<=108000&&khz%10==0;}
inline bool fmSignalCandidate(const FmQuality& q){
  return q.valid&&(q.status&0x3ff)>=320&&q.level>=180&&q.usn<=300&&q.wam<=400&&q.offset>=-300&&q.offset<=300;
}
inline int fmVolumeDb(int percent){
  if(percent<=0)return -60;
  if(percent>=100)return -6;
  // Match the network player's squared amplitude curve, with 6dB headroom.
  const int db=int(std::lround(40.0*std::log10(percent/100.0)-6.0));
  return db< -60?-60:db;
}
struct FmPeak {uint32_t khz=0;int level=0;};
struct FmPeakList {
  FmPeak entries[64]{};
  unsigned count=0;
  uint32_t previous=0;
  bool tracking=false;
  void clear(){count=0;previous=0;tracking=false;}
  void add(uint32_t khz,int level){
    if(count&&tracking&&khz==previous+100){if(level>entries[count-1].level)entries[count-1]={khz,level};}
    else if(count<64){entries[count++]={khz,level};tracking=true;}
    else tracking=false;
    previous=khz;
  }
};
