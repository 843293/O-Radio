#pragma once
#include <stdint.h>
struct UiPaletteRgb {uint32_t bg,card,soft,accent,ink,muted;};
constexpr int kUiThemeCount=7;
inline bool validUiTheme(int id){return id>=0&&id<kUiThemeCount;}
inline bool darkUiTheme(int id){return id>=3&&id<kUiThemeCount;}
inline uint16_t uiRgb565(uint32_t c){return ((c>>8)&0xF800)|((c>>5)&0x07E0)|((c>>3)&0x001F);}
inline UiPaletteRgb uiPaletteRgb(int id){
  switch(id){
    case 1:return {0xF0F7F1,0xD4EBDD,0xE0EAE3,0x356B52,0x19382B,0x52685A};
    case 2:return {0xFFF3EC,0xFADBCB,0xF2E4DA,0x985A3C,0x442C23,0x7D6254};
    case 3:return {0x0E1520,0x1B293B,0x253449,0xABC8F5,0xE7EEF9,0xA9B8CF};
    case 4:return {0x14111D,0x272133,0x322A40,0xD1B6F4,0xF1EAF8,0xBDB0CB};
    case 5:return {0x0E1713,0x1B2C23,0x25362D,0xA4D7BA,0xDDECE5,0xA0B6AA};
    case 6:return {0x19140F,0x30261C,0x3C3023,0xF2C58D,0xF6EBDD,0xC6B49D};
    default:return {0xF6F2FB,0xE5DCF5,0xEAE4EF,0x715798,0x30263E,0x71657D};
  }
}
