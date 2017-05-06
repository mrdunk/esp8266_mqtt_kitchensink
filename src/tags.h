/* Copyright 2017 Duncan Law (mrdunk@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ESP8266__TAGS_H
#define ESP8266__TAGS_H

/* This library does some standard parsing on incoming messages. */

#include "config.h"
#include "mqtt.h"
#include "host_attributes.h"
#include "devices.h"
#include "mdns_actions.h"


#define MAX_TAG_RECURSION 6


#define COMMON_DEF Config* _config, MdnsLookup* _brokers, mdns::MDns* _mdns, Mqtt* _mqtt, Io* _io

#define COMMON_PERAMS _config, _brokers, _mdns, _mqtt, _io

#define COMMON_ALLOCATE config(_config), brokers(_brokers), mdns(_mdns), mqtt(_mqtt), io(_io), base_children(_base_children), parent(NULL), children_len(_children_len)

#define CHILDREN_LEN (sizeof(children)/sizeof(children[0]))

class TagBase{
 public:
  TagBase(TagBase** _base_children, const uint8_t _children_len,
          COMMON_DEF, const char* _name) : COMMON_ALLOCATE, name(_name) {}

  virtual bool contentsAt(uint8_t /*index*/, String& content, int& value){
    content = "";
    value = 0;
    return false;
  }

  virtual unsigned int contentCount(){
    return 0;
  }

  TagBase* getChild(const String* name_list,
                          uint8_t list_pointer=0,
                          TagBase* best_so_far = NULL)
  {
    int8_t max_pointer = -1;
    for(int8_t i = 0; i < MAX_TAG_RECURSION; i++){
      if(name_list[i] == ""){
        max_pointer = i -1;
        break;
      }
    }
    
    if(max_pointer < 0){
      Serial.println("ERROR: name_list not populated.");
      return NULL;
    }

    for(uint8_t child = 0; child < children_len; child++){
      base_children[child]->parent = this;

      if(String(base_children[child]->name) == name_list[max_pointer]){
        // If there is not a match under the deepest mustache recursion level
        // we should return a match at a lower level if one exists.
        best_so_far = base_children[child];
      }

      if(String(base_children[child]->name) == name_list[list_pointer]){
        Serial.print(" ");
        Serial.print(list_pointer);
        Serial.print(" ");
        Serial.print(child);
        Serial.print(" ");
        Serial.print(base_children[child]->name);
        Serial.println(" match!");

        if(name_list[list_pointer +1] == ""){
          // The whole path matches.
          return base_children[child];
        } else if (TagBase* tag_child = 
                   base_children[child]->getChild(name_list, list_pointer +1))
        {
          // Deeper recursion level found a match (or at least a best_so_far).
          return tag_child;
        }
      }
      // No exact match so continue iterating.
    }

    Serial.print("no match for ");
    Serial.println(name_list[list_pointer]);
    return best_so_far;
  }

  TagBase* getParent(){
    return parent;
  }
 
 protected:
  Config* config;
  MdnsLookup* brokers;
  mdns::MDns* mdns;
  Mqtt* mqtt;
  Io* io;
  TagBase** base_children;
  TagBase* parent;
  const uint8_t children_len;
 public:
  const char* name;
};

class TagSessionValid : public TagBase{
 public:
  TagSessionValid(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "valid"),
                                children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    const unsigned int now = millis() / 1000;
    const int remaining = config->session_time + SESSION_TIMEOUT - now;
    if(config->sessionValid() && remaining > 0){
      content = "valid for ";
      content += remaining;
      value = remaining;
    } else {
      content = "expired";
      value = 0;
    }
    return false;
  }
};

class TagSessionValiduntil : public TagBase{
 public:
  TagSessionValiduntil(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "validuntil"),
                                     children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = config->session_time + SESSION_TIMEOUT;
    content = "";
    return false;
  }
};

class TagSessionProvidedtoken : public TagBase{
 public:
  TagSessionProvidedtoken(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "providedtoken"),
                                        children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = config->session_token_provided;
    content = config->session_token_provided;
    return false;
  }
};

class TagSessionExpectedtoken : public TagBase{
 public:
  TagSessionExpectedtoken(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "expectedtoken"),
                                        children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = config->session_token;
    content = config->session_token;
    return false;
  }
};

class TagSessionOverrideauth : public TagBase{
 public:
  TagSessionOverrideauth(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "overrideauth"),
                                        children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = config->session_override;
    content = config->session_override;
    return false;
  }
};

