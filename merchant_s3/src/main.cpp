#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <FS.h>
using fs::FS;
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "board_sts_6686n3.h"
#include "radio_core.h"
#include "network_player.h"
#include "station_store.h"
#include "web_ui.h"
#include "cjk_font.h"
#include "lcd_utf8.h"
#include "alarm_logic.h"
#include "button_click.h"
#include "alarm_button.h"
#include "alarm_editor.h"
#include "fm_receiver.h"
#include "fm_diagnostics.h"
#include "display_idle.h"
#include "auto_sleep.h"
#include "ui_palette.h"
#include "theme_picker.h"
#include "battery_monitor.h"
#include "sleep_plan.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <esp_system.h>
#include <type_traits>
#include <time.h>
namespace {
constexpr char kVersion[]="O-Radio 0.3.12";
RadioCore audioPath;
FmReceiver fm(audioPath);
bool networkWantedBeforeFm=false;
String modeError;
AlarmButton buttonA;
NetworkPlayer player;
StationStore stations;
TFT_eSPI screen;
WebServer server(80);
Preferences settings;
bool storageReady=false,otaBusy=false,otaGood=false;
String accessKey,apName,otaError;
uint32_t saveAt=0,rebootAt=0,lastDraw=0,stationRevision=1;
uint32_t selectedId=0;
bool autoPlay=false;
int themeId=0;
ThemePicker themePicker;
String themePickerError;
void closeThemePicker(bool save);
bool infoPage=false;
ButtonClick buttonB;
DisplayIdle displayIdle;
AutoSleep autoSleep;
BatteryMonitor battery;
bool anyButtonHeld=false;
void noteDisplayActivity(uint32_t now=millis()){
  autoSleep.activity(now);
  if(displayIdle.activity(now)){buttonA=AlarmButton();buttonB=ButtonClick();}
  digitalWrite(board::kTftBacklight,HIGH);
}
struct SleepRecall {
  uint32_t magic;
  bool fmMode,networkWanted,netMuted,fmMuted;
  int netVolume,fmVolume;
  int64_t alarmTarget,sleepEpoch;
  uint64_t timerUs;
};
static_assert(std::is_trivial<SleepRecall>::value,"RTC state must not have a boot-time constructor");
RTC_DATA_ATTR SleepRecall rtcSleepRecall{};
SleepRecall wakeRecall{};
bool deepWake=false,timerWake=false,releaseKeysGate=false;
bool wakeAlarmPending=false;
int rawWakeCause=0;
int64_t bootWakeEpoch=0;
time_t wakeAlarmTarget=0;
String sleepHistory;
bool deepSleepPending=false;
uint32_t deepRequestAt=0,deepReleasedAt=0,gateReleaseAt=0;
time_t deepAlarmTarget=0;
String deepSleepError;
bool requestDeepSleep();
void cancelDeepSleep();
void serviceDeepSleep();
RadioAlarm alarmConfig;
String alarmError;
bool alarmPage=false,alarmPreview=false;
RadioAlarm alarmDraft;
int alarmField=0;
String alarmEditError;
uint32_t previewOldId=0;
bool previewOldFm=false,previewOldWanted=false,previewOldMuted=false;
int previewOldVolume=50;
void endAlarmPreview();

uint32_t lastAlarmCheck=0;
bool localClock(tm& result){const time_t now=time(nullptr);return localtime_r(&now,&result)&&radioClockValid(result);}
String beijingTime(){tm t{};if(!localClock(t))return "等待联网校时";char b[24];strftime(b,sizeof b,"%Y-%m-%d %H:%M:%S",&t);return b;}
String alarmJson(const RadioAlarm& a){
  JsonDocument d;d["enabled"]=a.enabled;d["hour"]=a.hour;d["minute"]=a.minute;
  d["days"]=a.days;d["volume"]=a.volume;d["stationId"]=a.stationId;d["lastDay"]=a.lastDay;
  String out;serializeJson(d,out);return out;
}
bool persistAlarm(const RadioAlarm& a){
  if(!storageReady)return false;
  const String value=alarmJson(a);return settings.putString("alarm",value)==value.length();
}
void loadAlarm(){
  if(!storageReady)return;
  JsonDocument d;if(deserializeJson(d,settings.getString("alarm","{}")))return;
  const int h=d["hour"]|7,m=d["minute"]|0,days=d["days"]|127,volume=d["volume"]|50;
  if(h<0||h>23||m<0||m>59||days<0||days>127||volume<1||volume>100)return;
  alarmConfig.enabled=d["enabled"]|false;alarmConfig.hour=h;alarmConfig.minute=m;
  alarmConfig.days=days;alarmConfig.volume=volume;alarmConfig.stationId=d["stationId"]|0u;
  alarmConfig.lastDay=d["lastDay"]|0;
}
int activeVolume(){return fm.active?fm.volume:player.volume();}
bool playSelected();
bool switchMode(bool toFm){
  if(toFm==fm.active)return true;
  if(otaBusy){modeError="正在更新，不能切换模式";return false;}
  digitalWrite(board::kAmplifierMute,HIGH);
  if(toFm){
    networkWantedBeforeFm=player.snapshot().wanted;
    if(!player.quiesce()){digitalWrite(board::kAmplifierMute,LOW);modeError="网络播放器未能暂停";return false;}
    // quiesce() now stops clocks and releases GPIO before TEF becomes master.
    if(!player.audioBusReleased()){modeError="I2S 总线未释放";return false;}
    if(!fm.enter(fm.volume)){
      modeError=fm.error;
      if(audioPath.selectNetwork()){player.resume();digitalWrite(board::kAmplifierMute,LOW);}
      return false;
    }
  }else{
    if(!fm.leave()){modeError=fm.error;return false;}
    player.resume();
    if(networkWantedBeforeFm)playSelected();else player.stop();
  }
  infoPage=false;modeError="";digitalWrite(board::kAmplifierMute,LOW);return true;
}
void resumeAfterOta(){if(!fm.active)player.resume();}
portMUX_TYPE encoderMux=portMUX_INITIALIZER_UNLOCKED;
volatile int32_t encoderCounts[2]{};
volatile uint8_t encoderHistory[2]{};
DRAM_ATTR const int8_t kQuadrature[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
void ARDUINO_ISR_ATTR encoderA() {
  const uint8_t bits=(digitalRead(board::kEncoderAClock)<<1)|digitalRead(board::kEncoderAData);
  portENTER_CRITICAL_ISR(&encoderMux);
  encoderHistory[0]=((encoderHistory[0]<<2)|bits)&15;
  encoderCounts[0]+=kQuadrature[encoderHistory[0]];
  portEXIT_CRITICAL_ISR(&encoderMux);
}
void ARDUINO_ISR_ATTR encoderB() {
  const uint8_t bits=(digitalRead(board::kEncoderBClock)<<1)|digitalRead(board::kEncoderBData);
  portENTER_CRITICAL_ISR(&encoderMux);
  encoderHistory[1]=((encoderHistory[1]<<2)|bits)&15;
  encoderCounts[1]+=kQuadrature[encoderHistory[1]];
  portEXIT_CRITICAL_ISR(&encoderMux);
}
String quote(const String& value) {
  String out="\"";
  for (size_t i=0;i<value.length();++i) {
    const uint8_t c=value[i];
    if(c=='"'||c=='\\') {out+='\\';out+=char(c);}
    else if(c<32) {char buf[7];snprintf(buf,sizeof buf,"\\u%04x",c);out+=buf;}
    else out+=char(c);
  }
  return out+'"';
}
void json(int status,const String& body) {
  server.sendHeader("Cache-Control","no-store");
  server.send(status,"application/json; charset=utf-8",body);
}
void error(int status,const String& message) {json(status,"{\"error\":"+quote(message)+"}");}
bool authorized() {
  if(server.authenticate("admin",accessKey.c_str())) {
    if(server.method()==HTTP_POST && server.header("X-Radio-Control")!="1") {
      error(403,"Missing control header");return false;
    }
    if(deepSleepPending&&server.method()==HTTP_POST&&server.uri()!="/api/power/sleep"&&server.uri()!="/api/power/cancel"){
      error(409,"Deep sleep pending; cancel it first");return false;
    }
    if((alarmPage||themePicker.active)&&server.method()==HTTP_POST){error(409,"Close the on-device settings page first");return false;}
    if(server.method()==HTTP_POST)noteDisplayActivity();
    return true;
  }
  server.requestAuthentication();return false;
}
bool number(const String& s,int32_t& value) {
  if(s.isEmpty()||s.length()>10)return false;
  size_t i=s[0]=='-'?1:0;
  if(i==s.length())return false;
  for(;i<s.length();++i)if(!isDigit(s[i]))return false;
  const int64_t n=strtoll(s.c_str(),nullptr,10);
  if(n<INT32_MIN||n>INT32_MAX)return false;
  value=int32_t(n);return true;
}
void status() {
  if(!authorized())return;
  const auto p=player.snapshot();const Station* station=stations.find(selectedId);
  JsonDocument d;
  d["wakeReason"]=timerWake?"alarm_timer":deepWake?"button":"cold_or_reset";
  d["wakeCauseRaw"]=rawWakeCause;d["bootWakeEpoch"]=bootWakeEpoch;d["wakeAlarmPending"]=wakeAlarmPending;d["wakeAlarmTarget"]=int64_t(wakeAlarmTarget);
  if(!sleepHistory.isEmpty())d["lastSleepPlan"]=serialized(sleepHistory);
  d["deepSleepPending"]=deepSleepPending;d["deepSleepError"]=deepSleepError;d["deepAlarmTarget"]=int64_t(deepAlarmTarget);
  d["displayAsleep"]=displayIdle.asleep();d["autoSleepMinutes"]=30;d["autoSleepIdleMs"]=autoSleep.idleMs(millis());
  auto bat=d["battery"].to<JsonObject>();bat["valid"]=battery.valid;bat["estimated"]=true;bat["chargingKnown"]=false;bat["raw"]=battery.raw;
  if(battery.valid){bat["millivolts"]=battery.millivolts;bat["percent"]=battery.percent;}
  d["mode"]=fm.active?"fm":"network";d["fm"]=serialized(fm.json());d["modeError"]=modeError;
  d["theme"]=themeId;d["themePickerOpen"]=themePicker.active;d["beijingTime"]=beijingTime();tm clock{};d["timeValid"]=localClock(clock);
  d["alarmEditorOpen"]=alarmPage;d["alarmPreview"]=alarmPreview;d["alarm"]=serialized(alarmJson(alarmConfig));d["alarmError"]=alarmError;
  d["version"]=kVersion;d["audioReady"]=audioPath.state.ready;
  d["phase"]=p.phase;d["wanted"]=p.wanted;d["playingId"]=p.stationId;d["selectedId"]=selectedId;
  d["stationName"]=station?station->name:"";d["title"]=p.title;d["codec"]=p.codec;
  d["bitrate"]=p.bitrate;d["buffered"]=p.buffered;d["retries"]=p.retries;
  d["i2sFormat"]="LSBJ (merchant)";d["i2sBitShift"]=0;d["i2sBits"]=32;d["i2sMclkMultiple"]=256;d["volumeControl"]=fm.active?"TEF processor":"PCM";d["networkBusReleased"]=player.audioBusReleased();
  d["audioClockOwner"]=fm.active?"TEF":"ESP32";
  d["volume"]=activeVolume();d["networkVolume"]=player.volume();d["muted"]=fm.active?fm.muted:player.muted();d["autoplay"]=autoPlay;
  d["wifiConnected"]=WiFi.status()==WL_CONNECTED;d["ssid"]=WiFi.SSID();
  d["ip"]=WiFi.localIP().toString();d["ap"]=apName;d["rssi"]=WiFi.RSSI();
  d["error"]=p.error;d["audioError"]=audioPath.state.error;d["storageError"]=stations.error;
  d["storageReady"]=stations.mounted&&storageReady;d["stationCount"]=stations.entries.size();
  d["uptime"]=millis()/1000;d["freeHeap"]=ESP.getFreeHeap();d["freePsram"]=ESP.getFreePsram();
  String text;serializeJson(d,text);json(200,text);
}
bool playSelected() {
  const Station* s=stations.find(selectedId);return s&&player.play(s->id,s->url);
}
void stepStation(int direction,bool play) {
  if(stations.entries.empty())return;
  int index=-1;
  for(size_t i=0;i<stations.entries.size();++i)if(stations.entries[i].id==selectedId)index=i;
  const int count=stations.entries.size();
  index=((index<0?0:index)+direction%count+count)%count;
  selectedId=stations.entries[index].id;
  if(play)playSelected();
  saveAt=millis()+3000;
}
void control() {
  if(!authorized())return;
  if(otaBusy){error(409,"Firmware update in progress");return;}
  int32_t volume=activeVolume(),muted=fm.active?fm.muted:player.muted(),id=selectedId;
  bool nextAuto=autoPlay;
  if(server.hasArg("id")&&(!number(server.arg("id"),id)||id<1||!stations.find(id))){error(400,"Invalid station ID");return;}
  if(server.hasArg("volume")&&(!number(server.arg("volume"),volume)||volume<0||volume>100)){error(400,"Invalid volume");return;}
  if(server.hasArg("muted")&&(!number(server.arg("muted"),muted)||muted<0||muted>1)){error(400,"Invalid mute");return;}
  if(server.hasArg("autoplay")){int32_t v;if(!number(server.arg("autoplay"),v)||v<0||v>1){error(400,"Invalid autoplay");return;}nextAuto=v;}
  const String action=server.arg("action");
  if(!action.isEmpty()&&action!="play"&&action!="stop"&&action!="next"&&action!="previous"){error(400,"Invalid action");return;}
  if(!action.isEmpty()&&fm.active){
    if(!switchMode(false)){error(503,modeError);return;}
    if(!server.hasArg("muted"))muted=player.muted();
    if(!server.hasArg("volume"))volume=player.volume();
  }
  if(action=="play"){
    const Station* s=stations.find(id);
    if(!s){error(400,"Add a station first");return;}
    if(!player.play(s->id,s->url)){error(503,"Audio player unavailable");return;}
  }
  if(fm.active&&!fm.setVolume(volume,muted)){error(503,fm.error);return;}
  selectedId=id;if(!fm.active)player.setVolume(volume);
  if(!fm.active)player.setMuted(muted);
  autoPlay=nextAuto;
  if(action=="stop")player.stop();
  if(action=="next"||action=="previous")stepStation(action=="next"?1:-1,true);
  saveAt=millis()+3000;status();
}
void sendStations() {
  if(!authorized())return;
  json(200,"{\"revision\":"+String(stationRevision)+",\"stations\":"+stations.json()+"}");
}
void replaceStations() {
  if(!authorized())return;
  if(otaBusy){error(409,"Firmware update in progress");return;}
  if(server.header("X-Station-Revision")!=String(stationRevision)){error(409,"List changed; reload before saving");return;}
  const uint32_t playing=player.snapshot().stationId;
  String oldUrl;const Station* old=stations.find(playing);if(old)oldUrl=old->url;
  if(!stations.replace(server.arg("plain"))){error(400,stations.error);return;}
  ++stationRevision;
  const Station* active=stations.find(playing);
  if(!active||active->url!=oldUrl)player.stop();
  if(!stations.find(selectedId))selectedId=stations.entries.empty()?0:stations.entries[0].id;
  saveAt=millis()+3000;sendStations();
}
void saveAlarm() {
  if(!authorized())return;
  if(otaBusy){error(409,"Firmware update in progress");return;}
  int32_t enabled,hour,minute,days,volume,id;
  if(!number(server.arg("enabled"),enabled)||enabled<0||enabled>1||
     !number(server.arg("hour"),hour)||hour<0||hour>23||
     !number(server.arg("minute"),minute)||minute<0||minute>59||
     !number(server.arg("days"),days)||days<0||days>127||
     !number(server.arg("volume"),volume)||volume<1||volume>100||
     !number(server.arg("stationId"),id)||id<0||(enabled&&!stations.find(id))){error(400,"Invalid alarm settings or station");return;}
  RadioAlarm next=alarmConfig;next.enabled=enabled;next.hour=hour;next.minute=minute;
  next.days=days;next.volume=volume;next.stationId=id;
  // A changed schedule may ring again today; unchanged saves must not repeat.
  if(radioAlarmScheduleChanged(alarmConfig,next))next.lastDay=0;
  if(!persistAlarm(next)){error(500,"Cannot save alarm");return;}
  alarmConfig=next;wakeAlarmPending=false;alarmError="";status();
}
void checkAlarm(){
  if(otaBusy||deepSleepPending||millis()-lastAlarmCheck<500)return;
  lastAlarmCheck=millis();tm t{};
  if(!localClock(t))return;
  const time_t now=time(nullptr);
  if(wakeAlarmPending&&now>wakeAlarmTarget+600){wakeAlarmPending=false;alarmError="唤醒后联网超时，错过闹钟";}
  const bool scheduledWake=wakeAlarmPending&&radioWakeAlarmDue(wakeAlarmTarget,now)&&alarmConfig.enabled&&alarmConfig.lastDay!=radioDayKey(t);
  if(!scheduledWake&&!radioAlarmDue(alarmConfig,t))return;
  const Station* station=stations.find(alarmConfig.stationId);
  if(!station){wakeAlarmPending=false;alarmError="闹钟电台已删除，请重新选择";return;}
  if(WiFi.status()!=WL_CONNECTED){alarmError=wakeAlarmPending?"定时唤醒后等待 Wi-Fi，最多补响十分钟":"闹钟等待 Wi-Fi，当前分钟内重试";return;}
  // Persist the trigger before playback, preventing duplicate wakeups after reset.
  if(themePicker.active)closeThemePicker(false);
  if(alarmPage){endAlarmPreview();alarmPage=false;}
  RadioAlarm fired=alarmConfig;fired.lastDay=radioDayKey(t);
  if(fired.days==0)fired.enabled=false;
  if(!persistAlarm(fired)){alarmError="闹钟记录保存失败，未启动播放";return;}
  alarmConfig=fired;wakeAlarmPending=false;
  noteDisplayActivity();
  if(fm.active&&!switchMode(false)){alarmError=modeError;return;}
  player.setVolume(alarmConfig.volume);player.setMuted(false);
  if(!player.play(station->id,station->url)){alarmError="闹钟无法启动播放器";return;}
  selectedId=station->id;alarmError="";saveAt=millis()+3000;
}
void fmControl(){
  if(!authorized())return;
  if(otaBusy){error(409,"Firmware update in progress");return;}
  const String action=server.arg("action");
  if(action=="mode"){
    const String mode=server.arg("mode");if(mode!="fm"&&mode!="network"){error(400,"Invalid mode");return;}
    if(!switchMode(mode=="fm")){error(503,modeError);return;}
  }else if(action=="scan"){
    if(!switchMode(true)||!fm.startScan()){error(503,fm.error.isEmpty()?modeError:fm.error);return;}
    infoPage=false;
  }else if(action=="cancel")fm.cancelScan();
  else if(action=="mute"){
    int32_t mute;if(!number(server.arg("muted"),mute)||mute<0||mute>1){error(400,"Invalid mute");return;}
    if(!fm.active||!fm.setVolume(fm.volume,mute)){error(503,fm.error);return;}
  }else if(action=="tune"||action=="add"||action=="delete"){
    int32_t f;if(!number(server.arg("frequencyKhz"),f)||!fmFrequencyValid(f)){error(400,"Invalid FM frequency");return;}
    if(action=="tune"){
      if(!switchMode(true)||!fm.tune(f)){error(503,fm.error.isEmpty()?modeError:fm.error);return;}
    }else if(action=="add"){
      if(!fm.add(f,server.arg("name"))){error(400,fm.error);return;}
    }else if(!fm.remove(f)){error(400,fm.error);return;}
  }else {error(400,"Invalid FM action");return;}
  status();
}
String fmDiagnostics(){
  if(!fm.active)return "{\"error\":\"Switch to FM first\"}";
  if(fm.scanning)return "{\"error\":\"Stop scanning before diagnosis\"}";
  if(!player.audioBusReleased())return "{\"error\":\"Network I2S bus not released\"}";
  return collectFmDiagnostics(audioPath,player.audioBusReleased(),fm.frequency,fm.muted);
}
bool requestDeepSleep(){
  if(wakeAlarmPending){deepSleepError="闹钟唤醒任务未完成，暂不深睡";return false;}
  if(otaBusy){deepSleepError="正在更新固件，不能深睡";return false;}
  if(deepSleepPending)return true;
  deepAlarmTarget=0;deepSleepError="";
  if(alarmConfig.enabled){
    tm t{};const time_t now=time(nullptr);
    if(!localClock(t)){deepSleepError="闹钟已启用，请先联网校时再深睡";return false;}
    if(!stations.find(alarmConfig.stationId)||settings.getString("ssid","").isEmpty()){
      deepSleepError="请先配置闹钟电台和 Wi-Fi";return false;
    }
    deepAlarmTarget=nextRadioAlarmEpoch(alarmConfig,now);
    if(!deepAlarmTarget||deepAlarmTarget-now<=5||radioAlarmDue(alarmConfig,t)){
      deepSleepError="闹钟即将触发或时间无效，暂不深睡";return false;
    }
  }
  deepSleepPending=true;deepRequestAt=millis();deepReleasedAt=0;
  noteDisplayActivity();return true;
}
void cancelDeepSleep(){
  deepSleepPending=false;deepReleasedAt=0;deepSleepError="";
  buttonA=AlarmButton();buttonB=ButtonClick();releaseKeysGate=true;gateReleaseAt=0;noteDisplayActivity();
}
void setupWeb() {
  const char* headers[]={"X-Radio-Control","X-Station-Revision"};server.collectHeaders(headers,2);
  server.on("/",HTTP_GET,[]{if(authorized())server.send_P(200,"text/html; charset=utf-8",kWebUi);});
  server.on("/api/status",HTTP_GET,status);
  server.on("/api/control",HTTP_POST,control);
  server.on("/api/alarm",HTTP_POST,saveAlarm);
  server.on("/api/display/wake",HTTP_POST,[]{if(authorized())status();});
  server.on("/api/power/sleep",HTTP_POST,[]{
    if(!authorized())return;
    if(!requestDeepSleep()){error(409,deepSleepError);return;}
    json(200,"{\"ok\":true,\"pending\":true}");
  });
  server.on("/api/power/cancel",HTTP_POST,[]{if(authorized()){cancelDeepSleep();json(200,"{\"ok\":true}");}});
  server.on("/api/fm",HTTP_POST,fmControl);
  server.on("/api/fm/diagnostics",HTTP_POST,[]{
    if(!authorized())return;
    if(otaBusy){error(409,"Firmware update in progress");return;}
    if(!fm.active||fm.scanning||!player.audioBusReleased()){error(409,"Enter FM and stop scanning first");return;}
    const String data=fmDiagnostics();Serial.println(data);json(200,data);
  });
  server.on("/api/theme",HTTP_POST,[]{
    if(!authorized())return;
    if(otaBusy){error(409,"Firmware update in progress");return;}
    int32_t next;
    if(!number(server.arg("theme"),next)||!validUiTheme(next)){error(400,"Invalid theme");return;}
    if(!storageReady||settings.putInt("theme",next)!=sizeof(int32_t)){error(500,"Cannot save theme");return;}
    themeId=next;status();
  });
  server.on("/api/stations",HTTP_GET,sendStations);
  server.on("/api/stations",HTTP_POST,replaceStations);
  server.on("/api/wifi",HTTP_POST,[]{
    if(!authorized())return;
    if(otaBusy){error(409,"Firmware update in progress");return;}
    const String ssid=server.arg("ssid"),password=server.arg("password");
    if(ssid.isEmpty()||ssid.length()>32||password.length()>63||(!password.isEmpty()&&password.length()<8)){error(400,"Invalid Wi-Fi credentials");return;}
    if(!storageReady||settings.putString("ssid",ssid)!=ssid.length()||settings.putString("wifiPass",password)!=password.length()){error(500,"Cannot save Wi-Fi credentials");return;}
    player.stop();json(200,"{\"ok\":true}");WiFi.disconnect();WiFi.begin(ssid.c_str(),password.c_str());
  });
  server.on("/api/update",HTTP_POST,[]{
    if(!authorized())return;
    if(!otaGood){resumeAfterOta();error(400,otaError.isEmpty()?"OTA failed":otaError);return;}
    json(200,"{\"ok\":true}");rebootAt=millis()+1500;
  },[]{
    auto& u=server.upload();
    if(!server.authenticate("admin",accessKey.c_str())||server.header("X-Radio-Control")!="1")return;
    if(u.status==UPLOAD_FILE_START){
      if(deepSleepPending){otaError="Deep sleep pending";return;}
      noteDisplayActivity();
      otaGood=false;otaBusy=true;otaError="";
      if(fm.active){fm.cancelScan();fm.setVolume(fm.volume,true);}

      if(!player.quiesce()){otaError="Player did not stop; retry";otaBusy=false;return;}
      const esp_partition_t* next=esp_ota_get_next_update_partition(nullptr);
      if(!next||!Update.begin(next->size,U_FLASH)){otaError="Cannot start OTA";otaBusy=false;resumeAfterOta();}
    }else if(u.status==UPLOAD_FILE_WRITE&&otaBusy){
      if(u.totalSize==0&&(u.currentSize<36||u.buf[0]!=0xe9||u.buf[12]!=9||u.buf[13]!=0||u.buf[32]!=0x32||u.buf[33]!=0x54||u.buf[34]!=0xcd||u.buf[35]!=0xab)){
        otaError="Expected ESP32-S3 application image";Update.abort();otaBusy=false;resumeAfterOta();
      }else if(Update.write(u.buf,u.currentSize)!=u.currentSize){otaError="OTA write failed";Update.abort();otaBusy=false;resumeAfterOta();}
    }else if(u.status==UPLOAD_FILE_END){if(otaBusy){otaGood=Update.end(true);if(!otaGood){otaError="OTA validation failed";resumeAfterOta();}}otaBusy=false;
    }else if(u.status==UPLOAD_FILE_ABORTED){Update.abort();otaBusy=false;otaError="Upload aborted";resumeAfterOta();}
  });
  server.onNotFound([]{if(authorized())error(404,"Not found");});server.begin();
}
// One framebuffer in PSRAM, with transfers limited to changed horizontal bands.
TFT_eSprite canvas(&screen);
bool lcdInitialized=false,lcdSpriteReady=false,sleepOverlay=false;
int paintedTheme=-1,paintedPage=-1,paintedMode=-1;
String infoPageKey;
String lastBands[4];
struct Palette {uint16_t bg,card,soft,accent,ink,muted;};
uint16_t rgb(uint32_t c){return uiRgb565(c);}
Palette palette(){
  const auto p=uiPaletteRgb(themeId);
  return {rgb(p.bg),rgb(p.card),rgb(p.soft),rgb(p.accent),rgb(p.ink),rgb(p.muted)};
}
void uiText(const String& text,int x,int y,int maxWidth,uint16_t color,int scale=1){
  const int end=x+maxWidth;size_t at=0;
  canvas.setTextFont(2);canvas.setTextSize(scale);canvas.setTextColor(color);canvas.setTextWrap(false);
  while(at<text.length()){
    uint32_t cp=lcdNextCodepoint(text.c_str(),text.length(),at);if(cp<32)continue;
    if(cp<127){const char a[2]={char(cp),0};int w=canvas.textWidth(a,2);if(x+w>end)break;canvas.setCursor(x,y+2*scale);canvas.print(a);x+=w;}
    else{
      if(x+16*scale>end)break;
      const uint8_t* glyph=cjkGlyph(cp);
      if(glyph){
        if(scale==1)canvas.drawBitmap(x,y,glyph,16,20,color);
        else for(int gy=0;gy<20;++gy)for(int gx=0;gx<16;++gx)if(glyph[gy*2+gx/8]&(0x80>>(gx%8)))canvas.fillRect(x+gx*scale,y+gy*scale,scale,scale,color);
      }else canvas.drawRect(x+2*scale,y+3*scale,12*scale,14*scale,color);
      x+=16*scale;
    }
  }
  canvas.setTextSize(1);
}
void bandPush(int y,int height){canvas.pushSprite(0,y,0,y,320,height);}
void draw(){
  const Palette c=palette();
  if(!lcdInitialized){
    screen.fillScreen(c.bg);canvas.setColorDepth(16);
    lcdSpriteReady=canvas.createSprite(320,240)!=nullptr;
    lcdInitialized=true;
    if(!lcdSpriteReady){screen.setTextFont(2);screen.setTextColor(c.ink,c.bg);screen.drawString("Display buffer allocation failed",4,40);}
  }
  if(!lcdSpriteReady)return;
  if(deepSleepPending){
    if(!sleepOverlay){
      canvas.fillSprite(c.bg);canvas.fillRoundRect(12,35,296,170,22,c.card);
      uiText("准备深度休眠",28,52,264,c.ink);
      uiText("请松开 B 键",28,88,264,c.accent);
      uiText("A 键取消 / B 键唤醒",28,119,264,c.muted);
      if(deepAlarmTarget){
        tm target{};localtime_r(&deepAlarmTarget,&target);char b[24];strftime(b,sizeof b,"%m/%d %H:%M",&target);
        uiText(String("闹钟 ")+b,28,153,264,c.accent);
        uiText("将提前唤醒联网",28,179,264,c.muted);
      }else uiText("未启用闹钟，仅按 B 唤醒",28,153,264,c.muted);
      canvas.pushSprite(0,0);sleepOverlay=true;
    }
    return;
  }
  if(sleepOverlay){sleepOverlay=false;paintedTheme=-1;}

  if(paintedTheme!=themeId||paintedPage!=int(infoPage)||paintedMode!=int(fm.active)){
    canvas.fillSprite(c.bg);for(auto& b:lastBands)b="";infoPageKey="";
    paintedTheme=themeId;paintedPage=int(infoPage);paintedMode=int(fm.active);
  }
  if(themePicker.active){
    tm t{};char clock[8]="--:--";if(localClock(t))strftime(clock,sizeof clock,"%H:%M",&t);
    const Station* st=stations.find(selectedId);const auto ps=player.snapshot();
    const String title=fm.active?String(fm.frequency/1000.0,2)+" MHz":st?st->name:String("添加电台");
    const bool muted=fm.active?fm.muted:player.muted();
    const String key=String("picker:")+themeId+clock+title+String(activeVolume())+String(muted)+themePickerError+ps.phase;
    if(key!=infoPageKey){
      canvas.fillSprite(c.bg);
      canvas.setTextFont(4);canvas.setTextSize(2);canvas.setTextColor(c.ink);canvas.setCursor(12,6);canvas.print(clock);canvas.setTextSize(1);
      uiText("配色预览",204,13,104,c.accent);uiText("北京时间",14,53,160,c.muted);
      canvas.fillRoundRect(10,79,300,83,20,c.card);
      uiText(fm.active?"FM 收音机":ps.phase=="playing"?"网络电台 / 播放中":ps.wanted?"网络电台 / 连接中":"网络电台 / 已停止",22,86,276,c.accent);
      uiText(title,22,119,276,c.ink);
      canvas.fillRoundRect(10,169,300,28,14,c.soft);
      uiText(muted?"静音":"音量",20,172,45,c.accent);
      canvas.fillRoundRect(76,180,168,6,3,c.bg);int width=168*activeVolume()/100;
      if(width>0)canvas.fillRoundRect(76,180,width<6?6:width,6,3,c.accent);
      uiText(String(activeVolume()),264,172,36,c.ink);
      uiText(String(themeId+1)+"/"+kUiThemeCount+" "+uiThemeName(themeId),14,198,294,c.accent);
      uiText(themePickerError.isEmpty()?String("转B预览 按B保存 按A取消"):themePickerError,14,219,294,c.muted);
      canvas.pushSprite(0,0);infoPageKey=key;
    }
    return;
  }
  static bool wasAlarmPage=false;
  if(alarmPage){
    wasAlarmPage=true;
    const String key=alarmJson(alarmDraft)+String(alarmField)+alarmEditError+String(alarmPreview)+String(stationRevision)+String(themeId);
    if(key!=infoPageKey){
      canvas.fillSprite(c.bg);uiText("闹钟设置",14,8,180,c.ink);
      const Station* st=stations.find(alarmDraft.stationId);
      String repeat=alarmDraft.days==0?"仅一次":alarmDraft.days==127?"每天":alarmDraft.days==62?"工作日":alarmDraft.days==65?"周末":"自定义";
      String values[]={alarmDraft.enabled?"开启":"关闭",String(alarmDraft.hour)+" 时",String(alarmDraft.minute)+" 分",repeat,st?st->name:String("未选择电台"),String(alarmDraft.volume)};
      const char* labels[]={"闹钟","小时","分钟","重复","电台","音量"};
      for(int i=0;i<6;++i){int y=34+i*25;if(i==alarmField)canvas.fillRoundRect(8,y-1,304,24,8,c.card);
        uiText(labels[i],16,y,66,c.muted);uiText(values[i],88,y,215,i==alarmField?c.accent:c.ink);}
      uiText(alarmEditError.length()?alarmEditError:String("A选项 B调整 / 按B保存"),10,188,300,c.accent);
      uiText(alarmPreview?"按A停止试听 / 双击A取消":"按A试听 / 双击A取消",10,214,300,c.muted);
      canvas.pushSprite(0,0);infoPageKey=key;
    }
    return;
  }
  if(wasAlarmPage){wasAlarmPage=false;paintedTheme=-1;infoPageKey="";for(auto& b:lastBands)b="";canvas.fillSprite(c.bg);canvas.pushSprite(0,0);}
  if(infoPage){
    const bool wifi=WiFi.status()==WL_CONNECTED;
    const String ip=wifi?WiFi.localIP().toString():String("未连接 Wi-Fi");
    const String key=ip+apName+accessKey+kVersion+String(battery.valid?battery.millivolts:0);
    if(key!=infoPageKey){
      canvas.fillSprite(c.bg);uiText("设备信息",18,14,180,c.ink);uiText("双击B返回",208,14,100,c.muted);
      canvas.fillRoundRect(10,45,300,116,20,c.card);
      uiText("网页管理",22,53,270,c.accent);
      uiText(wifi?String("http://")+ip:String("局域网未连接"),22,77,276,c.ink);
      uiText("热点: "+apName,22,102,276,c.ink);
      uiText("http://192.168.4.1",22,128,276,c.muted);
      canvas.fillRoundRect(10,170,300,48,16,c.soft);
      uiText("用户名: admin",22,173,276,c.ink);
      uiText("密码: "+accessKey,22,195,276,c.accent);
      canvas.setTextFont(1);canvas.setTextSize(1);canvas.setTextColor(c.muted);canvas.setCursor(14,227);canvas.print(kVersion);
      canvas.setCursor(220,227);canvas.print(battery.valid?String(battery.millivolts/1000.0,2)+"V":"BAT --");
      canvas.pushSprite(0,0);infoPageKey=key;
    }
    return;
  }
  tm t{};const bool clockOk=localClock(t);char clock[8]="--:--",date[16]="NTP";
  if(clockOk){strftime(clock,sizeof clock,"%H:%M",&t);strftime(date,sizeof date,"%m/%d",&t);}
  const bool wifi=WiFi.status()==WL_CONNECTED;
  String key=String(clock)+date+deepSleepError+String(wifi)+String(wakeAlarmPending)+String(alarmConfig.enabled)+String(fm.active)+String(battery.valid?battery.percent:-1);
  if(key!=lastBands[0]){
    canvas.fillRect(0,0,320,76,c.bg);
    canvas.setTextColor(c.ink);canvas.setTextFont(4);canvas.setTextSize(2);canvas.setCursor(12,7);canvas.print(clock);canvas.setTextSize(1);
    uiText(date,192,8,50,c.muted);
    const uint16_t batteryColor=battery.valid&&battery.percent<=15?rgb(darkUiTheme(themeId)?0xFFB4AB:0xB54A43):c.accent;
    canvas.drawRoundRect(249,13,19,11,2,batteryColor);canvas.fillRect(268,16,2,5,batteryColor);
    if(battery.valid){const int width=15*battery.percent/100;if(width>0)canvas.fillRect(251,15,width,7,batteryColor);}
    canvas.setTextFont(1);canvas.setTextSize(1);canvas.setTextColor(batteryColor);canvas.setCursor(275,15);
    canvas.print(battery.valid?String(battery.percent)+"%":String("--"));
    canvas.fillRoundRect(190,36,118,28,14,c.soft);
    uiText(!deepSleepError.isEmpty()?"休眠失败":wakeAlarmPending?"闹钟唤醒":wifi?"Wi-Fi 在线":"等待联网",200,40,100,c.accent);
    uiText(String("北京时间 / ")+(fm.active?"FM":"NET")+(alarmConfig.enabled?" 闹钟":""),14,53,172,c.muted);
    bandPush(0,76);lastBands[0]=key;
  }
  if(fm.active){
    const uint32_t f=fm.scanning?fm.scanFrequency:fm.frequency;
    const auto& rs=audioPath.state;
    int index=-1;String presetName;
    for(size_t i=0;i<fm.presets.size();++i)if(fm.presets[i].khz==fm.frequency){index=i;presetName=fm.presets[i].name;}
    key=String(f)+":"+String(fm.scanning)+":"+String(fm.progress)+":"+String(rs.levelTenths)+":"+String(rs.qualityValid)+":"+String(rs.stereo)+":"+String(index)+":"+String(fm.presets.size())+fm.message+fm.error;
    if(key!=lastBands[1]){
      canvas.fillRect(0,76,320,116,c.bg);canvas.fillRoundRect(10,76,300,110,20,c.card);
      String label=fm.scanning?String("自动搜台 ")+fm.progress+"%":String("FM ")+(rs.stereo?"立体声":"单声道");
      if(!fm.scanning&&rs.qualityValid)label+="  "+String(rs.levelTenths/10.0,1)+" dBuV";
      uiText(label,22,82,276,c.accent);
      canvas.setTextFont(4);canvas.setTextSize(2);canvas.setTextColor(c.ink);canvas.setCursor(22,110);canvas.print(String(f/1000.0,2));canvas.setTextSize(1);
      uiText("MHz",246,132,54,c.muted);
      String detail;
      if(fm.scanning)detail="已找到 "+String(fm.foundCount())+" 台 / 单击 A 取消";
      else if(!fm.error.isEmpty())detail=fm.error;
      else if(!fm.message.isEmpty()&&fm.presets.empty())detail=fm.message;
      else detail=index>=0?String("电台 ")+String(index+1)+" / "+String(fm.presets.size())+(presetName.startsWith("FM ")?String(""):String(" ")+presetName):String("未收藏，长按 A 搜台");
      uiText(detail,22,160,276,c.muted);
      bandPush(76,116);lastBands[1]=key;
    }
  }else{
  const auto p=player.snapshot();const Station* station=stations.find(selectedId);
  const String name=station?station->name:String("添加电台");
  String phase=p.phase=="playing"?"正在播放":p.phase=="stopped"?"已停止":p.phase=="buffering"?"缓冲中":p.phase=="connecting"?"连接中":p.phase=="retrying"?"重连中":p.phase=="waiting_wifi"?"等待网络":p.phase=="audio_error"?"音频错误":"准备中";
  key=name+"\n"+phase+"\n"+p.codec+String(p.bitrate);
  if(key!=lastBands[1]){
    canvas.fillRect(0,76,320,116,c.bg);canvas.fillRoundRect(10,76,300,110,20,c.card);
    canvas.fillCircle(28,93,4,c.accent);
    String streamInfo=phase+"  "+p.codec;
    if(p.bitrate)streamInfo+="  "+String(p.bitrate/1000)+" kbps";
    uiText(streamInfo,40,82,254,c.accent);
    // Name size depends only on its length; stream metadata cannot change it.
    size_t at=0;int units=0;while(at<name.length()){const auto cp=lcdNextCodepoint(name.c_str(),name.length(),at);units+=cp<128?1:2;}
    const int scale=units<=16?2:1;
    uiText(name,22,scale==2?119:129,276,c.ink,scale);
    bandPush(76,116);lastBands[1]=key;
  }
  }
  const bool displayMuted=fm.active?fm.muted:player.muted();
  key=String(activeVolume())+":"+String(displayMuted);
  if(key!=lastBands[2]){
    canvas.fillRect(0,192,320,48,c.bg);canvas.fillRoundRect(10,198,300,34,17,c.soft);
    uiText(displayMuted?"静音":"音量",22,204,40,c.accent);
    canvas.fillRoundRect(76,211,168,8,4,c.bg);
    const int width=168*activeVolume()/100;
    if(width>0)canvas.fillRoundRect(76,211,width<8?8:width,8,4,c.accent);
    canvas.fillCircle(80+160*activeVolume()/100,215,6,c.accent);
    uiText(String(activeVolume()),261,204,40,c.ink);
    bandPush(192,48);lastBands[2]=key;
  }

}
void endAlarmPreview(){
  if(!alarmPreview)return;
  player.stop();selectedId=previewOldId;player.setVolume(previewOldVolume);player.setMuted(previewOldMuted);
  if(previewOldFm){networkWantedBeforeFm=previewOldWanted;switchMode(true);networkWantedBeforeFm=previewOldWanted;}
  else if(previewOldWanted)playSelected();
  alarmPreview=false;
}
void openAlarmEditor(){
  if(fm.scanning)fm.cancelScan();
  alarmDraft=alarmConfig;alarmField=0;alarmEditError="";alarmPage=true;infoPage=false;
  if(!stations.find(alarmDraft.stationId)&&!stations.entries.empty())alarmDraft.stationId=stations.entries.front().id;
}
void closeAlarmEditor(bool save){
  if(save){
    if(alarmDraft.enabled&&!stations.find(alarmDraft.stationId)){alarmEditError="请先通过网页添加电台";return;}
    RadioAlarm next=alarmDraft;next.lastDay=radioAlarmScheduleChanged(alarmConfig,next)?0:alarmConfig.lastDay;
    if(!persistAlarm(next)){alarmEditError="保存失败，请重试";return;}
    alarmConfig=next;wakeAlarmPending=false;alarmError="";
  }
  endAlarmPreview();alarmPage=false;paintedTheme=-1;
}
void editAlarm(int delta){
  alarmEditError="";
  switch(alarmField){
    case 0:alarmDraft.enabled=!alarmDraft.enabled;break;
    case 1:alarmDraft.hour=alarmWrap(alarmDraft.hour+delta,24);break;
    case 2:alarmDraft.minute=alarmWrap(alarmDraft.minute+delta,60);break;
    case 3:alarmDraft.days=alarmRepeatStep(alarmDraft.days,delta);break;
    case 4:{if(stations.entries.empty())break;int at=0;for(size_t i=0;i<stations.entries.size();++i)if(stations.entries[i].id==alarmDraft.stationId)at=i;
      endAlarmPreview();alarmDraft.stationId=stations.entries[alarmWrap(at+delta,int(stations.entries.size()))].id;break;}
    case 5:alarmDraft.volume=constrain(int(alarmDraft.volume)+delta,1,100);if(alarmPreview)player.setVolume(alarmDraft.volume);break;
  }
}
void toggleAlarmPreview(){
  if(alarmPreview){endAlarmPreview();return;}
  const Station* st=stations.find(alarmDraft.stationId);
  if(!st){alarmEditError="没有可试听的网络电台";return;}
  if(WiFi.status()!=WL_CONNECTED){alarmEditError="试听需要连接 Wi-Fi";return;}
  previewOldFm=fm.active;previewOldWanted=fm.active?networkWantedBeforeFm:player.snapshot().wanted;
  previewOldId=selectedId;previewOldVolume=player.volume();previewOldMuted=player.muted();
  if(fm.active&&!switchMode(false)){alarmEditError=modeError;return;}
  alarmPreview=true;player.setVolume(alarmDraft.volume);player.setMuted(false);
  if(!player.play(st->id,st->url)){alarmEditError="试听启动失败";endAlarmPreview();}
}
void openThemePicker(){
  if(fm.scanning)return;
  themePicker.open(themeId);themePickerError="";paintedTheme=-1;
  releaseKeysGate=true;gateReleaseAt=0;
}
void closeThemePicker(bool save){
  if(!themePicker.active)return;
  if(save){
    if(!storageReady||settings.putInt("theme",themePicker.selected)!=sizeof(int32_t)){
      themePickerError="保存失败，请重试";return;
    }
    themePicker.saved();
  }else themeId=themePicker.cancel();
  paintedTheme=-1;infoPageKey="";
  releaseKeysGate=true;gateReleaseAt=0;noteDisplayActivity();
}
void handleButtonB(int event){
  if(themePicker.active){if(event==ButtonClick::Single)closeThemePicker(true);return;}
  if(alarmPage){if(event==ButtonClick::Single)closeAlarmEditor(true);return;}
  if(event==ButtonClick::Double)infoPage=!infoPage;
  else if(event==ButtonClick::Single){switchMode(!fm.active);}
  else if(event==ButtonClick::Long)openThemePicker();
}
void handleA(int event){
  if(themePicker.active){if(event==AlarmButton::Short)closeThemePicker(false);return;}
  if(event==AlarmButton::Double){if(alarmPage)closeAlarmEditor(false);else openAlarmEditor();return;}
  if(alarmPage){if(event==AlarmButton::Short)toggleAlarmPreview();return;}
  if(event==AlarmButton::Long){if(fm.active&&!fm.scanning){fm.startScan();infoPage=false;}}
  else if(event==AlarmButton::Short){
    if(fm.active){if(fm.scanning)fm.cancelScan();else fm.setVolume(fm.volume,!fm.muted);}
    else {if(player.snapshot().wanted)player.stop();else playSelected();}
  }
}
void inputs() {
  int steps[2];portENTER_CRITICAL(&encoderMux);
  for(int i=0;i<2;++i){steps[i]=encoderCounts[i]/4;encoderCounts[i]%=4;}portEXIT_CRITICAL(&encoderMux);
  if(otaBusy)return;
  const uint32_t now=millis();
  if(deepSleepPending){if(digitalRead(board::kEncoderAButton)==LOW)cancelDeepSleep();return;}
  if(releaseKeysGate){
    if(digitalRead(board::kEncoderAButton)==LOW||digitalRead(board::kEncoderBButton)==LOW)gateReleaseAt=0;
    else if(!gateReleaseAt)gateReleaseAt=now;
    else if(now-gateReleaseAt>=200){releaseKeysGate=false;buttonA=AlarmButton();buttonB=ButtonClick();}
    return;
  }
  if(steps[0]||steps[1])noteDisplayActivity(now);
  const bool consumeRotation=displayIdle.wakeGuard(now);
  if(!consumeRotation){
    if(themePicker.active){if(steps[1]){themeId=themePicker.step(-constrain(steps[1],-10,10));themePickerError="";}}
    else if(alarmPage){if(steps[0])alarmField=alarmWrap(alarmField+constrain(steps[0],-10,10),6);if(steps[1])editAlarm(-constrain(steps[1],-10,10));}
    else {
    if(steps[0]){if(fm.active)fm.step(constrain(steps[0],-10,10));else stepStation(constrain(steps[0],-10,10),true);}
    if(steps[1]){
      const int volume=constrain(activeVolume()-constrain(steps[1],-10,10),0,100);
      if(fm.active)fm.setVolume(volume,fm.muted);else player.setVolume(volume);
      saveAt=millis()+3000;
    }
  }
  }
  static bool stable[2]={false,false},old[2]={false,false},consumeRelease[2]={false,false};
  static uint32_t changed[2]{};
  const uint8_t pins[2]={board::kEncoderAButton,board::kEncoderBButton};
  for(int i=0;i<2;++i){bool raw=digitalRead(pins[i])==LOW;
    if(raw!=old[i]){old[i]=raw;changed[i]=now;}
    if(now-changed[i]>35&&raw!=stable[i]){
      stable[i]=raw;
      if(raw){
        noteDisplayActivity(now);
        if(displayIdle.wakeGuard(now)){consumeRelease[i]=true;continue;}
        if(i==0){buttonA.press(now);if(!alarmPage&&!themePicker.active&&fm.active&&fm.scanning){fm.cancelScan();buttonA.suppressRelease();}}
        else handleButtonB(buttonB.press(now));
      }else{
        if(consumeRelease[i]){consumeRelease[i]=false;continue;}
        if(i==0)handleA(buttonA.release(now));
      }
    }
  }
  anyButtonHeld=stable[0]||stable[1];
  if(!displayIdle.asleep()&&!displayIdle.wakeGuard(now)){
    handleA(buttonA.tick(now));handleButtonB(buttonB.tick(now,stable[1]));
  }
}

bool saveRuntimeForSleep(){
  return storageReady&&settings.putUInt("netLast",selectedId)==sizeof(uint32_t)&&
    settings.putInt("netVol",player.volume())==sizeof(int32_t)&&
    settings.putInt("fmVol",fm.volume)==sizeof(int32_t)&&settings.putBool("netAuto",autoPlay)==1;
}
void serviceDeepSleep(){
  if(!deepSleepPending)return;
  const uint32_t now=millis();
  if(otaBusy){deepSleepPending=false;deepSleepError="更新中，已取消深睡";return;}
  if(now-deepRequestAt>15000){deepSleepPending=false;deepSleepError="B 键未松开，已取消深睡";return;}
  if(digitalRead(board::kEncoderBButton)==LOW){deepReleasedAt=0;return;}
  if(!deepReleasedAt){deepReleasedAt=now;return;}
  if(now-deepReleasedAt<250||now-deepRequestAt<1500)return;
  time_t current=time(nullptr);
  if(alarmConfig.enabled){
    tm clock{};deepAlarmTarget=nextRadioAlarmEpoch(alarmConfig,current);
    if(!localClock(clock)||!deepAlarmTarget||deepAlarmTarget-current<=5||radioAlarmDue(alarmConfig,clock)){
      deepSleepPending=false;deepSleepError="闹钟临近或时钟无效，已取消深睡";return;
    }
  }
  if(!saveRuntimeForSleep()){deepSleepPending=false;deepSleepError="保存设置失败，未进入深睡";return;}
  SleepRecall recall{};recall.magic=0x4F534C50;recall.fmMode=fm.active;
  recall.networkWanted=fm.active?networkWantedBeforeFm:player.snapshot().wanted;
  recall.netMuted=player.muted();recall.fmMuted=fm.muted;recall.netVolume=player.volume();recall.fmVolume=fm.volume;
  const int oldGate=digitalRead(board::kAmplifierMute);
  bool panelOff=false,wifiOff=false,rtcButton=false,inputsOff=false;
  esp_err_t err=esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if(err==ESP_OK)err=rtc_gpio_init(gpio_num_t(board::kEncoderBButton));
  rtcButton=err==ESP_OK;
  if(err==ESP_OK)err=rtc_gpio_set_direction(gpio_num_t(board::kEncoderBButton),RTC_GPIO_MODE_INPUT_ONLY);
  if(err==ESP_OK)err=rtc_gpio_pullup_en(gpio_num_t(board::kEncoderBButton));
  if(err==ESP_OK)err=rtc_gpio_pulldown_dis(gpio_num_t(board::kEncoderBButton));
  if(err==ESP_OK)err=esp_sleep_enable_ext0_wakeup(gpio_num_t(board::kEncoderBButton),0);

  if(err==ESP_OK&&!player.quiesce())err=ESP_FAIL;
  if(err==ESP_OK&&!player.audioBusReleased())err=ESP_FAIL;
  digitalWrite(board::kAmplifierMute,HIGH);
  if(err==ESP_OK&&fm.scanning)fm.cancelScan();
  if(err==ESP_OK&&!audioPath.prepareDeepSleep())err=ESP_FAIL;
  if(err==ESP_OK){
    digitalWrite(board::kTftBacklight,LOW);screen.writecommand(0x28);screen.writecommand(0x10);delay(120);panelOff=true;
    err=rtc_gpio_init(gpio_num_t(board::kTftBacklight));
    if(err==ESP_OK)err=rtc_gpio_set_direction(gpio_num_t(board::kTftBacklight),RTC_GPIO_MODE_OUTPUT_ONLY);
    if(err==ESP_OK)err=rtc_gpio_set_level(gpio_num_t(board::kTftBacklight),0);
    if(err==ESP_OK)err=rtc_gpio_hold_en(gpio_num_t(board::kTftBacklight));
  }
  if(err==ESP_OK){wifiOff=true;if(!WiFi.mode(WIFI_OFF))err=ESP_FAIL;}
  if(err==ESP_OK){
    for(uint8_t pin:{board::kEncoderAClock,board::kEncoderAData,board::kEncoderBClock,board::kEncoderBData})detachInterrupt(pin);
    for(uint8_t pin:{board::kEncoderAClock,board::kEncoderAData,board::kEncoderAButton,board::kEncoderBClock,board::kEncoderBData})pinMode(pin,INPUT);
    inputsOff=true;
    // Arm last, after Wi-Fi/audio shutdown, using fresh epoch and bounded signed math.
    recall.sleepEpoch=int64_t(time(nullptr));recall.alarmTarget=int64_t(deepAlarmTarget);
    recall.timerUs=deepAlarmTarget?radioAlarmWakeDelayUs(deepAlarmTarget,time_t(recall.sleepEpoch)):0;
    if(deepAlarmTarget&&!recall.timerUs)err=ESP_ERR_INVALID_STATE;
    if(err==ESP_OK&&recall.timerUs)err=esp_sleep_enable_timer_wakeup(recall.timerUs);
    if(err==ESP_OK){
      JsonDocument history;history["sleepEpoch"]=recall.sleepEpoch;history["alarmTarget"]=recall.alarmTarget;history["timerUs"]=recall.timerUs;
      sleepHistory="";serializeJson(history,sleepHistory);
      if(settings.putString("sleepPlan",sleepHistory)!=sleepHistory.length())err=ESP_FAIL;
    }
    if(err==ESP_OK){
      rtcSleepRecall=recall;
      Serial.printf("SLEEP alarm=%lld delay_us=%llu\n",(long long)recall.alarmTarget,(unsigned long long)recall.timerUs);Serial.flush();
      err=esp_deep_sleep_try_to_start();
    } // Returns only on failure/rejection.
  }
  // Any rejected sleep must return to a usable UI and audio path.
  rtcSleepRecall.magic=0;esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if(rtcButton){rtc_gpio_deinit(gpio_num_t(board::kEncoderBButton));pinMode(board::kEncoderBButton,INPUT_PULLUP);}
  rtc_gpio_hold_dis(gpio_num_t(board::kTftBacklight));rtc_gpio_deinit(gpio_num_t(board::kTftBacklight));
  pinMode(board::kTftBacklight,OUTPUT);
  if(panelOff){screen.writecommand(0x11);delay(120);screen.writecommand(0x29);}
  bool recovered=false;
  if(recall.fmMode){fm.muted=recall.fmMuted;recovered=fm.enter(recall.fmVolume);}
  else if(audioPath.selectNetwork()){player.resume();if(recall.networkWanted)playSelected();else player.stop();recovered=true;}
  if(recovered)digitalWrite(board::kAmplifierMute,oldGate);
  if(inputsOff){
    for(uint8_t pin:{board::kEncoderAClock,board::kEncoderAData,board::kEncoderAButton,board::kEncoderBClock,board::kEncoderBData})pinMode(pin,INPUT_PULLUP);
    portENTER_CRITICAL(&encoderMux);encoderCounts[0]=0;encoderCounts[1]=0;
    encoderHistory[0]=(digitalRead(board::kEncoderAClock)<<1)|digitalRead(board::kEncoderAData);
    encoderHistory[1]=(digitalRead(board::kEncoderBClock)<<1)|digitalRead(board::kEncoderBData);portEXIT_CRITICAL(&encoderMux);
    attachInterrupt(board::kEncoderAClock,encoderA,CHANGE);attachInterrupt(board::kEncoderAData,encoderA,CHANGE);
    attachInterrupt(board::kEncoderBClock,encoderB,CHANGE);attachInterrupt(board::kEncoderBData,encoderB,CHANGE);
  }
  if(wifiOff){WiFi.mode(WIFI_AP_STA);WiFi.setSleep(false);WiFi.softAP(apName.c_str(),accessKey.c_str());
    const String ssid=settings.getString("ssid","");if(!ssid.isEmpty())WiFi.begin(ssid.c_str(),settings.getString("wifiPass","").c_str());
    server.stop();server.begin();}
  deepSleepPending=false;deepSleepError="深睡未成功 ("+String(int(err))+")";
  releaseKeysGate=true;gateReleaseAt=0;paintedTheme=-1;noteDisplayActivity();
}

}
void setup() {
  const auto cause=esp_sleep_get_wakeup_cause();rawWakeCause=int(cause);bootWakeEpoch=int64_t(time(nullptr));
  deepWake=esp_reset_reason()==ESP_RST_DEEPSLEEP&&rtcSleepRecall.magic==0x4F534C50;
  timerWake=esp_reset_reason()==ESP_RST_DEEPSLEEP&&cause==ESP_SLEEP_WAKEUP_TIMER;
  if(deepWake)wakeRecall=rtcSleepRecall;
  wakeAlarmTarget=timerWake&&deepWake?time_t(wakeRecall.alarmTarget):0;
  wakeAlarmPending=timerWake&&wakeAlarmTarget>0;
  rtcSleepRecall.magic=0;
  rtc_gpio_hold_dis(gpio_num_t(board::kTftBacklight));rtc_gpio_deinit(gpio_num_t(board::kTftBacklight));
  rtc_gpio_deinit(gpio_num_t(board::kEncoderBButton));
  releaseKeysGate=deepWake;

  pinMode(board::kTftBacklight,OUTPUT);digitalWrite(board::kTftBacklight,LOW);
  pinMode(board::kAmplifierMute,OUTPUT);digitalWrite(board::kAmplifierMute,HIGH);
  Serial.begin(115200);screen.init();screen.setRotation(3);
  for(uint8_t pin:{board::kEncoderAClock,board::kEncoderAData,board::kEncoderAButton,board::kEncoderBClock,board::kEncoderBData,board::kEncoderBButton})pinMode(pin,INPUT_PULLUP);
  encoderHistory[0]=(digitalRead(board::kEncoderAClock)<<1)|digitalRead(board::kEncoderAData);
  encoderHistory[1]=(digitalRead(board::kEncoderBClock)<<1)|digitalRead(board::kEncoderBData);
  attachInterrupt(board::kEncoderAClock,encoderA,CHANGE);attachInterrupt(board::kEncoderAData,encoderA,CHANGE);
  attachInterrupt(board::kEncoderBClock,encoderB,CHANGE);attachInterrupt(board::kEncoderBData,encoderB,CHANGE);
  storageReady=settings.begin("o-radio",false);
  if(storageReady){
    accessKey=settings.getString("access","");selectedId=settings.getUInt("netLast",0);
    fm.volume=constrain(settings.getInt("fmVol",50),0,100);
    autoPlay=settings.getBool("netAuto",false);player.setVolume(settings.getInt("netVol",25));
  }
  // Requested boot level; manual playback stays muted until explicitly unmuted.
  player.setVolume(50);player.setMuted(!autoPlay);
  loadAlarm();
  if(storageReady){
    sleepHistory=settings.getString("sleepPlan","");
    JsonDocument history;
    if(timerWake&&!wakeAlarmTarget&&!deserializeJson(history,sleepHistory)){
      wakeAlarmTarget=time_t(history["alarmTarget"]|int64_t(0));wakeAlarmPending=wakeAlarmTarget>0;
    }
  }
  if(wakeAlarmPending)alarmError="闹钟定时唤醒，等待联网与到点";
  if(storageReady){const int saved=settings.getInt("theme",0);themeId=validUiTheme(saved)?saved:0;}
  if(accessKey.length()!=12){char key[13];snprintf(key,sizeof key,"%08lx%04lx",(unsigned long)esp_random(),(unsigned long)(esp_random()&0xffff));accessKey=key;if(storageReady)settings.putString("access",accessKey);}
  stations.begin();if(!stations.find(selectedId))selectedId=stations.entries.empty()?0:stations.entries[0].id;
  stationRevision=esp_random()&0x7fffffff;
  fm.begin();
  const bool audioOk=audioPath.begin(true);const bool playerOk=player.begin(audioOk);
  WiFi.persistent(false);WiFi.mode(WIFI_AP_STA);WiFi.setSleep(false);WiFi.setAutoReconnect(true);
  apName="O-NetRadio-"+WiFi.macAddress().substring(12);apName.replace(":","");WiFi.softAP(apName.c_str(),accessKey.c_str());
  if(storageReady){String ssid=settings.getString("ssid","");if(!ssid.isEmpty())WiFi.begin(ssid.c_str(),settings.getString("wifiPass","").c_str());}
  configTime(8*3600,0,"ntp.aliyun.com","ntp.tencent.com","pool.ntp.org");
  battery.begin();
  setupWeb();draw();noteDisplayActivity();
  delay(200);digitalWrite(board::kAmplifierMute,audioOk&&playerOk?LOW:HIGH);
  if(deepWake){
    player.setVolume(constrain(wakeRecall.netVolume,0,100));fm.volume=constrain(wakeRecall.fmVolume,0,100);
    player.setMuted(timerWake?true:wakeRecall.netMuted);fm.muted=wakeRecall.fmMuted;
    if(!timerWake){
      if(wakeRecall.fmMode){switchMode(true);networkWantedBeforeFm=wakeRecall.networkWanted;}
      else if(wakeRecall.networkWanted)playSelected();
    }
  }else if(autoPlay&&!timerWake)playSelected();
  Serial.printf("%s ready: TEF=%d player=%d stations=%u\n",kVersion,audioOk,playerOk,unsigned(stations.entries.size()));
}
void loop() {
  server.handleClient();inputs();if(!otaBusy)fm.tick();checkAlarm();const uint32_t now=millis();
  if(!otaBusy&&Serial.available()&&Serial.read()=='?')Serial.println(fmDiagnostics());
  if(!otaBusy)battery.tick();
  const bool previouslyAsleep=displayIdle.asleep();
  displayIdle.tick(now,otaBusy||fm.scanning||anyButtonHeld||deepSleepPending||alarmPage||themePicker.active||wakeAlarmPending);
  if(previouslyAsleep!=displayIdle.asleep())digitalWrite(board::kTftBacklight,displayIdle.asleep()?LOW:HIGH);
  if(!otaBusy&&!displayIdle.asleep()&&now-lastDraw>=100){lastDraw=now;draw();}
  if(!otaBusy&&!alarmPreview&&saveAt&&int32_t(now-saveAt)>=0){saveAt=0;if(storageReady){settings.putUInt("netLast",selectedId);settings.putInt("netVol",player.volume());settings.putInt("fmVol",fm.volume);settings.putBool("netAuto",autoPlay);}}
  // Playback intent protects network buffering/reconnects and even muted streams.
  // FM has no separate stop state: mute or volume zero is treated as stopped.
  static uint32_t lastAutoCheck=0;
  if(uint32_t(now-lastAutoCheck)>=250){
    lastAutoCheck=now;
    const bool playing=fm.active?(!fm.muted&&fm.volume>0):player.snapshot().wanted;
    const bool busy=playing||otaBusy||fm.scanning||anyButtonHeld||deepSleepPending||alarmPage||themePicker.active||wakeAlarmPending||releaseKeysGate||rebootAt;
    if(autoSleep.tick(now,busy))requestDeepSleep();
  }
  serviceDeepSleep();
  if(rebootAt&&int32_t(now-rebootAt)>=0)ESP.restart();
  delay(1);
}
