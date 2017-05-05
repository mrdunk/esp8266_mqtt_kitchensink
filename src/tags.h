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


#define COMMON_DEF Config* _config, MdnsLookup* _brokers, mdns::MDns* _mdns, Mqtt* _mqtt, Io* _io, const String& _session_token

#define COMMON_PERAMS _config, _brokers, _mdns, _mqtt, _io, _session_token

#define COMMON_ALLOCATE config(_config), brokers(_brokers), mdns(_mdns), mqtt(_mqtt), io(_io), session_token(_session_token), base_children(_base_children), parent(NULL), children_len(_children_len)

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

  unsigned int contentCount(){
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
  const String& session_token;
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
    if(config->sessionValid(session_token) && remaining > 0){
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
};

class TagSessionProvidedtoken : public TagBase{
 public:
  TagSessionProvidedtoken(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "providedtoken"),
                                        children{} { }
  TagBase* children[0];
};

class TagSessionExpectedtoken : public TagBase{
 public:
  TagSessionExpectedtoken(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "expectedtoken"),
                                        children{} { }
  TagBase* children[0];
};

class TagSession : public TagBase{
 public:
  TagSession(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "session"),
                           children{new TagSessionValid(COMMON_PERAMS),
                                    new TagSessionValiduntil(COMMON_PERAMS),
                                    new TagSessionProvidedtoken(COMMON_PERAMS),
                                    new TagSessionExpectedtoken(COMMON_PERAMS)} { }
  TagBase* children[4];
};

class TagHostHostname : public TagBase{
 public:
  TagHostHostname(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "hostname"),
                                children{} { }
  TagBase* children[0];
};

class TagHostMac : public TagBase{
 public:
  TagHostMac(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "mac"),
                           children{} { }
  TagBase* children[0];
};

class TagHostNwAddress : public TagBase{
 public:
  TagHostNwAddress(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "address"),
                                 children{} { }
  TagBase* children[0];
};

class TagHostNwGateway : public TagBase{
 public:
  TagHostNwGateway(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "gateway"),
                                 children{} { }
  TagBase* children[0];
};

class TagHostNwSubnet : public TagBase{
 public:
  TagHostNwSubnet(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "subnet"),
                                children{} { }
  TagBase* children[0];
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
};

class TagHostNwconfiguredGateway : public TagBase{
 public:
  TagHostNwconfiguredGateway(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "gateway"),
                                           children{} { }
  TagBase* children[0];
};

class TagHostNwconfiguredSubnet : public TagBase{
 public:
  TagHostNwconfiguredSubnet(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "subnet"),
                                          children{} { }
  TagBase* children[0];
};

class TagHostNwconfigured : public TagBase{
 public:
  TagHostNwconfigured(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "nwconfigured"),
                                    children{new TagHostNwAddress(COMMON_PERAMS),
                                             new TagHostNwGateway(COMMON_PERAMS),
                                             new TagHostNwSubnet(COMMON_PERAMS)} { }
  TagBase* children[3];
};

class TagHost : public TagBase{
 public:
  TagHost(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "host"),
                        children{new TagHostHostname(COMMON_PERAMS),
                                 new TagHostMac(COMMON_PERAMS),
                                 new TagHostNw(COMMON_PERAMS),
                                 new TagHostNwconfigured(COMMON_PERAMS)} { }
  TagBase* children[4];
};

class TagRoot : public TagBase{
 public:
  TagRoot(COMMON_DEF) : TagBase(children, CHILDREN_LEN, COMMON_PERAMS, "root"), 
                        children{new TagHost(COMMON_PERAMS),
                                 new TagSession(COMMON_PERAMS)} {}
  TagBase* children[2];
};


#endif  // ESP8266__TAGS_H
