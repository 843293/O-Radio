#include "button_click.h"
#include <cassert>
int main(){
 ButtonClick a;assert(a.press(100)==0);assert(a.tick(449,false)==0);assert(a.tick(450,false)==1);assert(a.tick(900,false)==0);
 ButtonClick b;assert(b.press(100)==0);assert(b.press(300)==2);assert(b.tick(1000,false)==0);
 ButtonClick c;assert(c.press(100)==0);assert(c.tick(900,true)==0);assert(c.tick(901,false)==1);
 ButtonClick d;assert(d.press(100)==0);assert(d.press(451)==1);assert(d.tick(801,false)==1);
 ButtonClick e;assert(e.press(0xFFFFFF00u)==0);assert(e.tick(100,false)==1);
 ButtonClick f;assert(f.press(0xFFFFFF00u)==0);assert(f.press(10)==2);assert(f.tick(1000,false)==0);
 ButtonClick held;assert(held.press(100)==0);assert(held.tick(3099,true)==0);assert(held.tick(3100,true)==3);assert(held.tick(5000,true)==0);assert(held.tick(5001,false)==0);
 ButtonClick doubleHeld;assert(doubleHeld.press(100)==0);assert(doubleHeld.press(200)==2);assert(doubleHeld.tick(4000,true)==0);assert(doubleHeld.tick(4001,false)==0);
}
