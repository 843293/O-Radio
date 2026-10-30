#pragma once
#include <vector>
inline std::vector<int> fakeDetachedPins;
inline void esp_rom_gpio_connect_out_signal(int pin,int,bool,bool){fakeDetachedPins.push_back(pin);}
