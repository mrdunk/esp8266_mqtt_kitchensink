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

#include <ESP8266WiFi.h>
#include "FS.h"

#include "host_attributes.h"
#include "serve_files.h"
#include "ipv4_helpers.h"
#include "tags.h"

extern Config config;
extern TagRoot root_tag;
extern TagItterator tag_itterator;


// Ensure buffer contains only valid hostname characters.
void sanitizeHostname(char* buffer){
  for(unsigned int i=0; i < strlen(buffer);i++){
    if(buffer[i] >= 'A' && buffer[i] <= 'Z'){
      buffer[i] = buffer[i] + 'a' - 'A';
    } else if(buffer[i] >= 'a' && buffer[i] <= 'z'){
      // pass
    } else if(buffer[i] >= '0' && buffer[i] <= '9'){
      // pass
    } else if(buffer[i] == '-'){
      // pass
    } else {
      buffer[i] = '-';
    }
  }
}

void SetHostname(const char* new_hostname) {
  strncpy(config.hostname, new_hostname, HOSTNAME_LEN -1);
  config.hostname[HOSTNAME_LEN -1] = '\0';
  sanitizeHostname(config.hostname);
  WiFi.hostname(config.hostname);
}


bool validFileChar(const char letter, const uint16_t i){
  if(letter >= 'A' && letter <= 'Z'){
    // pass
  } else if(letter >= 'a' && letter <= 'z'){
    // pass
  } else if(letter >= '0' && letter <= '9'){
    // pass
  } else if(letter == '.' && i > 0){
    // Don't allow '.' as the first character in a file.
    // pass
  } else if(letter == '_'){
    // pass
  } else if(letter == '-'){
    // pass
  } else {
    return false;
  }
  return true;
}

bool sanitizeFilePath(const String& buffer){
  bool valid = true;
  for(uint16_t i=0; i < buffer.length();i++){
    bool valid_ = (validFileChar(buffer[i], i) || buffer[i] == '/');
    if(valid and !valid_){
      Serial.print("Invalid char in path: \"");
      Serial.print(buffer[i]);
      Serial.print("\"  at pos ");
      Serial.println(i);
    }
    valid &= valid_;
  }
  return valid;
}

bool sanitizeFilename(const String& buffer){
  bool valid = true;
  for(uint16_t i=0; i < buffer.length();i++){
    bool valid_ = validFileChar(buffer[i], i);
    if(valid and !valid_){
      Serial.print("Invalid char in filename: \"");
      Serial.print(buffer[i]);
      Serial.print("\"  at pos ");
      Serial.println(i);
    }
    valid &= valid_;
  }
  return valid;
}

void Config::clear(){
  hostname[0] = '\0';
  ip = IPAddress(0,0,0,0);
  gateway = IPAddress(0,0,0,0);
  subnet = IPAddress(255,255,255,0);
  brokerip = IPAddress(0,0,0,0);
  brokerport = 1883;
  subscribeprefix[0] = '\0';
  publishprefix[0] = '\0';
  for(int i = 0; i < MAX_DEVICES; i++){
    devices[i] = (const Connected_device){{0},Io_Type::test,0,0,0,false,true};
  }
  firmwarehost[0] = '\0';
  firmwaredirectory[0] = '\0';
  firmwareport = 0;
  enablepassphrase[0] = '\0';
  enableiopin = 0;
  wifi_ssid[0] = '\0';
  wifi_passwd[0] = '\0';
}

bool Config::sessionExpired(){
  //Serial.print("Time expires: ");
  //Serial.println(session_time + SESSION_TIMEOUT);

  const unsigned int now = millis() / 1000;
  //Serial.print("Time now:     ");
  //Serial.println(now);

  if((session_time + SESSION_TIMEOUT) <= now){
    session_token = 0;
    session_override = false;
    return true;
  }
  return false;
}

bool Config::sessionValid(){
  if(sessionExpired()){
    return false;
  }

  //Serial.print("session_key is:       ");
  //Serial.println(session_key);

  if(session_token_provided == session_token &&
      session_token_provided != 0)
  {
    //Serial.println("Authentication Successful");
    session_time = millis() / 1000;
    session_override = false;
    return true;
  }
  if(session_override){
    //Serial.println("Authentication override");
    session_time = millis() / 1000;
    return true;
  }
  //Serial.println("Authentication failed");
  return false;
}

void assemblePathObject(JsonObject& json_object, String& path);

void assemblePathArray(JsonArray& json_object, String& path){
  uint8_t counter = 0;
  String path_backup = path;
  for (auto dataobj : json_object){
    path = path_backup;
    path += counter++;
    path += ".";

    if(dataobj.is<char*>()){
      //Serial.println(path);
      //Serial.print("\t: ");
      //Serial.println(dataobj.as<char*>());
      TagBase* tag = tag_itterator.getByPath(path);
      if(tag != nullptr && tag->configurable){
        tag->contentsSave(dataobj.as<char*>());
      }
    } else if(dataobj.is<JsonArray>()){
      assemblePathArray(dataobj, path);
    } else {
      assemblePathObject(dataobj, path);
    }
  }
  path = path_backup;
}

void assemblePathObject(JsonObject& json_object, String& path){
  String path_backup = path;
  for (auto dataobj : json_object){
    path = path_backup;
    path += dataobj.key;
    path += ".";

    if(dataobj.value.is<char*>()){
      //Serial.print(path);
      //Serial.print("\t: ");
      //Serial.println(dataobj.value.as<char*>());
      TagBase* tag = tag_itterator.getByPath(path);
      if(tag != nullptr && tag->configurable){
        tag->contentsSave(dataobj.value.as<char*>());
      }
    } else if(dataobj.value.is<JsonArray>()){
      assemblePathArray(dataobj.value, path);
    } else {
      assemblePathObject(dataobj.value, path);
    }
  }
  path = path_backup;
}

