#pragma once
#include "ui_palette.h"
class ThemePicker {
 public:
  bool active=false;
  int initial=0,selected=0;
  void open(int current){initial=selected=validUiTheme(current)?current:0;active=true;}
  int step(int delta){selected=((selected+delta)%kUiThemeCount+kUiThemeCount)%kUiThemeCount;return selected;}
  int cancel(){active=false;return initial;}
  void saved(){active=false;}
};
inline const char* uiThemeName(int id){
  static const char* names[]={"浅紫","薄荷绿","蜜桃","午夜蓝","暮紫","深林绿","暖墨琥珀"};
  return names[validUiTheme(id)?id:0];
}
