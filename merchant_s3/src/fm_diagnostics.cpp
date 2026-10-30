#include "fm_diagnostics.h"
#include "board_sts_6686n3.h"
#include <ArduinoJson.h>
#include <driver/pulse_cnt.h>
#include <esp_timer.h>
namespace {
void countPin(JsonObject out,int pin){
  out["gpio"]=pin;
  pcnt_unit_handle_t unit=nullptr;pcnt_channel_handle_t channel=nullptr;
  pcnt_unit_config_t uc{};uc.high_limit=32767;uc.low_limit=-32768;
  esp_err_t result=pcnt_new_unit(&uc,&unit);bool enabled=false,running=false;
  if(result==ESP_OK){pcnt_chan_config_t cc{};cc.edge_gpio_num=pin;cc.level_gpio_num=-1;result=pcnt_new_channel(unit,&cc,&channel);}
  if(result==ESP_OK)result=pcnt_channel_set_edge_action(channel,PCNT_CHANNEL_EDGE_ACTION_INCREASE,PCNT_CHANNEL_EDGE_ACTION_HOLD);
  if(result==ESP_OK)result=pcnt_channel_set_level_action(channel,PCNT_CHANNEL_LEVEL_ACTION_KEEP,PCNT_CHANNEL_LEVEL_ACTION_KEEP);
  if(result==ESP_OK){result=pcnt_unit_enable(unit);enabled=result==ESP_OK;}
  if(result==ESP_OK)result=pcnt_unit_clear_count(unit);
  if(result==ESP_OK){result=pcnt_unit_start(unit);running=result==ESP_OK;}
  int count=0;int64_t elapsed=0;
  if(result==ESP_OK){
    const int64_t start=esp_timer_get_time();delayMicroseconds(1000);
    result=pcnt_unit_stop(unit);running=result!=ESP_OK;elapsed=esp_timer_get_time()-start;
    if(result==ESP_OK)result=pcnt_unit_get_count(unit,&count);
  }
  if(running)pcnt_unit_stop(unit);
  if(enabled)pcnt_unit_disable(unit);
  if(channel)pcnt_del_channel(channel);
  if(unit)pcnt_del_unit(unit);
  out["error"]=int(result);out["valid"]=result==ESP_OK&&count>=0&&count<32760&&elapsed>0&&elapsed<8000;
  out["edges"]=count;out["elapsedUs"]=elapsed;
  if(result==ESP_OK&&elapsed>0)out["estimatedHz"]=int64_t(count)*1000000/elapsed;
}
}
String collectFmDiagnostics(RadioCore& radio,bool released,uint32_t frequency,bool muted){
  const auto r=radio.diagnostics();JsonDocument d;
  d["version"]="O-Radio 0.3.6";d["frequencyKhz"]=frequency;d["userMuted"]=muted;
  d["networkBusReleased"]=released;d["gpio45Level"]=digitalRead(board::kAmplifierMute);
  d["operationReadOk"]=r.operationValid;d["operationStatus"]=r.operation;
  d["processingReadOk"]=r.processingValid;d["softmuteTenthsPercent"]=r.softmute;d["highcutTenthsPercent"]=r.highcut;
  d["stereoBlendTenthsPercent"]=r.stereoBlend;d["qualityValid"]=r.quality.valid;
  d["qualityStatus"]=r.quality.status;d["levelTenthsDbuV"]=r.quality.level;
  d["usnTenthsPercent"]=r.quality.usn;d["wamTenthsPercent"]=r.quality.wam;d["offsetTenthsKhz"]=r.quality.offset;
  d["driverError"]=radio.state.error;
  if(released){
    countPin(d["bclk"].to<JsonObject>(),board::kI2sBclk);
    countPin(d["ws"].to<JsonObject>(),board::kI2sLrclk);
    countPin(d["data"].to<JsonObject>(),board::kI2sDout);
  }
  d["note"]="GPIO edges are measurements, not proof of DAC wiring or valid audio; BCLK/WS expected approx 3072000/48000 Hz only if physically shared.";
  String out;serializeJson(d,out);return out;
}
