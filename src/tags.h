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

#include <ArduinoJson.h>        // ArduinoJson library.
#include "FS.h"

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

  bool contentsSaveWrapper(const String& content){
    bool return_val = contentsSave(content);
    if(return_val){
      config->save();

      // Force reconnect to MQTT so we subscribe to any new addresses.
      // TODO: only do this if io has changed.
      mqtt->forceDisconnect();
      io->setup();
    }
    return return_val;
  }

  virtual bool contentsSave(const String& /*content*/){
    configurable = false;
    direct_value = false;
    return false;
  }

  virtual uint8_t contentCount(){
    return 0;
  }

  TagBase* matchPath(const String* name_list, uint8_t list_pointer=0) {
    if(strcmp(name, name_list[list_pointer].c_str()) == 0 || list_pointer >= MAX_TAG_RECURSION){
      return parent;
    }
    for(uint8_t child = 0; child < children_len; child++){
      return matchPath(name_list, list_pointer +1);
    }
    return this;
  }

  TagBase* getChild(uint8_t index){
    if(index > children_len){
      return nullptr;
    }
    base_children[index]->parent = this;
    return base_children[index];
  }

  TagBase* getParent(){
    return parent;
  }

  String getPath(){
    String path = name;
    TagBase* p = parent;
    if(p && strcmp(p->name, "root") != 0){
      if(p->contentCount() > 0){
        path = "." + path;
        path = sequence + path;
      };
      path = p->getPath() + "." + path;
    }

    return path;
  }
    
  bool sendData(std::function< void(String&, String&) > callback){
    String content;
    int value;
    //DynamicJsonBuffer jsonBuffer;
    StaticJsonBuffer<150> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["name"] = getPath();
    root["_command"] = "teach";

    bool return_val = contentsAt(sequence, content, value);

    if(content == "" && (parent == nullptr || parent->contentCount() == 0)){
      return true;
    }

    if(content == ""){
      content = "_";
    }

    root["content"] = content;
    root["value"] = value;
    root["sequence"] = sequence;
    if(parent){
      root["total"] = parent->contentCount();
    }

    String host_topic = "";
    String host_payload = "";
    root.printTo(host_payload);

    //Serial.println(host_payload);

    callback(host_topic, host_payload);

    return return_val;
  }

 protected:
  Config* config;
  MdnsLookup* brokers;
  mdns::MDns* mdns;
  Mqtt* mqtt;
  Io* io;
  TagBase** base_children;
  TagBase* parent;
 public:
  const uint8_t children_len;
  const char* name;
  uint8_t sequence;
  bool configurable;
  bool direct_value;
};

class TagUiIopinValue : public TagBase{
 public:
  TagUiIopinValue(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "value"),
                        children{
                        } {}
  TagBase* children[0];
  
  int values[11] = {0,1,2,3,4,5,12,13,14,15,16};  // Valid output pins.

  bool contentsAt(uint8_t index, String& content, int& value){
    value = values[index];
    content = value;
    return (index < 10);
  }
};

class TagUiIopinSelected : public TagBase{
 public:
  TagUiIopinSelected(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "selected"),
                        children{
                        } {}
  TagBase* children[0];
  
  int values[11] = {0,1,2,3,4,5,12,13,14,15,16};  // Valid output pins.

  bool contentsAt(uint8_t index, String& content, int& value){
    value = 0;
    content = "un-set";

    if(strcmp(getParent()->getParent()->name, "enable") == 0){
      value = (config->enableiopin == values[index]);
      content = value;
    } else if(strcmp(getParent()->getParent()->name, "io") == 0){
      // Not every config->devices entry is populated.
      value = config->labelToIndex(getParent()->sequence);
      
      content = (config->devices[value].iopin == values[index]);
    }

    return (index < 10);
  }
};

class TagUiIopin : public TagBase{
 public:
  TagUiIopin(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "iopin"),
                        children{new TagUiIopinValue(COMMON_PERAMS),
                                 new TagUiIopinSelected(COMMON_PERAMS)
                        } {
    configurable = true;
    direct_value = true;
                        }
  TagBase* children[2];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = "un-set";

