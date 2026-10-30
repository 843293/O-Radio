#pragma once
#include <vector>
using gpio_num_t=int;
constexpr int ESP_OK=0,GPIO_MODE_INPUT=1,GPIO_FLOATING=0;
inline bool fakePinFailure=false;
inline std::vector<int> fakeReleasedPins;
inline int gpio_reset_pin(int p){fakeReleasedPins.push_back(p);return fakePinFailure?-1:0;}
inline int gpio_set_direction(int,int){return fakePinFailure?-1:0;}
inline int gpio_set_pull_mode(int,int){return fakePinFailure?-1:0;}
