#include <cassert>
#include <iostream>
#include "radio_core.h"
#include <Wire.h>
bool contains(std::initializer_list<uint8_t> bytes){
  const std::vector<uint8_t> expected(bytes);
  return std::find(Wire.writes.begin(),Wire.writes.end(),expected)!=Wire.writes.end();
}
int main(){
 RadioCore radio;assert(radio.begin(true));
 assert(Wire.sda==6&&Wire.scl==5&&Wire.address==0x64);
 assert(contains({0x40,4,1,0,0x8c,0xa0,0,0,0}));
 assert(contains({0x1c,0,0x74})&&contains({0x1c,0,0x75}));
 assert(contains({48,11,1,0,1}));
 for(const auto& c:Wire.writes){
   assert(!(c.size()>=3&&(c[0]==32||c[0]==33)&&c[1]==1));
   if(c.size()==13&&c[0]==48&&c[1]==22)assert(c[9]==0&&c[10]==0);
 }
 Wire.writes.clear();assert(radio.selectFm(99500,50,true));
 assert(contains({48,12,1,0,0}));assert(contains({48,13,1,0,128,0,224}));
 assert(contains({48,13,1,0,33,0,224}));
 assert(contains({48,22,1,0,33,0,2,0,32,1,0,0x12,0xc0}));
 assert(contains({64,1,1,0,0}));
 assert(contains({32,1,1,0,1,0x26,0xde}));
 assert(contains({64,3,1,0,0,0,32,0,3}));
 assert(contains({64,3,1,0,0,0,33,0,3}));
 assert(radio.state.frequencyKhz==99500&&radio.state.muted);
 Wire.writes.clear();assert(radio.setFmVolume(50,false));
 assert(contains({48,10,1,0xff,0x4c})); // -180 tenths dB
 assert(contains({48,11,1,0,0}));
 for(const auto& c:Wire.writes)assert(!(c[0]==32&&c[1]==1)); // volume must not retune
 assert(!radio.selectFm(64990,50,false));assert(!radio.setFmVolume(101,false));
 Wire.response={3,0xe8,0,250,0,100,0,50,0xff,0x9c,0,0,0,0};
 const auto q=radio.sampleFmQuality();assert(q.valid&&q.status==1000&&q.level==250&&q.offset==-100&&q.usn==100&&q.wam==50);
 const auto diagnostic=radio.diagnostics();assert(diagnostic.operationValid&&diagnostic.processingValid);
 assert(contains({64,128,1})&&contains({32,134,1}));
 Wire.shortRead=true;assert(!radio.sampleFmQuality().valid);Wire.shortRead=false;Wire.response.clear();
 Wire.writes.clear();assert(radio.selectNetwork());assert(contains({48,11,1,0,1}));
 assert(contains({48,22,1,0,33,0,0,0,32,0,0,0x12,0xc0}));
 assert(contains({64,1,1,0,1}));
 Wire.writes.clear();assert(radio.prepareDeepSleep());
 assert(contains({48,21,1,0,128,0,0}));assert(contains({64,1,1,0,1}));assert(radio.state.muted);
 Wire.fail=true;assert(!radio.prepareDeepSleep());assert(!radio.selectFm(99500,50,true));
 RadioCore missing;assert(!missing.begin(true));
 std::cout<<"PASS: TEF init, no RF tune at network boot, FM source, volume, quality, return to network and I/O failures\n";
}