    if(strcmp(getParent()->name, "enable") == 0){
      value = config->enableiopin;
      content = value;
    } else if(strcmp(getParent()->name, "io") == 0){
      // Not every config->devices entry is populated.
      value = config->labelToIndex(sequence);
      
      content = config->devices[value].iopin;
      value = config->devices[value].iopin;
    }

    return false;
  }

  uint8_t contentCount(){
    return 11;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagUiIoPin.contentsSave(");
    Serial.print(content);
    Serial.println(")");
    Serial.println(getParent()->name);

    if(strcmp(getParent()->name, "enable") == 0){
      config->enableiopin = content.toInt();
    } else if(strcmp(getParent()->name, "io") == 0){
      // Not every config->devices entry is populated.
      uint8_t value = config->labelToIndex(sequence);

      config->devices[value].iopin = content.toInt();
    }
    return true;
  }
};

class TagUiIotypeSelected : public TagBase{
 public:
  TagUiIotypeSelected(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "selected"),
                        children{
                        } {}
  TagBase* children[0];

  bool contentsAt(uint8_t index, String& content, int& value){
    // Not every config->devices entry is populated.
    value = config->labelToIndex(getParent()->sequence);

    content = (config->devices[value].io_type == index);

    return (index < 5);
  }
};

class TagUiIotypeValue : public TagBase{
 public:
  TagUiIotypeValue(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "value"),
                        children{
                        } {}
  TagBase* children[0];

  const char values[6][12] = {"test", "onoff", "pwm", "inputpullup", "input", "timer"};
  
  bool contentsAt(uint8_t index, String& content, int& value){
    value = index;
    content = values[index];
    return (index < 5);
  }
};

class TagUiIotype : public TagBase{
 public:
  TagUiIotype(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "iotype"),
                        children{new TagUiIotypeValue(COMMON_PERAMS),
                                 new TagUiIotypeSelected(COMMON_PERAMS)
                        } {
    configurable = true;
                        }
  TagBase* children[2];
  
  uint8_t contentCount(){
    return 6;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagUiIoType.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    // Not every config->devices entry is populated.
    uint8_t value = config->labelToIndex(sequence);

    config->devices[value].setType(content);
    return true;
  }
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
      content = "0";
      value = 0;
    }
    return false;
  }

  uint8_t contentCount(){
    const unsigned int now = millis() / 1000;
    const int remaining = config->session_time + SESSION_TIMEOUT - now;
    if(config->sessionValid() && remaining > 0){
      return 1;
    }
    return 0;
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

class TagSessionEnablePassword : public TagBase{
 public:
  TagSessionEnablePassword(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "password"),
                                children{} {
    configurable = true;
                                }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = "****";
    return false;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagSessionEnablePassword.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    content.toCharArray(config->enablepassphrase, STRING_LEN);
    return true;
  }
};

class TagSessionEnable : public TagBase{
 public:
  TagSessionEnable(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "enable"),
                                        children{
                                          new TagUiIopin(COMMON_PERAMS),
                                          new TagSessionEnablePassword(COMMON_PERAMS),
                                        } {}
  TagBase* children[2];
};

class TagSession : public TagBase{
 public:
  TagSession(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "session"),
                           children{new TagSessionValid(COMMON_PERAMS),
                                    new TagSessionValiduntil(COMMON_PERAMS),
                                    new TagSessionProvidedtoken(COMMON_PERAMS),
                                    new TagSessionExpectedtoken(COMMON_PERAMS),
                                    new TagSessionOverrideauth(COMMON_PERAMS),
                                    new TagSessionEnable(COMMON_PERAMS),
                           } { }
  TagBase* children[6];
};

class TagHostHostname : public TagBase{
 public:
  TagHostHostname(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "hostname"),
                                children{} {
    configurable = true;
                                }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = config->hostname;
    return false;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagHostHostname.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    content.toCharArray(config->hostname, HOSTNAME_LEN);
    sanitizeHostname(config->hostname);
    WiFi.hostname(config->hostname);
    return true;
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
  TagHostUptime(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "uptime"),
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
    content = ESP.getFlashChipSpeed() / 1000000;
    content += "MHz";
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
    value = ESP.getCycleCount();
    content = value;
    return false;
  }
};

