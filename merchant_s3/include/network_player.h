#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
struct PlaybackStatus {
  String phase="stopped",title,codec,error;
  uint32_t stationId=0,bitrate=0,buffered=0,retries=0;
  bool wanted=false;
};
class NetworkPlayer {
 public:
  bool begin(bool hardwareReady);
  bool play(uint32_t id,const String& url);
  void stop();
  bool quiesce(uint32_t timeoutMs=6000);
  void resume(){suspend_=false;}
  void setVolume(int volume){volume_=constrain(volume,0,100);}
  void setMuted(bool mute){muted_=mute;}
  int volume()const{return volume_.load();}
  bool muted()const{return muted_.load();}
  PlaybackStatus snapshot();
  bool audioBusReleased()const{return busReleased_.load();}
 private:
  struct Request {uint32_t id;char url[769];};
  QueueHandle_t queue_=nullptr;
  SemaphoreHandle_t mutex_=nullptr;
  TaskHandle_t task_=nullptr;
  PlaybackStatus status_;
  std::atomic<int> volume_{25};
  std::atomic<bool> busReleased_{false};
  std::atomic<bool> muted_{false},suspend_{false},suspended_{false};
  bool hardwareReady_=false;
  void worker();
  static void trampoline(void* p){static_cast<NetworkPlayer*>(p)->worker();}
};
