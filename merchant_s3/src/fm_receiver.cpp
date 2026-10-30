#include "fm_receiver.h"
#include <ArduinoJson.h>
namespace {
String frequencyName(uint32_t khz){char b[24];snprintf(b,sizeof b,"FM %.2f",double(khz)/1000);return b;}
String serializeState(const std::vector<FmPreset>& list,uint32_t khz){
  JsonDocument d;d["frequencyKhz"]=khz;auto a=d["presets"].to<JsonArray>();
  for(const auto& p:list){auto o=a.add<JsonObject>();o["frequencyKhz"]=p.khz;o["name"]=p.name;}
  String result;serializeJson(d,result);return result;
}
}
void FmReceiver::begin(){
  storage_=prefs_.begin("o-fm",false);
  if(!storage_){error="FM 配置存储不可用";return;}
  const String saved=prefs_.getString("state","");if(saved.isEmpty())return;
  JsonDocument d;if(deserializeJson(d,saved)){error="FM 配置损坏，未覆盖原数据";return;}
  const uint32_t f=d["frequencyKhz"]|99500u;
  auto a=d["presets"].as<JsonArray>();
  if(!fmFrequencyValid(f)||a.isNull()||a.size()>64){error="FM 配置无效";return;}
  std::vector<FmPreset> next;
  for(auto o:a){const uint32_t khz=o["frequencyKhz"]|0u;const String name=o["name"]|"";
    if(!fmFrequencyValid(khz)||name.length()>64){error="FM 收藏数据无效";return;}
    for(const auto& p:next)if(p.khz==khz){error="FM 收藏频率重复";return;}
    next.push_back({khz,name});}
  presets=next;frequency=f;
}
bool FmReceiver::persist(const std::vector<FmPreset>& list,uint32_t khz){
  const String data=serializeState(list,khz);
  if(!storage_||prefs_.putString("state",data)!=data.length()){error="FM 配置保存失败";return false;}
  return true;
}
bool FmReceiver::enter(int percent){
  volume=constrain(percent,0,100);
  if(!core_.selectFm(frequency,volume,muted)){error=core_.state.error;return false;}
  active=true;error="";return true;
}
bool FmReceiver::leave(){
  if(scanning){scanning=false;found_.clear();peaks_.clear();message="搜台已取消，保留原列表";}
  if(!core_.selectNetwork()){error=core_.state.error;return false;}
  active=false;return true;
}
bool FmReceiver::hardwareTune(uint32_t khz,bool silence){
  if(!core_.configure(true,khz,fmVolumeDb(volume),silence||volume==0,0)){error=core_.state.error;return false;}
  return true;
}
bool FmReceiver::tune(uint32_t khz){
  if(!active||scanning||!fmFrequencyValid(khz)){error="请先切换 FM 或结束搜台";return false;}
  const uint32_t old=frequency;
  if(!hardwareTune(khz,muted))return false;
  if(!persist(presets,khz)){hardwareTune(old,muted);return false;}
  frequency=khz;error="";return true;
}
bool FmReceiver::step(int direction){
  if(presets.empty()){message="尚无收藏，长按 A 搜台或网页添加";return false;}
  if(scanning)return false;
  int index=-1;for(size_t i=0;i<presets.size();++i)if(presets[i].khz==frequency)index=i;
  const int n=presets.size();if(index<0)index=direction>0?-1:0;
  index=((index+direction%n)%n+n)%n;return tune(presets[index].khz);
}
bool FmReceiver::setVolume(int percent,bool silence){
  if(percent<0||percent>100)return false;
  if(active&&!core_.setFmVolume(percent,silence||scanning)){error=core_.state.error;return false;}
  volume=percent;muted=silence;return true;
}
bool FmReceiver::startScan(){
  if(!active||scanning||!core_.state.ready){error="FM 尚未就绪或正在搜台";return false;}
  if(!storage_){error="无法保存搜台结果";return false;}
  found_.clear();peaks_.clear();scanFrequency=65000;progress=0;sampleStage_=0;
  if(!hardwareTune(scanFrequency,true))return false;
  due_=millis()+110;scanning=true;message="正在扫描 65-108 MHz";error="";return true;
}
void FmReceiver::cancelScan(){
  if(!scanning)return;
  scanning=false;found_.clear();peaks_.clear();
  hardwareTune(frequency,muted);message="搜台已取消，保留原列表";
}
void FmReceiver::finishScan(){
  scanning=false;
  found_.clear();
  for(unsigned i=0;i<peaks_.count;++i)found_.push_back({peaks_.entries[i].khz,frequencyName(peaks_.entries[i].khz)});
  if(found_.empty()){hardwareTune(frequency,muted);message="未找到有效电台，保留原列表";return;}
  const uint32_t old=frequency,target=found_.front().khz;
  if(!hardwareTune(target,muted))return;
  if(!persist(found_,target)){hardwareTune(old,muted);return;}
  presets=found_;frequency=target;progress=100;message="搜台完成，已保存 "+String(presets.size())+" 台";
  found_.clear();peaks_.clear();error="";
}
void FmReceiver::tick(){
  if(!active)return;
  if(!scanning){core_.poll();return;}
  if(int32_t(millis()-due_)<0)return;
  if(sampleStage_==0){first_=core_.sampleFmQuality();sampleStage_=1;due_=millis()+25;return;}
  const auto q=core_.sampleFmQuality();sampleStage_=0;
  if(!core_.state.ready||!q.valid||!first_.valid){
    if(!q.valid&&!first_.valid){error="质量读取失败，搜台已中止";cancelScan();return;}
  }
  if(fmSignalCandidate(first_)&&fmSignalCandidate(q))peaks_.add(scanFrequency,(first_.level+q.level)/2);
  progress=(scanFrequency-65000)*100/(108000-65000);
  if(scanFrequency>=108000){finishScan();return;}
  scanFrequency+=100;
  if(!hardwareTune(scanFrequency,true)){scanning=false;hardwareTune(frequency,muted);return;}
  due_=millis()+110;
}
bool FmReceiver::add(uint32_t khz,const String& name){
  if(scanning||!fmFrequencyValid(khz)||name.length()>64){error="收藏参数无效或正在搜台";return false;}
  auto next=presets;bool replaced=false;
  for(auto& p:next)if(p.khz==khz){p.name=name.isEmpty()?frequencyName(khz):name;replaced=true;break;}
  if(!replaced){if(next.size()>=64){error="最多 64 个 FM 收藏";return false;}next.push_back({khz,name.isEmpty()?frequencyName(khz):name});}
  if(!persist(next,frequency))return false;
  presets=next;error="";return true;
}
bool FmReceiver::remove(uint32_t khz){
  if(scanning){error="请先结束搜台";return false;}
  std::vector<FmPreset> next;for(const auto& p:presets)if(p.khz!=khz)next.push_back(p);
  if(!persist(next,frequency))return false;
  presets=next;error="";return true;
}
String FmReceiver::json() const{
  JsonDocument d;d["frequencyKhz"]=frequency;d["scanFrequencyKhz"]=scanFrequency;
  d["volume"]=volume;d["gainDb"]=fmVolumeDb(volume);d["active"]=active;d["scanning"]=scanning;d["muted"]=muted;d["progress"]=progress;
  d["levelTenths"]=core_.state.levelTenths;d["qualityValid"]=active&&!scanning&&core_.state.qualityValid;
  d["stereo"]=core_.state.stereo;d["found"]=peaks_.count;d["message"]=message;d["error"]=error;
  auto a=d["presets"].to<JsonArray>();for(const auto& p:presets){auto o=a.add<JsonObject>();o["frequencyKhz"]=p.khz;o["name"]=p.name;}
  String out;serializeJson(d,out);return out;
}