class TagHostCoreUptime : public TagBase{
 public:
  TagHostCoreUptime(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "uptime"),
                                 children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = millis() / 1000;
    content = value;
    content += "seconds";
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
                                   new TagHostCoreCpucycles(COMMON_PERAMS),
                                   new TagHostCoreUptime(COMMON_PERAMS)
                          } { }
  TagBase* children[12];
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
    content = ip_to_string(WiFi.gatewayIP());
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
    content = ip_to_string(WiFi.subnetMask());
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
  TagHostNwconfiguredAddress(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "address"),
                                           children{} {
    configurable = true;
                                           }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(config->ip);
    return false;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagHostNwconfiguredAddress.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    config->ip = string_to_ip(content);
    return true;
  }
};

class TagHostNwconfiguredGateway : public TagBase{
 public:
  TagHostNwconfiguredGateway(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "gateway"),
                                           children{} {
    configurable = true;
                                           }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(config->gateway);
    return false;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagHostNwconfiguredGateway.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    config->gateway = string_to_ip(content);
    return true;
  }
};

class TagHostNwconfiguredSubnet : public TagBase{
 public:
  TagHostNwconfiguredSubnet(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "subnet"),
                                          children{} {
    configurable = true;
                                          }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = ip_to_string(config->subnet);
    return false;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagHostNwconfiguredSubnet.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    config->subnet = string_to_ip(content);
    return true;
  }
};

class TagHostNwconfigured : public TagBase{
 public:
  TagHostNwconfigured(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "nwconfigured"),
                                    children{new TagHostNwconfiguredAddress(COMMON_PERAMS),
                                             new TagHostNwconfiguredGateway(COMMON_PERAMS),
                                             new TagHostNwconfiguredSubnet(COMMON_PERAMS)} { }
  TagBase* children[3];
};

class TagHostSsidsName : public TagBase{
 public:
  TagHostSsidsName(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "name"),
                                          children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t index, String& content, int& value){
    parent->contentCount();

    value = WiFi.RSSI(index);
    content = WiFi.SSID(index);
    return (index < parent->contentCount() -1);
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
    content = value;
    content += "dBm";
    return (index < parent->contentCount() -1);
  }
};

class TagHostSsids : public TagBase{
 public:
  TagHostSsids(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "ssids"),
                                    children{new TagHostSsidsName(COMMON_PERAMS),
                                             new TagHostSsidsSignal(COMMON_PERAMS)} { }
  TagBase* children[2];
  
  uint8_t contentCount(){
    static const uint8_t UPDATE_EVERY = 30;  // seconds.
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

class TagHostMqttBrokerAddress : public TagBase{
 public:
  TagHostMqttBrokerAddress(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "address"),
                                    children{ } {
    configurable = true;
                                    }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = config->brokerip;
    content = ip_to_string(config->brokerip);
    return false;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagHostMqttBrokerAddress.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    config->brokerip = string_to_ip(content);
    return true;
  }
};
  
class TagHostMqttBrokerPort : public TagBase{
 public:
  TagHostMqttBrokerPort(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "port"),
                                    children{ } {
    configurable = true;
                                    }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = config->brokerport;
    content = config->brokerport;
    return false;
  }

  bool contentsSave(const String& content){
    Serial.print("TagHostMqttBrokerPort.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    config->brokerport = content.toInt();
    return true;
  }
};
  
class TagHostMqttBroker : public TagBase{
 public:
  TagHostMqttBroker(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "broker"),
                                    children{new TagHostMqttBrokerAddress(COMMON_PERAMS),
                                             new TagHostMqttBrokerPort(COMMON_PERAMS),
                                    } { }
  TagBase* children[2];
};
  
class TagHostMqttSubscriptionprefix : public TagBase{
 public:
  TagHostMqttSubscriptionprefix(COMMON_DEF) : 
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "subscribe_prefix"),
                                    children{ } {

    configurable = true;
                                    }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = config->subscribeprefix;
    return false;
  }

  bool contentsSave(const String& content){
    Serial.print("TagHostMqttSubscriptionprefix.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    content.toCharArray(config->subscribeprefix, PREFIX_LEN);
    sanitizeTopic(config->subscribeprefix);
    return true;
  }
};
  
