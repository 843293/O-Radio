#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>
#include "board_sts_6686n3.h"
#include "audio_bus.h"
extern i2s_chan_handle_t networkAudioHandle();
namespace { bool channelStopped=false,pinsReleased=false; }
bool releaseNetworkAudioBus(){
  if(pinsReleased)return true;
  auto h=networkAudioHandle();
  if(!h)return false;
  if(!channelStopped){if(i2s_channel_disable(h)!=ESP_OK)return false;channelStopped=true;}
  bool ok=true;
  for(uint8_t pin:{board::kI2sBclk,board::kI2sLrclk,board::kI2sDout,board::kI2sMclk}){
    ok=(gpio_reset_pin(gpio_num_t(pin))==ESP_OK)&&ok;
    esp_rom_gpio_connect_out_signal(pin,SIG_GPIO_OUT_IDX,false,false);
    ok=(gpio_set_direction(gpio_num_t(pin),GPIO_MODE_INPUT)==ESP_OK)&&ok;
    ok=(gpio_set_pull_mode(gpio_num_t(pin),GPIO_FLOATING)==ESP_OK)&&ok;
  }
  pinsReleased=ok;return ok;
}
bool acquireNetworkAudioBus(){
  if(!channelStopped)return true;
  i2s_std_gpio_config_t cfg{};
  cfg.bclk=gpio_num_t(board::kI2sBclk);cfg.ws=gpio_num_t(board::kI2sLrclk);
  cfg.dout=gpio_num_t(board::kI2sDout);cfg.mclk=gpio_num_t(board::kI2sMclk);cfg.din=I2S_GPIO_UNUSED;
  if(i2s_channel_reconfig_std_gpio(networkAudioHandle(),&cfg)!=ESP_OK||i2s_channel_enable(networkAudioHandle())!=ESP_OK)return false;
  channelStopped=false;pinsReleased=false;return true;
}
