#include "network_player.h"
#include <Audio.h>
#include <WiFi.h>
#include "board_sts_6686n3.h"
#include "audio_bus.h"
bool NetworkPlayer::begin(bool ready) {
  hardwareReady_=ready;mutex_=xSemaphoreCreateMutex();queue_=xQueueCreate(1,sizeof(Request));
  if(!mutex_||!queue_){status_.phase="audio_error";status_.error="Player allocation failed";return false;}
  if(!ready){status_.phase="audio_error";status_.error="TEF audio path initialization failed";return true;}
  if(!psramFound()){status_.phase="audio_error";status_.error="PSRAM required for network audio";return false;}
  if(xTaskCreatePinnedToCore(trampoline,"net_player",16384,this,1,&task_,0)!=pdPASS){
    status_.phase="audio_error";status_.error="Player task creation failed";return false;
  }
  return true;
}
bool NetworkPlayer::play(uint32_t id,const String& url) {
  if(!task_||suspend_||url.length()>768)return false;
  Request r{};r.id=id;url.toCharArray(r.url,sizeof r.url);
  return xQueueOverwrite(queue_,&r)==pdPASS;
}
void NetworkPlayer::stop(){if(queue_){Request r{};xQueueOverwrite(queue_,&r);}}
PlaybackStatus NetworkPlayer::snapshot(){
  if(!mutex_){PlaybackStatus s;s.phase="audio_error";s.error="Player allocation failed";return s;}
  xSemaphoreTake(mutex_,portMAX_DELAY);auto s=status_;xSemaphoreGive(mutex_);return s;
}
bool NetworkPlayer::quiesce(uint32_t timeoutMs){
  if(!task_)return true;
  suspend_=true;
  const uint32_t start=millis();while(!suspended_&&millis()-start<timeoutMs)delay(10);
  if(!suspended_){suspend_=false;return false;}return true;
}
void NetworkPlayer::worker() {
  Audio* audio=new(std::nothrow) Audio();
  if(!audio||!audio->setPinout(board::kI2sBclk,board::kI2sLrclk,board::kI2sDout,board::kI2sMclk)){
    xSemaphoreTake(mutex_,portMAX_DELAY);status_.phase="audio_error";status_.error="I2S setup failed";xSemaphoreGive(mutex_);
    for(;;){if(suspend_)suspended_=true;vTaskDelay(pdMS_TO_TICKS(50));}
  }
  // Merchant startup explicitly selects LSBJ (zero bit shift) after pin setup.
  audio->setI2SCommFMT_LSB(true);
  audio->setConnectionTimeout(2500,4000);audio->setVolumeSteps(100);
  Audio::audio_info_callback=[this](Audio::msg_t msg){
    if(!msg.msg)return;
    if(msg.e==Audio::evt_streamtitle||msg.e==Audio::evt_eof){
      xSemaphoreTake(mutex_,portMAX_DELAY);
      if(msg.e==Audio::evt_streamtitle)status_.title=String(msg.msg).substring(0,256);
      xSemaphoreGive(mutex_);
    }
  };
  Request desired{};bool wanted=false,connected=false,busNeedsRestore=false;int previousVolume=-1;
  uint32_t retryAt=0,progressAt=millis(),lastSeconds=0,lastPublish=0,retries=0;
  auto phase=[&](const char* p,const char* e=""){
    xSemaphoreTake(mutex_,portMAX_DELAY);status_.phase=p;status_.error=e;
    status_.stationId=desired.id;status_.wanted=wanted;status_.retries=retries;xSemaphoreGive(mutex_);
  };
  for(;;){
    if(suspend_){
      if(!suspended_){
        audio->stopSong();connected=false;
        // 16 DMA descriptors x 512 frames at 48kHz: allow auto-clear to drain.
        vTaskDelay(pdMS_TO_TICKS(250));
        busNeedsRestore=true;
        if(!releaseNetworkAudioBus()){phase("audio_error","Cannot release network I2S bus");vTaskDelay(pdMS_TO_TICKS(100));continue;}
        busReleased_=true;phase("paused_ota");suspended_=true;
      }
      vTaskDelay(pdMS_TO_TICKS(20));continue;
    }
    if(busNeedsRestore){
      if(!acquireNetworkAudioBus()){phase("audio_error","Cannot restore network I2S bus");vTaskDelay(pdMS_TO_TICKS(100));continue;}
      busReleased_=false;busNeedsRestore=false;previousVolume=-1;retryAt=millis();
      phase(wanted?"connecting":"stopped");
    }
    suspended_=false;
    Request request;
    if(xQueueReceive(queue_,&request,0)==pdPASS){
      audio->stopSong();desired=request;wanted=desired.url[0];connected=false;retryAt=millis();retries=0;
      xSemaphoreTake(mutex_,portMAX_DELAY);status_.title="";status_.codec="";status_.buffered=0;status_.bitrate=0;xSemaphoreGive(mutex_);
      phase(wanted?"connecting":"stopped");
    }
    // Network audio uses PCM attenuation, not the muted RF tuner.
    const int effective=muted_?0:volume_.load();
    if(previousVolume!=effective){audio->setVolume(effective);previousVolume=effective;}
    if(wanted&&WiFi.status()!=WL_CONNECTED){
      if(connected)audio->stopSong();
      connected=false;phase("waiting_wifi");retryAt=millis()+1000;
    }else if(wanted&&!connected&&int32_t(millis()-retryAt)>=0){
      phase("connecting");connected=audio->connecttohost(desired.url);
      progressAt=millis();lastSeconds=0;
      if(!connected){++retries;retryAt=millis()+min<uint32_t>(30000,2000*min<uint32_t>(retries,15));phase("retrying","Stream connection failed");}
      else phase("buffering");
    }
    if(connected){
      audio->loop();const uint32_t seconds=audio->getAudioCurrentTime();
      if(seconds!=lastSeconds){lastSeconds=seconds;progressAt=millis();}
      if(!audio->isRunning()||millis()-progressAt>25000){
        audio->stopSong();connected=false;++retries;
        retryAt=millis()+min<uint32_t>(30000,2000*min<uint32_t>(retries,15));phase("retrying","Stream stopped or timed out");
      }else if(millis()-lastPublish>=500){
        lastPublish=millis();
        xSemaphoreTake(mutex_,portMAX_DELAY);
        status_.phase=seconds?"playing":"buffering";status_.buffered=audio->inBufferFilled();
        status_.bitrate=audio->getBitRate();status_.codec=audio->getCodecname();xSemaphoreGive(mutex_);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
