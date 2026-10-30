#include "theme_picker.h"
#include "button_click.h"
#include <cassert>
int main(){
 ThemePicker p;p.open(2);assert(p.active&&p.initial==2&&p.selected==2);
 assert(p.step(1)==3&&p.step(4)==0&&p.step(-1)==6&&p.step(-14)==6);
 assert(p.cancel()==2&&!p.active);p.open(3);assert(p.step(2)==5);p.saved();assert(!p.active&&p.selected==5);
 p.open(99);assert(p.selected==0&&p.initial==0);
 for(int i=0;i<7;++i)assert(uiThemeName(i)[0]!='\0');
 ButtonClick b;assert(b.press(100)==0);assert(b.tick(3100,true)==ButtonClick::Long);
 assert(b.tick(3200,false)==0&&b.tick(4000,false)==0); // Opening hold release cannot save.
 assert(b.press(5000)==0);assert(b.tick(5350,false)==ButtonClick::Single);
}
