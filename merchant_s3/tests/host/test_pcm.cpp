#include "pcm_bridge.h"
static_assert(pcm16To32(0)==0);
static_assert(pcm16To32(1)==65536);
static_assert(pcm16To32(-1)==-65536);
static_assert(pcm16To32(32767)==2147418112);
static_assert(pcm16To32(-32768)==(-2147483647-1));
int main(){}
