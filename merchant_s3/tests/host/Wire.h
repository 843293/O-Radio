#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
class FakeWire {
 public:
  std::vector<std::vector<uint8_t>> writes;
  std::vector<uint8_t> current;
  bool fail=false,shortRead=false;
  int sda=-1,scl=-1,address=-1;
  size_t remaining=0,readPosition=0;
  std::vector<uint8_t> response;
  void begin(int a,int b,int){sda=a;scl=b;}
  void setTimeOut(int){}
  void beginTransmission(int a){address=a;current.clear();}
  size_t write(const uint8_t* p,size_t n){current.assign(p,p+n);return n;}
  uint8_t endTransmission(){writes.push_back(current);return fail?2:0;}
  size_t requestFrom(uint8_t,size_t n){remaining=shortRead?0:n;readPosition=0;return remaining;}
  int available(){return remaining;}
  int read(){if(remaining)--remaining;return readPosition<response.size()?response[readPosition++]:0;}
};
inline FakeWire Wire;
