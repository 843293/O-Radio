#include "lcd_utf8.h"
#include <cassert>
#include <cstring>
int main(){
  const char* s="A中国电台";size_t at=0;size_t len=strlen(s);
  assert(lcdNextCodepoint(s,len,at)=='A');
  assert(lcdNextCodepoint(s,len,at)==0x4E2D);
  assert(lcdNextCodepoint(s,len,at)==0x56FD);
  assert(lcdNextCodepoint(s,len,at)==0x7535);
  assert(lcdNextCodepoint(s,len,at)==0x53F0);assert(at==len);
  const char* bad[]={"\xC0\x80","\xED\xA0\x80","\xF4\x90\x80\x80","\xE4\xB8","\x80"};
  for(auto b:bad){at=0;assert(lcdNextCodepoint(b,strlen(b),at)==0xFFFD);assert(at>0);}
}