class TagHostMqttPublishprefix : public TagBase{
 public:
  TagHostMqttPublishprefix(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "publish_prefix"),
                                    children{ } {
    configurable = true;
                                    }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = config->publishprefix;
    return false;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagHostMqttPublishprefix.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    content.toCharArray(config->publishprefix, PREFIX_LEN);
    sanitizeTopic(config->publishprefix);
    return true;
  }
};
  
class TagHostMqtt : public TagBase{
 public:
  TagHostMqtt(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "mqtt"),
                                    children{new TagHostMqttBroker(COMMON_PERAMS),
                                             new TagHostMqttSubscriptionprefix(COMMON_PERAMS),
                                             new TagHostMqttPublishprefix(COMMON_PERAMS),
                                    } { }
  TagBase* children[3];
};
  
class TagHostHttpAddress : public TagBase{
 public:
  TagHostHttpAddress(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "address"),
                                    children{ } {
    configurable = true;
                                    }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = config->firmwarehost;
    return false;
  }

  bool contentsSave(const String& content){
    Serial.print("TagHostHttpAddress.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    if(sanitizeFilename(content)){
      content.toCharArray(config->firmwarehost, STRING_LEN);
      return true;
    }
    return false;
  }
};
  
class TagHostHttpPort : public TagBase{
 public:
  TagHostHttpPort(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "port"),
                                    children{ } {
    configurable = true;
                                    }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = config->firmwareport;
    content = config->firmwareport;
    return false;
  }

  bool contentsSave(const String& content){
    Serial.print("TagHostHttpPort.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    config->firmwareport = content.toInt();
    return true;
  }
};
  
class TagHostHttpDirectory : public TagBase{
 public:
  TagHostHttpDirectory(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "directory"),
                                    children{ } {
    configurable = true;
                                    }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = 0;
    content = config->firmwaredirectory;
    return false;
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagHostHttpDirectory.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    if(sanitizeFilePath(content)){
      content.toCharArray(config->firmwaredirectory, STRING_LEN);
      return true;
    }
    return false;
  }
};
  
class TagHostHttp : public TagBase{
 public:
  TagHostHttp(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "http"),
                                    children{new TagHostHttpAddress(COMMON_PERAMS),
                                             new TagHostHttpPort(COMMON_PERAMS),
                                             new TagHostHttpDirectory(COMMON_PERAMS),
                                    } { }
  TagBase* children[3];
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
                                 new TagHostMqtt(COMMON_PERAMS),
                                 new TagHostHttp(COMMON_PERAMS),
                        } { }
  TagBase* children[10];
};