class TagSession : public TagBase{
 public:
  TagSession(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "session"),
                           children{new TagSessionValid(COMMON_PERAMS),
                                    new TagSessionValiduntil(COMMON_PERAMS),
                                    new TagSessionProvidedtoken(COMMON_PERAMS),
                                    new TagSessionExpectedtoken(COMMON_PERAMS),
                                    new TagSessionOverrideauth(COMMON_PERAMS)} { }
  TagBase* children[5];
};

class TagHostHostname : public TagBase{
 public:
  TagHostHostname(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "hostname"),
                                children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = config->hostname;
    return false;
  }
};

class TagHostMac : public TagBase{
 public:
  TagHostMac(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "mac"),
                           children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    uint8_t mac[6];
    WiFi.macAddress(mac);
    content = macToStr(mac);
    return false;
  }
};

class TagHostUptime : public TagBase{
 public:
  TagHostUptime(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "mac"),
                           children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = millis() / 1000;
    content = (millis() / 1000);
    content += " seconds";
    return false;
  }
};

class TagHostRssi : public TagBase{
 public:
  TagHostRssi(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "rssi"),
                           children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = WiFi.RSSI();
    content = WiFi.RSSI();
    content += "dBm";
    return false;
  }
};

class TagHostCoreCpuspeed : public TagBase{
 public:
  TagHostCoreCpuspeed(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "cpu_speed"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = ESP.getCpuFreqMHz();
    content = ESP.getCpuFreqMHz();
    content += "MHz";
    return false;
  }
};

class TagHostCoreFlashsize : public TagBase{
 public:
  TagHostCoreFlashsize(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "flash_size"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = ESP.getFlashChipSize();
    content = (ESP.getFlashChipSize() / 1024);
    content += "k";
    return false;
  }
};

class TagHostCoreFlashspeed : public TagBase{
 public:
  TagHostCoreFlashspeed(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "flash_speed"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ESP.getFlashChipSpeed();
    return false;
  }
};

class TagHostCoreFlashfree : public TagBase{
 public:
  TagHostCoreFlashfree(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "flash_free"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = ESP.getFreeSketchSpace();
    content = (ESP.getFreeSketchSpace() / 1024);
    content += "k";
    return false;
  }
};

class TagHostCoreFlashratio : public TagBase{
 public:
  TagHostCoreFlashratio(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "flash_ratio"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = "";
    if(ESP.getFlashChipSize() > 0){
      value = (100 * ESP.getFreeSketchSpace() / ESP.getFlashChipSize()) -1;
      content = (int(100 * ESP.getFreeSketchSpace() / ESP.getFlashChipSize()));
      content += "%";
    }
    return false;
  }
};

class TagHostCoreRamfree : public TagBase{
 public:
  TagHostCoreRamfree(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "ram_free"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = ESP.getFreeHeap();
    content = (ESP.getFreeHeap() / 1024);
    content += "k";
    return false;
  }
};

class TagHostCoreSdkversion : public TagBase{
 public:
  TagHostCoreSdkversion(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "sdk_version"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ESP.getSdkVersion();
    return false;
  }
};

class TagHostCoreCoreversion : public TagBase{
 public:
  TagHostCoreCoreversion(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "core_version"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ESP.getCoreVersion();
    return false;
  }
};

class TagHostCoreResetreason : public TagBase{
 public:
  TagHostCoreResetreason(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "reset_reason"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ESP.getResetReason();
    return false;
  }
};

class TagHostCoreChipid : public TagBase{
 public:
  TagHostCoreChipid(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "chip_id"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ESP.getChipId();
    return false;
  }
};

class TagHostCoreCpucycles : public TagBase{
 public:
  TagHostCoreCpucycles(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "cpu_cycles"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ESP.getCycleCount();
    return false;
  }
};

class TagHostCore : public TagBase{
 public:
  TagHostCore(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "core"),
                          children{new TagHostCoreCpuspeed(COMMON_PERAMS),
                                   new TagHostCoreFlashsize(COMMON_PERAMS),
                                   new TagHostCoreFlashfree(COMMON_PERAMS),
                                   new TagHostCoreFlashratio(COMMON_PERAMS),
                                   new TagHostCoreFlashspeed(COMMON_PERAMS),
                                   new TagHostCoreRamfree(COMMON_PERAMS),
                                   new TagHostCoreSdkversion(COMMON_PERAMS),
                                   new TagHostCoreCoreversion(COMMON_PERAMS),
                                   new TagHostCoreResetreason(COMMON_PERAMS),
                                   new TagHostCoreChipid(COMMON_PERAMS),
                                   new TagHostCoreCpucycles(COMMON_PERAMS)
                          } { }
  TagBase* children[11];
};