bool Config::load(const String& filename){
  //Serial.print("Config::load(");
  //Serial.print(filename);
  //Serial.println(")");

  bool result = SPIFFS.begin();
  if(!result){
    Serial.println("Unable to use SPIFFS.");
    SPIFFS.end();
    return false;
  }

  // this opens the file in read-mode
  File file = SPIFFS.open(filename, "r");

  if (!file) {
    Serial.print("File doesn't exist: ");
    Serial.println(filename);
    SPIFFS.end();
    return false;
  }

  String line;
  String l;
  while(file.available()) {
      l = file.readStringUntil('\n');
      l.trim();
      line += l;
  }

  file.close();
  SPIFFS.end();

  StaticJsonBuffer<1500> jsonBuffer;
  //DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(line);
  if(!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }
  //Serial.println("parseObject() success");
  String path;
  assemblePathObject(root, path);

  //root.prettyPrintTo(line);
  //Serial.print(line);
  return true;
}

bool Config::save(const String& filename){
	Serial.print("Config::save(");
  Serial.print("filename");
  Serial.println(")");

  bool result = SPIFFS.begin();
  if(!result){
		Serial.println("Unable to use SPIFFS.");
    SPIFFS.end();
    return false;
  }

	// this opens the file in read-mode
	File file = SPIFFS.open(filename, "r");

	if (!file) {
		Serial.print("File doesn't exist yet. Creating it: ");
	} else {
		Serial.print("Overwriting file: ");
	}
  Serial.println(filename);
  file.close();
  SPIFFS.end();

  String out_buffer;
  out_buffer.reserve(1000);

  TagBase* path[MAX_TAG_RECURSION] = {nullptr};
  TagBase* path_last[MAX_TAG_RECURSION] = {nullptr};
  uint8_t depth = 0;

  tag_itterator.reset();
  while(true){
    TagBase* tag = tag_itterator.loop(&depth);
    if(tag == nullptr || tag->configurable){
      for(uint8_t i = 0; i < MAX_TAG_RECURSION; i++){
        path[i] = nullptr;
      }
      path[depth] = tag;
      for(int8_t i = depth -1; i >= 0; i--){
        path[i] = path[i +1]->getParent();
      }

      for(int8_t i = MAX_TAG_RECURSION -1; i >= 0; i--){
        if(path[i] != path_last[i] && path_last[i] != nullptr){
          if(path_last[i]->children_len > 0 && path_last[i]->direct_value == false){
            if(out_buffer.endsWith(",\n")){
              out_buffer.remove(out_buffer.length() -2, 1);
            }
            for(uint8_t j=0; j < i; j++){out_buffer.concat("  ");}
            out_buffer.concat("},\n");
          }
          if(path_last[i]->contentCount() > 0 && path_last[i]->direct_value == false){
            if(out_buffer.endsWith(",\n")){
              out_buffer.remove(out_buffer.length() -2, 1);
            }
            for(uint8_t j=0; j < i; j++){out_buffer.concat("  ");}
            out_buffer.concat("],\n");
          }
        }
      }

      if(tag == nullptr){
        break;
      }

      for(uint8_t i = 0; i < MAX_TAG_RECURSION; i++){
        if(path[i] != nullptr && path[i] != path_last[i]){
          if(path[i] != nullptr && path_last[i] != nullptr &&
              path[i]->sequence != path_last[i]->sequence)
          {
            // Changed list elements.
            if(out_buffer.endsWith(",\n")){
              out_buffer.remove(out_buffer.length() -2, 1);
            }
            for(uint8_t j=0; j < i; j++){out_buffer.concat("  ");}
            out_buffer.concat("},{\n");
          }

          for(uint8_t j=0; j < i; j++){out_buffer.concat("  ");}
          if(strcmp(path[i]->name, "root") != 0){
            out_buffer.concat("\"");
            out_buffer.concat(path[i]->name);
            out_buffer.concat("\":");
          }

          if(path[i]->contentCount() > 0 && path[i]->direct_value == false){
            for(uint8_t j=0; j < i; j++){out_buffer.concat("  ");}
            out_buffer.concat(" [ ");
          }

          if(path[i]->children_len > 0 && path[i]->direct_value == false){
            //for(uint8_t j=0; j < i; j++){out_buffer.concat("  ");}
            out_buffer.concat(" {\n");
          } else {
            String content;
            int value;
            path[i]->contentsAt(path[i]->sequence, content, value);
            out_buffer.concat(" \"");
            out_buffer.concat(content);
            out_buffer.concat("\",\n");
          }
        }
      }

      for(uint8_t i = 0; i < MAX_TAG_RECURSION; i++){
        path_last[i] = path[i];
      }
    }
  }
  if(out_buffer.endsWith(",\n")){
    out_buffer.remove(out_buffer.length() -2, 1);
  }

	// open the file in write mode
	result = SPIFFS.begin();
	file = SPIFFS.open(filename, "w");
	if (!file) {
		Serial.println("file creation failed");
    SPIFFS.end();
		return false;
	}

  file.println(out_buffer);
  //file.println(out_buffer.length());

  file.close();
  Serial.println("Done saving config file.");

  SPIFFS.end();
  return true;
}

void Config::insertDevice(Connected_device device){
  if(device.address_segment[0].segment[0] == '\0'){
    // Not a populated device.
    return;
  }
  for(int i = 0; i < MAX_DEVICES; i++){
    if(devices[i].address_segment[0].segment[0] == '\0'){
      memcpy(&(devices[i]), &device, sizeof(device));
      return;
    }
  }
}

