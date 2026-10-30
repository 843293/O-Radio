#include <Audio.h>
// Pinned ESP32-audioI2S commit 8b4bbbd declares these non-pure base methods
// without definitions, leaving Decoder's vtable unresolved with GCC 14.
// All six concrete decoders override both; supply only the base fallbacks.
const char* Decoder::getStreamTitle() { return ""; }
const char* Decoder::whoIsIt() { return "Unknown"; }