class TagServersMqttActive : public TagBase{
 public:
  TagServersMqttActive(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "active"),
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = (active ? "Y":"N");
    value = active; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttServicename : public TagBase{
 public:
  TagServersMqttServicename(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "service_name"), 
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = p_host->service_name;
    value = active; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttHostname : public TagBase{
 public:
  TagServersMqttHostname(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "host_name"),
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = p_host->host_name;
    value = active; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttAddress : public TagBase{
 public:
  TagServersMqttAddress(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "address"),
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = ip_to_string(p_host->address);
    value = active; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttPort : public TagBase{
 public:
  TagServersMqttPort(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "port"),
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = p_host->port;
    value = p_host->port; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttServicevaliduntil : public TagBase{
 public:
  TagServersMqttServicevaliduntil(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "service_valid_until"),
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = p_host->service_valid_until;
    value = p_host->service_valid_until; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttHostvaliduntil : public TagBase{
 public:
  TagServersMqttHostvaliduntil(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "host_valid_until"), 
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = p_host->host_valid_until;
    value = p_host->host_valid_until; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttAddressvaliduntil : public TagBase{
 public:
  TagServersMqttAddressvaliduntil(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "address_valid_until"),
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = p_host->ipv4_valid_until;
    value = p_host->ipv4_valid_until; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttSuccesscounter : public TagBase{
 public:
  TagServersMqttSuccesscounter(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "success_counter"),
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    content = p_host->success_counter;
    value = p_host->success_counter; 
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqttConnectionattempts : public TagBase{
 public:
  TagServersMqttConnectionattempts(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "connection_attempts"),
                        children{ } {}
  TagBase* children[0];
  bool contentsAt(uint8_t index, String& content, int& value){
    Host* p_host;
    bool active;
    brokers->SetIterater(index);
    brokers->GetLastHost(&p_host, active);
    value = p_host->success_counter + p_host->fail_counter; 
    content = value;
    return (index < parent->contentCount() -1);
  }
};

class TagServersMqtt : public TagBase{
 public:
  TagServersMqtt(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "mqtt"), 
                        children{new TagServersMqttActive(COMMON_PERAMS),
                                 new TagServersMqttServicename(COMMON_PERAMS),
                                 new TagServersMqttHostname(COMMON_PERAMS),
                                 new TagServersMqttAddress(COMMON_PERAMS),
                                 new TagServersMqttPort(COMMON_PERAMS),
                                 new TagServersMqttServicevaliduntil(COMMON_PERAMS),
                                 new TagServersMqttHostvaliduntil(COMMON_PERAMS),
                                 new TagServersMqttAddressvaliduntil(COMMON_PERAMS),
                                 new TagServersMqttSuccesscounter(COMMON_PERAMS),
                                 new TagServersMqttConnectionattempts(COMMON_PERAMS)
                        } {}
  TagBase* children[10];
  
  uint8_t contentCount(){
    Host* p_host;
    bool active;
    uint8_t count = 0;
    brokers->ResetIterater();
    while(brokers->IterateHosts(&p_host, &active)){
      count++;
    }
    brokers->ResetIterater();
    return count;
  }
};

class TagServers : public TagBase{
 public:
  TagServers(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "servers"),
                        children{new TagServersMqtt(COMMON_PERAMS),
                        } {}
  TagBase* children[1];
};

class TagMdnsBuffersuccess : public TagBase{
 public:
  TagMdnsBuffersuccess(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "buffer_success"),
                                        children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = mdns->packet_count - mdns->buffer_size_fail;
    content = value;
    return false;
  }
};

class TagMdnsPacketcount : public TagBase{
 public:
  TagMdnsPacketcount(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "packet_count"),
                                        children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = mdns->packet_count;
    content = value;
    return false;
  }
};

class TagMdnsSuccessrate : public TagBase{
 public:
  TagMdnsSuccessrate(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "success_rate"),
                                        children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    if(mdns->packet_count > 0){
      value = 100 - (100 * mdns->buffer_size_fail / mdns->packet_count);
      content = value;
      content += "%";
    }
    return false;
  }
};

class TagMdnsLargestpacket : public TagBase{
 public:
  TagMdnsLargestpacket(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "largest_packet"),
                                        children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = mdns->largest_packet_seen;
    content = value;
    return false;
  }
};

class TagMdnsBuffersize : public TagBase{
 public:
  TagMdnsBuffersize(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "buffer_size"),
                                        children{} { }
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    value = BUFFER_SIZE;
    content = value;
    return false;
  }
};

class TagMdns : public TagBase{
 public:
  TagMdns(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "mdns"),
                        children{new TagMdnsBuffersuccess(COMMON_PERAMS),
                                 new TagMdnsPacketcount(COMMON_PERAMS),
                                 new TagMdnsSuccessrate(COMMON_PERAMS),
                                 new TagMdnsLargestpacket(COMMON_PERAMS),
                                 new TagMdnsBuffersize(COMMON_PERAMS)
                        } {}
  TagBase* children[5];
};

class TagFsFilesFilename : public TagBase{
 public:
  TagFsFilesFilename(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "filename"),
                        children{
                        } {}
  TagBase* children[0];
  
  bool contentsAt(uint8_t index, String& content, int& value){
    unsigned int file_count = parent->contentCount();

    uint16_t pointer = 1;
    for(uint8_t i = 0; i < index; i++){
      pointer = config->files.indexOf('|', pointer) +2;
    }
    uint16_t pointer_end = config->files.indexOf(',', pointer);
    value = index;
    content = config->files.substring(pointer, pointer_end);
    return (index < file_count -1);
  }
};

class TagFsFilesSize : public TagBase{
 public:
  TagFsFilesSize(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "size"),
                        children{
                        } {}
  TagBase* children[0];
  
  bool contentsAt(uint8_t index, String& content, int& value){
    unsigned int file_count = parent->contentCount();

    uint16_t pointer = 1;
    for(uint8_t i = 0; i < index; i++){
      pointer = config->files.indexOf('|', pointer) +2;
    }
    uint16_t pointer_start = config->files.indexOf(',', pointer) +1;
    uint16_t pointer_end = config->files.indexOf('|', pointer);
    value = config->files.substring(pointer_start, pointer_end).toInt();
    content = ((float)value / 1024);
    content += "k";
    return (index < file_count -1);
  }
};

class TagFsFilesIsmustache : public TagBase{
 public:
  TagFsFilesIsmustache(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "is_mustache"),
                        children{
                        } {}
  TagBase* children[0];
  
  bool contentsAt(uint8_t index, String& content, int& value){
    String file_content;
    int file_value;
    bool return_val = false;

    for(uint8_t i = 0; i < parent->children_len; i++){
      if(strcmp(parent->getChild(i)->name, "filename") == 0){
        return_val = parent->getChild(i)->contentsAt(index, file_content, file_value);
      }
    }
    value = (int)(file_content.endsWith(".mustache"));
    content = ((bool)value ? "Y":"N");
    return return_val;
  }

  uint8_t contentCount(){
    String content;
    int value;
    parent->contentsAt(0, content, value);
    return (bool)value;
  }
};

class TagFsFiles : public TagBase{
 public:
  TagFsFiles(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "files"),
                        children{new TagFsFilesFilename(COMMON_PERAMS),
                                 new TagFsFilesSize(COMMON_PERAMS),
                                 new TagFsFilesIsmustache(COMMON_PERAMS)
                        } {}
  TagBase* children[3];
  
  uint8_t contentCount(){
    int8_t file_count = 0;
    if(config->files == "0"){
      if(!SPIFFS.begin()){
        Serial.println("WARNING: Unable to SPIFFS.begin()");
        return 0;
      }
      config->files = "";
      Dir dir = SPIFFS.openDir("/");
      while(dir.next()){
        file_count++;
        File file = dir.openFile("r");
        config->files += file.name();
        config->files += ",";
        config->files += file.size();
        config->files += "|";
        file.close();
      }
      SPIFFS.end();
    } else {
      for (uint16_t i=0; i < config->files.length(); i++){
        file_count += (config->files[i] == '|');
      }
    }
    return file_count;
  }
};

class TagFsSpaceSize: public TagBase{
 public:
  TagFsSpaceSize(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "size"),
                        children{
                        } {}
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    if(!SPIFFS.begin()){
      Serial.println("WARNING: Unable to SPIFFS.begin()");
      return 0;
    }
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    value = fs_info.totalBytes;
    content = (value / 1024);
    content += "k";
    SPIFFS.end();
    return false;
  }
};

class TagFsSpaceUsed : public TagBase{
 public:
  TagFsSpaceUsed(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "used"),
                        children{
                        } {}
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    if(!SPIFFS.begin()){
      Serial.println("WARNING: Unable to SPIFFS.begin()");
      return 0;
    }
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    value = fs_info.usedBytes;
    content = (value / 1024);
    content += "k";
    SPIFFS.end();
    return false;
  }
};

class TagFsSpaceRemaining : public TagBase{
 public:
  TagFsSpaceRemaining(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "remaining"),
                        children{
                        } {}
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    if(!SPIFFS.begin()){
      Serial.println("WARNING: Unable to SPIFFS.begin()");
      return 0;
    }
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    value = fs_info.totalBytes - fs_info.usedBytes;
    content = (value / 1024);
    content += "k";
    SPIFFS.end();
    return false;
  }
};

class TagFsSpaceRatioused : public TagBase{
 public:
  TagFsSpaceRatioused(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "ratio_used"),
                        children{
                        } {}
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    if(!SPIFFS.begin()){
      Serial.println("WARNING: Unable to SPIFFS.begin()");
      return 0;
    }
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(fs_info.totalBytes > 0){
      value = (100.0 * fs_info.usedBytes / fs_info.totalBytes);
      content = value;
      content += "%";
    }
    SPIFFS.end();
    return false;
  }
};

