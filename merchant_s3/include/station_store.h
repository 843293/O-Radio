#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
struct Station { uint32_t id; String name,url,group; };
class StationStore {
 public:
  static constexpr size_t kMaxStations=100;
  std::vector<Station> entries;
  bool mounted=false;
  String error;
  bool begin();
  bool replace(const String& text);
  String json() const;
  const Station* find(uint32_t id) const;
  static bool validUrl(const String& url);
 private:
  bool parse(const String& text,std::vector<Station>& out);
  bool load(const char* path);
};
