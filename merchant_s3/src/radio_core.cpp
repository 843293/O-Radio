#include "radio_core.h"
#include <Wire.h>
#include "board_sts_6686n3.h"
#include "tef_patch_v102.h"
#include "tef_init_table.h"

namespace {
uint16_t be16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
}
bool RadioCore::write(const uint8_t* bytes, size_t size) {
  Wire.beginTransmission(board::kTefAddress);
  const bool complete = Wire.write(bytes, size) == size;
  const uint8_t result = Wire.endTransmission();
  delay(2);
  if (!complete || result) {
    ++state.errors;
    state.error = "I2C write failed";
    return false;
  }
  return true;
}
bool RadioCore::command(uint8_t module, uint8_t cmd,
                        std::initializer_list<uint16_t> args) {
  uint8_t bytes[32] = {module, cmd, 1};
  size_t i = 3;
  if (args.size() > 14) return false;
  for (auto word : args) { bytes[i++] = word >> 8; bytes[i++] = word; }
  return write(bytes, i);
}
bool RadioCore::read(uint8_t module, uint8_t cmd, uint8_t* bytes, size_t size) {
  if (!command(module, cmd, {})) return false;
  if (Wire.requestFrom(board::kTefAddress, size) != size) {
    while (Wire.available()) Wire.read();
    ++state.errors;
    state.error = "I2C read failed";
    return false;
  }
  for (size_t i = 0; i < size; ++i) bytes[i] = Wire.read();
  return true;
}
bool RadioCore::patch(const uint8_t* bytes, size_t size) {
  uint8_t chunk[25] = {0x1b};
  while (size) {
    const size_t count = min(size, size_t(24));
    memcpy(chunk + 1, bytes, count);
    if (!write(chunk, count + 1)) return false;
    size -= count; bytes += count;
  }
  return true;
}
bool RadioCore::begin(bool networkAudio) {
  Wire.begin(board::kTefSda, board::kTefScl, 100000);
  Wire.setTimeOut(50);
  uint8_t status[2]{};
  bool responded = false;
  for (unsigned retry = 0; retry < 20; ++retry) {
    if (read(64, 128, status, sizeof status)) { responded = true; break; }
    delay(5);
  }
  if (!responded) return false;
  const uint8_t reset[] = {0x1e,0x5a,0x01,0x5a,0x5a};
  if (!write(reset,sizeof reset)) return false;
  delay(100);
  const uint8_t clear[] = {0x1c,0,0};
  const uint8_t patchSelect[] = {0x1c,0,0x74};
  const uint8_t lutSelect[] = {0x1c,0,0x75};
  if (!write(clear,3)) return false;
  delay(100);
  if (!write(patchSelect,3) || !patch(pPatchBytes102,PatchSize102) ||
      !write(clear,3)) return false;
  delay(100);
  if (!write(lutSelect,3) || !patch(pLutBytes102,LutSize102) ||
      !write(clear,3)) return false;
  const uint8_t activate[] = {0x14,0,1};
  if (!write(activate,3)) return false;
  delay(50);
  const uint8_t clock[] = {0x40,4,1,0,0x8c,0xa0,0,0,0};
  if (!write(clock,sizeof clock) || !command(64,5,{1})) return false;
  delay(100);
  if (!command(64,1,{1})) return false;
  for (size_t offset = 0; offset < sizeof tuner_init_tab;) {
    const uint8_t size = tuner_init_tab[offset++];
    if (!size) break;
    if (offset + size > sizeof tuner_init_tab) return false;
    const uint8_t* entry=tuner_init_tab+offset;
    // The inherited RF table enables TEF's I2S output in master mode.
    // Never enable that driver when ESP32 owns the shared BCK/WS lines.
    const bool rfOnlyOutput=networkAudio&&entry[0]==48&&(entry[1]==22||entry[1]==13);
    if (!rfOnlyOutput&&!write(entry,size)) return false;
    offset += size;
  }
  if (networkAudio) {
    // Merchant network playback explicitly mutes the RF tuner.
    // Keep its digital output disabled; ESP32 supplies the audio clocks.
    if (!command(48,11,{1}) || !command(48,22,{33,0,16,0,4800}) ||
        !command(48,22,{32,0,16,0,4800})) return false;
    state.muted=true;
    state.ready = true;
    state.error = "";
    return true;
  }
  if (!command(48,12,{0}) || !command(32,31,{500})) return false;
  state.ready = true;
  if (!configure(state.fm,state.frequencyKhz,state.volumeDb,state.muted,
                 state.bandwidthKhz)) { state.ready = false; return false; }
  state.error = "";
  return true;
}
bool RadioCore::selectNetwork(){
  if(!state.ready)return false;
  const bool ok=command(48,11,{1})&&command(48,22,{33,0,32,0,4800})&&command(48,22,{32,0,32,0,4800})&&command(64,1,{1});
  state.muted=true;state.qualityValid=false;
  if(!ok)state.error="Cannot mute RF audio for network playback";
  return ok;
}
bool RadioCore::prepareDeepSleep(){
  if(!state.ready)return false;
  if(!command(48,11,{1})||!command(48,22,{33,0,32,0,4800})||
     !command(48,22,{32,0,32,0,4800})||!command(48,21,{128,0})||!command(64,1,{1})){
    state.error="TEF standby failed";return false;
  }
  state.muted=true;state.qualityValid=false;return true;
}
bool RadioCore::selectFm(uint32_t khz,int volume,bool mute){
  if(!state.ready||!fmFrequencyValid(khz)||volume<0||volume>100)return false;
  // Original V3.21 table: output SD1 from audio processor, I2S32 master 48k.
  // Caller must first stop ESP32 clocks and release its GPIO outputs.
  if(!command(48,11,{1})||!command(48,22,{32,0,32,0,4800})||
     !command(48,13,{128,224})||!command(48,21,{128,1})||
     !command(48,12,{0})||!command(32,31,{500})||
     !command(48,13,{33,224})||
     !command(64,3,{0,32,3})||!command(64,3,{1,32,0})||!command(64,3,{2,32,0})||
     !command(64,3,{0,33,3})||!command(64,3,{1,33,0})||!command(64,3,{2,33,0})||
     !command(64,1,{0})||
     !command(48,22,{33,2,32,256,4800})){
    state.error="Cannot select FM digital output";return false;
  }
  return configure(true,khz,fmVolumeDb(volume),mute||volume==0,0);
}
bool RadioCore::setFmVolume(int volume,bool mute){
  if(!state.ready||volume<0||volume>100)return false;
  const bool silence=mute||volume==0;
  if((silence&&!command(48,11,{1}))||!command(48,10,{uint16_t(int16_t(fmVolumeDb(volume)*10))})||
     (!silence&&!command(48,11,{0}))){state.error="FM volume I/O failed";return false;}
  state.volumeDb=fmVolumeDb(volume);state.muted=silence;return true;
}
FmQuality RadioCore::sampleFmQuality(){
  FmQuality q;uint8_t bytes[14]{};
  if(!state.ready||!read(32,129,bytes,sizeof bytes))return q;
  q.status=be16(bytes);q.valid=(q.status&0x3ff)!=0;
  q.level=int16_t(be16(bytes+2));q.usn=be16(bytes+4);q.wam=be16(bytes+6);q.offset=int16_t(be16(bytes+8));
  state.qualityValid=q.valid;state.levelTenths=q.level;return q;
}
RadioDiagnostics RadioCore::diagnostics(){
  RadioDiagnostics d;d.quality=sampleFmQuality();uint8_t operation[2]{},processing[8]{};
  d.operationValid=read(64,128,operation,sizeof operation);
  if(d.operationValid)d.operation=be16(operation);
  d.processingValid=read(32,134,processing,sizeof processing);
  if(d.processingValid){d.softmute=be16(processing);d.highcut=be16(processing+2);d.stereoBlend=be16(processing+4);d.highBlend=be16(processing+6);}
  return d;
}
bool RadioCore::setNetworkVolume(int percent, bool mute) {
  if (!state.ready || percent < 0 || percent > 100) return false;
  const bool silence=mute || percent==0;
  const int gainTenths=-600+(percent*480)/100; // -60..-12 dB; no positive gain.
  if (silence && !command(48,11,{1})) return false;
  if (!command(48,10,{uint16_t(int16_t(gainTenths))})) return false;
  if (!silence && !command(48,11,{0})) return false;
  state.volumeDb=gainTenths/10;state.muted=silence;state.error="";
  return true;
}
bool RadioCore::configure(bool fm, uint32_t khz, int db, bool mute, int bandwidth) {
  if (!state.ready) return false;
  if ((fm && (khz < 65000 || khz > 108000 || khz % 10)) ||
      (!fm && (khz < 144 || khz > 27000)) || db < -60 || db > 0 ||
      (fm && bandwidth != 0 && (bandwidth < 56 || bandwidth > 311)) ||
      (!fm && (bandwidth < 3 || bandwidth > 9))) {
    state.error = "Invalid radio settings";
    return false;
  }
  const uint8_t module = fm ? 32 : 33;
  // Mute during band/tuning changes; restore requested mute only after success.
  if (!command(48,11,{1}) ||
      !command(module,1,{1,uint16_t(fm ? khz/10 : khz)}) ||
      !(fm ? command(32,10,{uint16_t(bandwidth == 0 ? 1 : 0),
                            uint16_t(bandwidth*10),1000,1000})
           : command(33,10,{0,uint16_t(bandwidth*10)})) ||
      !command(48,10,{uint16_t(int16_t(db*10))}) ||
      !command(48,11,{uint16_t(mute)})) {
    state.ready = false;
    state.qualityValid = false;
    state.error = "Radio I/O failed; reboot to reinitialize";
    return false;
  }
  state.fm=fm; state.frequencyKhz=khz; state.volumeDb=db;
  state.muted=mute; state.bandwidthKhz=bandwidth;
  state.qualityValid=false; state.stereo=false; state.error="";
  return true;
}
bool RadioCore::step(int direction) {
  int32_t f=int32_t(state.frequencyKhz)+direction*(state.fm?100:5);
  f=constrain(f,state.fm?65000:144,state.fm?108000:27000);
  return configure(state.fm,f,state.volumeDb,state.muted,state.bandwidthKhz);
}
void RadioCore::poll() {
  if (!state.ready || millis()-lastPoll_<500) return;
  lastPoll_=millis();
  uint8_t bytes[14]{};
  state.qualityValid=read(state.fm?32:33,128,bytes,sizeof bytes) && ((be16(bytes)&0x3ff)!=0);
  if (state.qualityValid) state.levelTenths=int16_t(be16(bytes+2));
  uint8_t signal[2]{};
  state.stereo=state.fm && read(32,133,signal,2) && (be16(signal)>>15);
}
