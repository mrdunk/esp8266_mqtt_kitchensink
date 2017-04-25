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


#define COMMON_DEF Config* _config, MdnsLookup* _brokers, mdns::MDns* _mdns, Mqtt* _mqtt, Io* _io, const String& _session_token

#define COMMON_PERAMS _config, _brokers, _mdns, _mqtt, _io, _session_token

#define COMMON_ALLOCATE config(_config), brokers(_brokers), mdns(_mdns), mqtt(_mqtt), io(_io), session_token(_session_token), base_children(_base_children)

class TagBase{
 public:
  TagBase(const TagBase** _base_children, COMMON_DEF, const char* _name) : 
          COMMON_ALLOCATE, name(_name) {}

  void contentText(String& content, int& value){
    content = "";
    value = 0;
  }
  void contentsAt(unsigned int /*index*/, String& content){
    content = "";
  }
  unsigned int contentCount(){
    return 0;
  }
  const TagBase* getChild(const String* name_list, unsigned int list_pointer=0){
    Serial.println(name);
    Serial.println(sizeof(base_children));
    Serial.println(sizeof(base_children[0]));
    for(unsigned int child = 0; child < sizeof(base_children)/sizeof(base_children[0]); child++){
      Serial.print(base_children[child]->name);
      if(String(base_children[child]->name) == name_list[list_pointer]){
        Serial.println(" match!");
        return base_children[child];
      }
      Serial.println();
    }
    Serial.print("no match for ");
    Serial.println(name_list[list_pointer]);
    return NULL;
  }
 
 protected:
  Config* config;
  MdnsLookup* brokers;
  mdns::MDns* mdns;
  Mqtt* mqtt;
  Io* io;
  const String& session_token;
  const TagBase** base_children;
  const char* name;
};

class TagSessionValid : public TagBase{
 public:
  TagSessionValid(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "valid"), children{} { }
  const TagBase* children[0];
  
  void contentValue(String& content, int& value){
    const unsigned int now = millis() / 1000;
    const int remaining = config->session_time + SESSION_TIMEOUT - now;
    if(config->sessionValid(session_token) && remaining > 0){
      content = "valid for ";
      content += remaining;
      value = 1;
    } else {
      content = "expired";
      value = 0;
    }
  }
};

class TagSessionValiduntil : public TagBase{
 public:
  TagSessionValiduntil(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "validuntil"),
                                     children{} { }
  const TagBase* children[0];
};

class TagSessionProvidedtoken : public TagBase{
 public:
  TagSessionProvidedtoken(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "providedtoken"),
                                        children{} { }
  const TagBase* children[0];
};

class TagSessionExpectedtoken : public TagBase{
 public:
  TagSessionExpectedtoken(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "expectedtoken"),
                                        children{} { }
  const TagBase* children[0];
};

class TagSession : public TagBase{
 public:
  TagSession(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "session"),
                           children{new TagSessionValid(COMMON_PERAMS),
                                    new TagSessionValiduntil(COMMON_PERAMS),
                                    new TagSessionProvidedtoken(COMMON_PERAMS),
                                    new TagSessionExpectedtoken(COMMON_PERAMS)} { }
  const TagBase* children[4];
};

class TagHostHostname : public TagBase{
 public:
  TagHostHostname(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "hostname"),
                                children{} { }
  const TagBase* children[0];
};

class TagHostMac : public TagBase{
 public:
  TagHostMac(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "mac"),
                           children{} { }
  const TagBase* children[0];
};

class TagHostNwAddress : public TagBase{
 public:
  TagHostNwAddress(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "address"),
                                 children{} { }
  const TagBase* children[0];
};

class TagHostNwGateway : public TagBase{
 public:
  TagHostNwGateway(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "gaeteway"),
                                 children{} { }
  const TagBase* children[0];
};

class TagHostNwSubnet : public TagBase{
 public:
  TagHostNwSubnet(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "subnet"),
                                children{} { }
  const TagBase* children[0];
};

class TagHostNw : public TagBase{
 public:
  TagHostNw(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "nw"),
                          children{new TagHostNwAddress(COMMON_PERAMS),
                                   new TagHostNwGateway(COMMON_PERAMS),
                                   new TagHostNwSubnet(COMMON_PERAMS)} { }
  const TagBase* children[3];
};

class TagHostNwconfiguredAddress : public TagBase{
 public:
  TagHostNwconfiguredAddress(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "address"),
                                           children{} { }
  const TagBase* children[0];
};

class TagHostNwconfiguredGateway : public TagBase{
 public:
  TagHostNwconfiguredGateway(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "gaeteway"),
                                           children{} { }
  const TagBase* children[0];
};

class TagHostNwconfiguredSubnet : public TagBase{
 public:
  TagHostNwconfiguredSubnet(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "subnet"),
                                          children{} { }
  const TagBase* children[0];
};

class TagHostNwconfigured : public TagBase{
 public:
  TagHostNwconfigured(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "nwconfigured"),
                                    children{new TagHostNwAddress(COMMON_PERAMS),
                                             new TagHostNwGateway(COMMON_PERAMS),
                                             new TagHostNwSubnet(COMMON_PERAMS)} { }
  const TagBase* children[3];
};

class TagHost : public TagBase{
 public:
  TagHost(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "host"),
                        children{new TagHostHostname(COMMON_PERAMS),
                                 new TagHostMac(COMMON_PERAMS),
                                 new TagHostNw(COMMON_PERAMS),
                                 new TagHostNwconfigured(COMMON_PERAMS)} { }
  const TagBase* children[4];
};

class TagRoot : public TagBase{
 public:
  TagRoot(COMMON_DEF) : TagBase(children, COMMON_PERAMS, "root"), 
                        children{new TagHost(COMMON_PERAMS),
                                 new TagSession(COMMON_PERAMS)} {}
  const TagBase* children[2];
};


#endif  // ESP8266__TAGS_H
