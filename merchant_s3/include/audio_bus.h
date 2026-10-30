#pragma once
// Called only by the network worker, after decoder writes have stopped.
bool releaseNetworkAudioBus();
bool acquireNetworkAudioBus();
