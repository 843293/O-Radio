#pragma once
#include <stddef.h>
#include <stdint.h>
// Invalid UTF-8 consumes one byte, so malformed metadata cannot stall rendering.
inline uint32_t lcdNextCodepoint(const char* s,size_t len,size_t& at) {
  if(at>=len)return 0;
  const uint8_t first=uint8_t(s[at++]);
  if(first<128)return first;
  const int count=first>=0xC2&&first<=0xDF?1:first>=0xE0&&first<=0xEF?2:first>=0xF0&&first<=0xF4?3:0;
  if(!count||at+count>len)return 0xFFFD;
  uint32_t cp=first&((1u<<(6-count))-1);
  for(int i=0;i<count;++i){const uint8_t b=uint8_t(s[at+i]);if((b&0xC0)!=0x80)return 0xFFFD;cp=(cp<<6)|(b&63);}
  if((count==1&&cp<128)||(count==2&&cp<2048)||(count==3&&cp<65536)||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF))return 0xFFFD;
  at+=count;return cp;
}
