#include "display_idle.h"
#include <cassert>
int main(){
 DisplayIdle d;d.activity(100);d.tick(30099,false);assert(!d.asleep());d.tick(30100,false);assert(d.asleep());
 assert(d.activity(31000));assert(!d.asleep());assert(d.wakeGuard(31000));assert(d.wakeGuard(31499));assert(!d.wakeGuard(31500));
 d.activity(32000);assert(!d.wakeGuard(32000));d.tick(62000,true);assert(!d.asleep());
 d.tick(91999,false);assert(!d.asleep());d.tick(92000,false);assert(d.asleep());
 d.tick(92001,true);assert(!d.asleep());assert(d.wakeGuard(92001));
 DisplayIdle wrap;wrap.activity(0xFFFFFF00u);wrap.tick(29744,false);assert(wrap.asleep());
}