class TagFsSpaceRatioremaining : public TagBase{
 public:
  TagFsSpaceRatioremaining(COMMON_DEF) :
    TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "ratio_remaining"),
                        children{
                        } {}
  TagBase* children[0];
  
  bool contentsAt(uint8_t /*index*/, String& content, int& value){
    if(!SPIFFS.begin()){
      Serial.println("WARNING: Unable to SPIFFS.begin()");
      return 0;
    }
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(fs_info.totalBytes > 0){
      value = (100.0 * (fs_info.totalBytes - fs_info.usedBytes) / fs_info.totalBytes);
      content = value;
      content += "%";
    }
    SPIFFS.end();
    return false;
  }
};

class TagFsSpace : public TagBase{
 public:
  TagFsSpace(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "space"),
                        children{new TagFsSpaceSize(COMMON_PERAMS),
                                 new TagFsSpaceUsed(COMMON_PERAMS),
                                 new TagFsSpaceRemaining(COMMON_PERAMS),
                                 new TagFsSpaceRatioused(COMMON_PERAMS),
                                 new TagFsSpaceRatioremaining(COMMON_PERAMS)
                        } {}
  TagBase* children[5];
};

class TagFs : public TagBase{
 public:
  TagFs(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "fs"),
                        children{new TagFsFiles(COMMON_PERAMS),
                                 new TagFsSpace(COMMON_PERAMS)
                        } {}
  TagBase* children[2];
};

