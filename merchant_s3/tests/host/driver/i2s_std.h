#pragma once
#include "gpio.h"
using i2s_chan_handle_t=void*;
constexpr int I2S_GPIO_UNUSED=-1;
struct i2s_std_gpio_config_t {int mclk,bclk,ws,dout,din;};
inline int fakeDisableCalls=0,fakeEnableCalls=0,fakeConfigCalls=0;
inline bool fakeDisableFailure=false,fakeConfigFailure=false,fakeEnableFailure=false;
inline i2s_std_gpio_config_t fakeRestoredPins{};
inline int i2s_channel_disable(void*){++fakeDisableCalls;return fakeDisableFailure?-1:0;}
inline int i2s_channel_enable(void*){++fakeEnableCalls;return fakeEnableFailure?-1:0;}
inline int i2s_channel_reconfig_std_gpio(void*,const i2s_std_gpio_config_t* c){++fakeConfigCalls;fakeRestoredPins=*c;return fakeConfigFailure?-1:0;}
