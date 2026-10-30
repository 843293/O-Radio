#include <cassert>
#include <driver/i2s_std.h>
#include <esp_rom_gpio.h>
#include "audio_bus.h"
i2s_chan_handle_t networkAudioHandle(){return reinterpret_cast<void*>(1);}
int main(){
 fakeDisableFailure=true;assert(!releaseNetworkAudioBus());assert(fakeReleasedPins.empty());
 fakeDisableFailure=false;assert(releaseNetworkAudioBus());assert(fakeDisableCalls==2);
 assert((fakeReleasedPins==std::vector<int>{2,43,1,44}));
 assert(fakeDetachedPins==fakeReleasedPins);
 assert(releaseNetworkAudioBus());assert(fakeDisableCalls==2);
 fakeConfigFailure=true;assert(!acquireNetworkAudioBus());assert(fakeEnableCalls==0);
 fakeConfigFailure=false;fakeEnableFailure=true;assert(!acquireNetworkAudioBus());
 fakeEnableFailure=false;assert(acquireNetworkAudioBus());
 assert(fakeRestoredPins.bclk==2&&fakeRestoredPins.ws==43&&fakeRestoredPins.dout==1&&fakeRestoredPins.mclk==44&&fakeRestoredPins.din==-1);
 fakePinFailure=true;assert(!releaseNetworkAudioBus());const int calls=fakeDisableCalls;
 fakePinFailure=false;assert(releaseNetworkAudioBus());assert(fakeDisableCalls==calls);assert(acquireNetworkAudioBus());
}