class TagIoIndex : public TagBase{
 public:
  TagIoIndex(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "index"),
                        children{
                        } {}

  TagBase* children[0];

  bool contentsAt(uint8_t index, String& content, int& value){
    // Not every config->devices entry is populated.
    value = config->labelToIndex(index);

    content = value;
    return (bool)(getParent()->contentCount() - index -1);
  }
};

class TagIoTopic : public TagBase{
 public:
  TagIoTopic(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "topic"),
                        children{ } {
    configurable = true;
                        }

  TagBase* children[0];

  bool contentsAt(uint8_t index, String& content, int& value){
    // Not every config->devices entry is populated.
    value = config->labelToIndex(index);

    content = DeviceAddress(config->devices[value]);
    return (bool)(getParent()->contentCount() - index -1);
  }

  bool contentsSave(const String& content){
    Serial.print("TagIoTopic.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    // Not every config->devices entry is populated.
    uint8_t value = config->labelToIndex(sequence);

    DeviceAddressSet(config->devices[value], content);
    return true;
  }
};

class TagIoInverted : public TagBase{
 public:
  TagIoInverted(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "inverted"),
                        children{
                        } {}

  TagBase* children[0];

  bool contentsAt(uint8_t index, String& content, int& value){
    // Not every config->devices entry is populated.
    value = config->labelToIndex(index);
    
    content = config->devices[value].inverted ? "Y":"N";
    value = config->devices[value].inverted;
    return (bool)(getParent()->contentCount() - index -1);
  }
};

class TagIoDefault : public TagBase{
 public:
  TagIoDefault(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "default"),
                        children{ } {
    configurable = true;
                        }

  TagBase* children[0];

  bool contentsAt(uint8_t index, String& content, int& value){
    // Not every config->devices entry is populated.
    value = config->labelToIndex(index);

    content = config->devices[value].io_default;
    value = config->devices[value].io_default;
    return (bool)(getParent()->contentCount() - index -1);
  }
  
  bool contentsSave(const String& content){
    Serial.print("TagIoDefault.contentsSave(");
    Serial.print(content);
    Serial.println(")");

    // Not every config->devices entry is populated.
    uint8_t value = config->labelToIndex(sequence);

    config->devices[value].io_default = content.toInt();
    return true;
  }
};

class TagIo : public TagBase{
 public:
  TagIo(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "io"),
                        children{new TagIoIndex(COMMON_PERAMS),
                                 new TagIoTopic(COMMON_PERAMS),
                                 new TagIoInverted(COMMON_PERAMS),
                                 new TagIoDefault(COMMON_PERAMS),
                                 new TagUiIopin(COMMON_PERAMS),
                                 new TagUiIotype(COMMON_PERAMS)
                        } {}
  TagBase* children[6];