class TagHostNwAddress : public TagBase{
 public:
  TagHostNwAddress(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "address"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(WiFi.localIP());
    return false;
  }
};

class TagHostNwGateway : public TagBase{
 public:
  TagHostNwGateway(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "gateway"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(WiFi.subnetMask());
    return false;
  }
};

class TagHostNwSubnet : public TagBase{
 public:
  TagHostNwSubnet(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "subnet"),
                                children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(WiFi.gatewayIP());
    return false;
  }
};

class TagHostNw : public TagBase{
 public:
  TagHostNw(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "nw"),
                          children{new TagHostNwAddress(COMMON_PERAMS),
                                   new TagHostNwGateway(COMMON_PERAMS),
                                   new TagHostNwSubnet(COMMON_PERAMS)} { }
  TagBase* children[3];
};

class TagHostNwconfiguredAddress : public TagBase{
 public:
  TagHostNwconfiguredAddress(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "address"),
                                           children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(config->ip);
    return false;
  }
};

class TagHostNwconfiguredGateway : public TagBase{
 public:
  TagHostNwconfiguredGateway(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "gateway"),
                                           children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(config->gateway);
    return false;
  }
};

class TagHostNwconfiguredSubnet : public TagBase{
 public:
  TagHostNwconfiguredSubnet(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "subnet"),
                                          children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(config->subnet);
    return false;
  }
};

class TagHostNwconfigured : public TagBase{
 public:
  TagHostNwconfigured(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "nwconfigured"),
                                    children{new TagHostNwAddress(COMMON_PERAMS),
                                             new TagHostNwGateway(COMMON_PERAMS),
                                             new TagHostNwSubnet(COMMON_PERAMS)} { }
  TagBase* children[3];
};

class TagHostSsidsName : public TagBase{
 public:
  TagHostSsidsName(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "name"),
                                          children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t index, String& content, int& value){
    parent->contentCount();

    value = parent->contentCount();//WiFi.RSSI(index);
    content = WiFi.SSID(index);
    return (index < parent->contentCount());
  }
};

class TagHostSsidsSignal : public TagBase{
 public:
  TagHostSsidsSignal(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "signal"),
                                          children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t index, String& content, int& value){
    parent->contentCount();

    value = WiFi.RSSI(index);
    content = WiFi.SSID(index);
    //content = value;
    //content += "dBm";
    return (index < parent->contentCount());
  }
};

class TagHostSsids : public TagBase{
 public:
  TagHostSsids(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "ssids"),
                                    children{new TagHostSsidsName(COMMON_PERAMS),
                                             new TagHostSsidsSignal(COMMON_PERAMS)} { }
  TagBase* children[2];
  
  unsigned int contentCount(){
    static const uint8_t UPDATE_EVERY = 10;  // seconds.
    uint32_t now = millis() / 1000;
    static uint32_t last_updated = 0;
    static int8_t wifi_count = -1;
    if(last_updated + UPDATE_EVERY <= now || wifi_count < 0){
      wifi_count = WiFi.scanNetworks();
    }
    last_updated = now;
    return wifi_count;
  }
};

class TagHost : public TagBase{
 public:
  TagHost(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "host"),
                        children{new TagHostHostname(COMMON_PERAMS),
                                 new TagHostMac(COMMON_PERAMS),
                                 new TagHostUptime(COMMON_PERAMS),
                                 new TagHostRssi(COMMON_PERAMS),
                                 new TagHostCore(COMMON_PERAMS),
                                 new TagHostNw(COMMON_PERAMS),
                                 new TagHostNwconfigured(COMMON_PERAMS),
                                 new TagHostSsids(COMMON_PERAMS),
                        } { }
  TagBase* children[8];
};

class TagRoot : public TagBase{
 public:
  TagRoot(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "root"), 
                        children{new TagHost(COMMON_PERAMS),
                                 new TagSession(COMMON_PERAMS)} {}
  TagBase* children[2];
};


#endif  // ESP8266__TAGS_H
