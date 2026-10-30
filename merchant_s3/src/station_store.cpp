#include "station_store.h"
#include <SPIFFS.h>
namespace {
constexpr char kPath[]="/o_net.json",kBackup[]="/o_net.back.json",kTemp[]="/o_net.tmp";
String encode(const std::vector<Station>& stations) {
  JsonDocument doc;auto list=doc.to<JsonArray>();
  for(const auto& s:stations){auto o=list.add<JsonObject>();o["id"]=s.id;o["name"]=s.name;o["url"]=s.url;o["group"]=s.group;}
  String text;serializeJson(doc,text);return text;
}
}
bool StationStore::validUrl(const String& url) {
  if(url.length()>768||url.length()<9)return false;
  const size_t start=url.startsWith("https://")?8:(url.startsWith("http://")?7:0);
  if(!start||url[start]=='/'||url[start]=='?'||url[start]=='#')return false;
  for(size_t i=0;i<url.length();++i)if(uint8_t(url[i])<=32||uint8_t(url[i])>=127)return false;
  return true;
}
bool StationStore::parse(const String& text,std::vector<Station>& out) {
  if(text.length()>120000){error="List too large (120 KB maximum)";return false;}
  JsonDocument doc;
  if(deserializeJson(doc,text,DeserializationOption::NestingLimit(5))||!doc.is<JsonArray>()) {error="Expected a JSON array";return false;}
  if(doc.size()>kMaxStations){error="Maximum 100 stations";return false;}
  uint32_t next=1;
  for(JsonVariant v:doc.as<JsonArray>()) {
    if(!v.is<JsonObject>()||!v["name"].is<const char*>()||!v["url"].is<const char*>()||
       (!v["group"].isNull()&&!v["group"].is<const char*>())||(!v["id"].isNull()&&!v["id"].is<uint32_t>())){error="Invalid station fields";return false;}
    Station s{v["id"]|0u,v["name"].as<String>(),v["url"].as<String>(),v["group"]|""};
    s.name.trim();s.group.trim();
    if(s.name.isEmpty()||s.name.length()>96||s.group.length()>64||!validUrl(s.url)){error="Invalid name, group or stream URL";return false;}
    for(size_t i=0;i<s.name.length();++i)if(uint8_t(s.name[i])<32){error="Control character in name";return false;}
    if(s.id>=0x7fffffffu){error="Invalid station ID";return false;}
    if(s.id>=next)next=s.id+1;
    if(!s.id){
      for(;;++next){bool used=false;for(const auto& old:out)if(old.id==next)used=true;if(!used)break;}
      if(!next||next==UINT32_MAX){error="Station ID exhausted";return false;}
      s.id=next++;
    }
    for(const auto& old:out)if(old.id==s.id){error="Duplicate station ID";return false;}
    out.push_back(std::move(s));
  }
  return true;
}
bool StationStore::load(const char* path) {
  File f=SPIFFS.open(path,"r");if(!f)return false;
  if(f.size()>120000){error="Stored list too large";return false;}
  String text=f.readString();f.close();std::vector<Station> next;
  if(!parse(text,next))return false;
  entries=std::move(next);return true;
}
bool StationStore::begin() {
  mounted=SPIFFS.begin(false);
  if(!mounted){error="SPIFFS unavailable; not formatted to preserve factory data";return false;}
  if(load(kPath)){error="";return true;}
  if(load(kBackup)){error="Recovered station list from backup";return true;}
  if(!SPIFFS.exists(kPath)&&!SPIFFS.exists(kBackup)){error="";return true;}
  error="Station list damaged; import a new list";return false;
}
bool StationStore::replace(const String& text) {
  if(!mounted){error="SPIFFS unavailable";return false;}
  std::vector<Station> next;if(!parse(text,next))return false;
  String body=encode(next);
  File f=SPIFFS.open(kTemp,"w");
  if(!f){error="Cannot create station file";return false;}
  const size_t written=f.print(body);f.flush();f.close();
  if(written!=body.length()){SPIFFS.remove(kTemp);error="Station file write incomplete";return false;}
  // Keep the previous complete file during replacement; boot can recover the backup.
  if(SPIFFS.exists(kPath)){
    if(SPIFFS.exists(kBackup)&&!SPIFFS.remove(kBackup)){error="Cannot rotate backup";return false;}
    if(!SPIFFS.rename(kPath,kBackup)){error="Cannot back up station list";return false;}
  }
  if(!SPIFFS.rename(kTemp,kPath)){if(SPIFFS.exists(kBackup))SPIFFS.rename(kBackup,kPath);error="Cannot commit station list";return false;}
  entries=std::move(next);error="";return true;
}
String StationStore::json() const {return encode(entries);}
const Station* StationStore::find(uint32_t id) const {for(const auto& s:entries)if(s.id==id)return &s;return nullptr;}