  uint8_t contentCount(){
    uint8_t count = 0;
    for(int i = 0; i < MAX_DEVICES; ++i) {
      if(strlen(config->devices[i].address_segment[0].segment) > 0) {
        count ++;
      }
    }
    return count;
  }
};

class TagRoot : public TagBase{
 public:
  TagRoot(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "root"), 
                        children{new TagHost(COMMON_PERAMS),
                                 new TagSession(COMMON_PERAMS),
                                 new TagServers(COMMON_PERAMS),
                                 new TagMdns(COMMON_PERAMS),
                                 new TagFs(COMMON_PERAMS),
                                 new TagIo(COMMON_PERAMS)
                        } {}
  TagBase* children[6];
};


class TagItterator{
 public: 
  TagItterator(TagRoot& tagRoot) {
    for(uint8_t i = 0; i < MAX_TAG_RECURSION; i++){
      tag[i] = nullptr;
    }
    tag[0] = &tagRoot;
    reset();
    last_loop = true;
  }

  TagBase* getByPath(const String& path){
    uint8_t path_pointer_head = 0;
    uint8_t path_pointer_tail = 0;
    TagBase* tag_pointer = tag[0];
    uint8_t sequence = 0;
    while(path_pointer_tail < path.length()){
      path_pointer_head = path_pointer_tail;
      path_pointer_tail = path.indexOf(".", path_pointer_head) +1;
      if(path_pointer_tail == 0){
        path_pointer_tail = path.length() +1;
      }

      if(path.substring(path_pointer_head, path_pointer_tail -1).toInt() ||
          path.substring(path_pointer_head, path_pointer_tail -1) == "0")
      {
        sequence = path.substring(path_pointer_head, path_pointer_tail -1).toInt();
      } else {
        TagBase* child;
        uint8_t i = 0;
        while((child = tag_pointer->getChild(i))){
          if(child != nullptr &&
              String(child->name) == path.substring(path_pointer_head, path_pointer_tail -1))
          {
            tag_pointer = child;
            tag_pointer->sequence = sequence;
            sequence = 0;
            break;
          }
          i++;
        }
      }
    }
    if(tag_pointer != nullptr){
      Serial.println(tag_pointer->getPath());
    }
    return tag_pointer;
  }

  void reset(){
    count[0] = 0;
    last_loop = false;
    for(uint8_t i = 1; i < MAX_TAG_RECURSION; i++){
      tag[i] = nullptr;
      count[i] = 0;
    }
  }

  TagBase* getSibling(TagBase* parent, TagBase* child){
    if(child == nullptr){
      return parent->getChild(0);
    }
    for(uint8_t sibling = 0; sibling < parent->children_len -1; sibling++){
      if(parent->getChild(sibling) == child){
        return parent->getChild(sibling +1);
      }
    }
    return nullptr;
  }

  TagBase* loop(uint8_t* p_depth = nullptr){
    uint8_t depth = 0;

    if(last_loop){
      if(p_depth != nullptr){
        *p_depth = 0;
      }
      return nullptr;
    }


    for(depth = 0; depth < MAX_TAG_RECURSION; depth++){
      if(tag[depth] == nullptr){
        depth--;
        break;
      }
    }

    TagBase* return_tag = tag[depth];
    return_tag->sequence = count[depth -1];

    Serial.print(depth);
    for(uint8_t i = 0; i < depth; i++){ Serial.print("  ");}
    Serial.print(" ");
    Serial.print(tag[depth]->name);
    Serial.print("\t");
    Serial.println(tag[depth]->sequence);

    if(tag[depth]->children_len > 0){
      tag[depth +1] = tag[depth]->getChild(0);
      count[depth +1] = 0;
    } else {
      uint8_t d = depth +1;
      while(tag[d] == nullptr){
        d--;
        
        if(d == 0){
          last_loop = true;
          break;
        }
          
        count[d]++;
        if(count[d] < tag[d]->contentCount()){
          break;
        }
        count[d] = 0;
        
        tag[d] = getSibling(tag[d -1], tag[d]);
      }
    }

    if(p_depth != nullptr){
      *p_depth = depth;
    }
    return return_tag;
  }

 private:
  TagBase* tag[MAX_TAG_RECURSION];
  int8_t count[MAX_TAG_RECURSION];
  std::function< void(String&, String&) > callback;
  bool last_loop;
};

#endif  // ESP8266__TAGS_H
