#include "ui_palette.h"
#include <cassert>
#include <cmath>
#include <algorithm>
// Test the actual RGB565-quantized colors used by the LCD, not just RGB888.
double luminance(uint32_t rgb){
 const auto q=uiRgb565(rgb);
 double r=((q>>11)&31)/31.0,g=((q>>5)&63)/63.0,b=(q&31)/31.0;
 auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
 return .2126*linear(r)+.7152*linear(g)+.0722*linear(b);
}
double contrast(uint32_t a,uint32_t b){double x=luminance(a),y=luminance(b);return (std::max(x,y)+.05)/(std::min(x,y)+.05);}
int main(){
 assert(kUiThemeCount==7&&!validUiTheme(-1)&&!validUiTheme(7));
 assert(uiRgb565(0xff0000)==0xf800&&uiRgb565(0x0000ff)==0x001f);
 assert(uiPaletteRgb(0).bg==0xF6F2FB&&uiPaletteRgb(1).accent==0x356B52&&uiPaletteRgb(2).accent==0x985A3C);
 assert(uiPaletteRgb(999).bg==uiPaletteRgb(0).bg);
 for(int i=0;i<7;++i){assert(validUiTheme(i));assert(darkUiTheme(i)==(i>=3));}
 for(int i=3;i<7;++i){const auto p=uiPaletteRgb(i);
   assert(luminance(p.bg)<luminance(p.card)&&luminance(p.card)<luminance(p.soft));
   for(auto bg:{p.bg,p.card,p.soft})for(auto fg:{p.ink,p.muted,p.accent})assert(contrast(fg,bg)>=4.5);
   assert(contrast(0xFFB4AB,p.bg)>=4.5);
 }
}
