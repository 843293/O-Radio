#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "radio_core.h"
struct FmPreset {uint32_t khz;String name;};
class FmReceiver {
 public:
  explicit FmReceiver(RadioCore& core):core_(core){}
  void begin();
  bool enter(int volume);
  bool leave();
  bool active=false,scanning=false,muted=true;
  int volume=50,progress=0;
  uint32_t frequency=99500,scanFrequency=65000;
  String message,error;
  std::vector<FmPreset> presets;
  bool tune(uint32_t khz);
  bool step(int direction);
  bool setVolume(int percent,bool silence);
  bool startScan();
  void cancelScan();
  void tick();
  bool add(uint32_t khz,const String& name);
  bool remove(uint32_t khz);
  String json() const;
  size_t foundCount() const {return peaks_.count;}
 private:
  RadioCore& core_;
  Preferences prefs_;
  bool storage_=false;
  uint32_t due_=0;
  unsigned sampleStage_=0;
  FmQuality first_;
  std::vector<FmPreset> found_;
  FmPeakList peaks_;
  bool persist(const std::vector<FmPreset>& list,uint32_t khz);
  bool hardwareTune(uint32_t khz,bool silence);
  void finishScan();
};
