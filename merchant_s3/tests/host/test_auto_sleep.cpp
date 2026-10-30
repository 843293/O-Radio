#include "auto_sleep.h"
#include <cassert>
int main(){
 AutoSleep a;a.activity(100);
 assert(!a.tick(100+AutoSleep::kDelayMs-1,false));
 assert(a.tick(100+AutoSleep::kDelayMs,false));
 assert(!a.tick(101+AutoSleep::kDelayMs,false));
 a.activity(5000000);assert(!a.tick(6000000,false));a.activity(6000000);
 assert(!a.tick(7799999,false));assert(a.tick(7800000,false));
 AutoSleep b;assert(!b.tick(1800000,true));assert(!b.tick(3599999,false));assert(b.tick(3600000,false));
 AutoSleep c;c.activity(0xfffffff0u);assert(!c.tick(uint32_t(0xfffffff0u+AutoSleep::kDelayMs-1),false));assert(c.tick(uint32_t(0xfffffff0u+AutoSleep::kDelayMs),false));
}
