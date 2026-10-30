#pragma once
#include <Arduino.h>
#include <initializer_list>
#include "fm_logic.h"

struct RadioDiagnostics {
  bool operationValid=false,processingValid=false;
  uint16_t operation=0,softmute=0,highcut=0,stereoBlend=0,highBlend=0;
  FmQuality quality;
};

struct RadioState {
  bool ready = false;
  bool muted = false;
  bool fm = true;
  bool qualityValid = false;
  bool stereo = false;
  uint32_t frequencyKhz = 99500;
  int volumeDb = -30;
  int bandwidthKhz = 0;
  int levelTenths = 0;
  uint32_t errors = 0;
  String error;
};

class RadioCore {
 public:
  RadioState state;
  bool begin(bool networkAudio = false);
  bool selectNetwork();
  bool prepareDeepSleep();
  bool selectFm(uint32_t khz,int volume,bool mute);
  bool setFmVolume(int volume,bool mute);
  FmQuality sampleFmQuality();
  RadioDiagnostics diagnostics();
  bool setNetworkVolume(int percent, bool mute);
  bool configure(bool fm, uint32_t khz, int db, bool mute, int bandwidth);
  bool step(int direction);
  void poll();
 private:
  bool write(const uint8_t* bytes, size_t size);
  bool command(uint8_t module, uint8_t cmd, std::initializer_list<uint16_t> args);
  bool read(uint8_t module, uint8_t cmd, uint8_t* bytes, size_t size);
  bool patch(const uint8_t* bytes, size_t size);
  uint32_t lastPoll_ = 0;
};
