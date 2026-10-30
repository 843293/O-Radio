#include <driver/i2s_std.h>
#include <esp_timer.h>
#include <cstring>
#include "pcm_bridge.h"

extern "C" {
esp_err_t __real_i2s_channel_init_std_mode(i2s_chan_handle_t,const i2s_std_config_t*);
esp_err_t __real_i2s_channel_reconfig_std_clock(i2s_chan_handle_t,const i2s_std_clk_config_t*);
esp_err_t __real_i2s_channel_reconfig_std_slot(i2s_chan_handle_t,const i2s_std_slot_config_t*);
esp_err_t __real_i2s_channel_write(i2s_chan_handle_t,const void*,size_t,size_t*,uint32_t);
}
namespace { i2s_chan_handle_t audioHandle=nullptr; }
extern "C" esp_err_t __wrap_i2s_channel_init_std_mode(i2s_chan_handle_t h,const i2s_std_config_t* cfg) {
  if(!cfg)return ESP_ERR_INVALID_ARG;
  auto adjusted=*cfg;
  adjusted.slot_cfg=I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_STEREO);
  adjusted.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
  auto result=__real_i2s_channel_init_std_mode(h,&adjusted);
  if(result==ESP_OK)audioHandle=h;
  return result;
}
extern "C" esp_err_t __wrap_i2s_channel_reconfig_std_clock(i2s_chan_handle_t h,const i2s_std_clk_config_t* cfg) {
  if(h!=audioHandle)return __real_i2s_channel_reconfig_std_clock(h,cfg);
  if(!cfg)return ESP_ERR_INVALID_ARG;
  auto adjusted=*cfg;adjusted.mclk_multiple=I2S_MCLK_MULTIPLE_256;
  return __real_i2s_channel_reconfig_std_clock(h,&adjusted);
}
extern "C" esp_err_t __wrap_i2s_channel_reconfig_std_slot(i2s_chan_handle_t h,const i2s_std_slot_config_t* cfg) {
  if(h!=audioHandle)return __real_i2s_channel_reconfig_std_slot(h,cfg);
  if(!cfg)return ESP_ERR_INVALID_ARG;
  i2s_std_slot_config_t adjusted=I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_STEREO);
  return __real_i2s_channel_reconfig_std_slot(h,&adjusted);
}
extern "C" esp_err_t __wrap_i2s_channel_write(i2s_chan_handle_t h,const void* src,size_t size,size_t* written,uint32_t timeout) {
  if(h!=audioHandle)return __real_i2s_channel_write(h,src,size,written,timeout);
  if(written)*written=0;
  if(!src||!written||size%4)return ESP_ERR_INVALID_ARG;
  const auto* bytes=static_cast<const uint8_t*>(src);
  const int64_t deadline=esp_timer_get_time()+int64_t(timeout)*1000;
  alignas(4) int32_t expanded[128];
  size_t consumed=0;
  while(consumed<size){
    const size_t chunk=(size-consumed>256)?256:size-consumed;
    for(size_t i=0;i<chunk/2;++i){
      int16_t sample;memcpy(&sample,bytes+consumed+i*2,2);
      expanded[i]=pcm16To32(sample);
    }
    const int64_t remaining=deadline-esp_timer_get_time();
    const uint32_t wait=remaining>0?uint32_t((remaining+999)/1000):0;
    size_t output=0;
    auto result=__real_i2s_channel_write(h,expanded,chunk*2,&output,wait);
    // The decoder counts input PCM16 bytes, not the expanded bus bytes.
    consumed+=output/2;*written=consumed;
    if(result!=ESP_OK)return result;
    if(output!=chunk*2)return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}

i2s_chan_handle_t networkAudioHandle(){return audioHandle;}
