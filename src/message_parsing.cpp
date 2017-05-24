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

#include <ArduinoJson.h>        // ArduinoJson library.
#include "message_parsing.h"
#include "ipv4_helpers.h"

extern TagRoot root_tag;
extern TagItterator tag_itterator;
extern std::function< void(String&, String&) > tag_itterator_callback;

void parse_tag_name(String& tag_name, String* name_list){
  uint8_t section_count = 0;
  uint8_t section_start = 0;
  while(section_start < tag_name.length() && section_count < MAX_TAG_RECURSION){
    int section_end = tag_name.indexOf(".", section_start);
    if(section_end < 0){
      section_end = tag_name.length();
    }
    name_list[section_count++] = tag_name.substring(section_start, section_end);
    section_start = section_end +1;
  }
  for(uint8_t i = section_count; i < MAX_TAG_RECURSION; i++){
    name_list[i] = "";
  }
}

void parse_topic(char* subscribeprefix,
                 char* topic,
                 Address_Segment* address_segments){
  // We only care about the part of the topic without the prefix
  // so mark any segments matching the prefix to be ignored.
  int segment = 0;
  char* prefix_start = subscribeprefix;
  char* topic_start = topic;
  //char prefix_tmp[128];
  //char topic_tmp[128];
  while(prefix_start < subscribeprefix + strlen(subscribeprefix) &&
      topic_start < topic + strlen(topic)){
    char* prefix_end = strchr(prefix_start, '/');
    char* topic_end = strchr(topic_start, '/');
    if(!prefix_end){
      prefix_end = subscribeprefix + strlen(subscribeprefix);
    }
    if(!topic_end){
      topic_end = topic + strlen(topic);
    }

    //memset(prefix_tmp, '\0', 128);
    //memset(topic_tmp, '\0', 128);
    //strncpy(prefix_tmp, prefix_start, prefix_end - prefix_start);
    //strncpy(topic_tmp, topic_start, topic_end - topic_start);

    //Serial.print(prefix_tmp);
    //Serial.print("\t<>\t");
    //Serial.print(topic_tmp);
    //Serial.print("\t");

    if(strncmp(prefix_start, "+", prefix_end - prefix_start) == 0){
      //Serial.println("Match");
      segment--;
    } else if(strncmp(prefix_start, topic_start, prefix_end - prefix_start) == 0){
      //Serial.println("Match");
      segment--;
    } else {
      // No match.
      //Serial.println("nope");
      segment = 0;
      break;
    }

    prefix_start = prefix_end +1;
    topic_start = topic_end +1;
  }

  char* p_segment_start = topic;
  char* p_segment_end = strchr(topic, '/');
  while(p_segment_end != NULL){
    if(segment >= 0){
      int segment_len = p_segment_end - p_segment_start;
      if(segment_len > NAME_LEN){
        segment_len = NAME_LEN;
      }
      strncpy(address_segments[segment].segment, p_segment_start, segment_len);
      address_segments[segment].segment[segment_len] = '\0';
    }
    p_segment_start = p_segment_end +1;
    p_segment_end = strchr(p_segment_start, '/');
    segment++;
  }
  strncpy(address_segments[segment++].segment, p_segment_start, NAME_LEN);
  
  for(; segment < ADDRESS_SEGMENTS; segment++){
    address_segments[segment].segment[0] = '\0';
  }
}

bool compare_addresses(const Address_Segment* address_1, const Address_Segment* address_2){
  /*for(int s=0; s < ADDRESS_SEGMENTS; s++){
    if(strlen(address_1[s].segment) == 0 && strlen(address_2[s].segment) == 0){
      break;
    }
    Serial.print(address_1[s].segment);
    Serial.print("\t\t");
    Serial.println(address_2[s].segment);
  }*/

  if(strlen(address_2[0].segment) <= 0){
    return false;
  }
  if(strcmp(address_1[0].segment, "_all") != 0 &&
      strcmp(address_1[0].segment, address_2[0].segment) != 0){
    return false;
  }
  for(int s=1; s < ADDRESS_SEGMENTS; s++){
    if(strcmp(address_1[s].segment, "_all") == 0){
      return true;
    }
    if(strcmp(address_1[s].segment, address_2[s].segment) != 0){
      return false;
    }
  }
  return true;
}
  
String valueFromStringPayload(const String& payload, const String& key) {
  StaticJsonBuffer<300> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()){
    Serial.print("parseObject() failed. ");
    Serial.print(payload);
    Serial.print("  ");
    Serial.println(key);
    return "";
  }

  if(!root.containsKey(key)){
    //Serial.println("containsKey(" + key + ") failed");
    return "";
  }

  return root[key];
}

String value_from_payload(const byte* _payload, const unsigned int length, const String key) {
  String payload("");

  // Loop until length as we don't have null terminator.
  for(unsigned int i = 0; i < length; i++){
    payload += (char)_payload[i];
  }

  return valueFromStringPayload(payload, key);
}

void actOnMessage(Io* io, Config* config, String& topic, const String& payload,
                  std::function< void(String&, String&) > callback)
{
  Serial.print(topic);
  Serial.print(" : ");
  Serial.println(payload);

  if(topic == ""){
    topic = valueFromStringPayload(payload, "_subject");
  }
  if(topic == ""){
    Serial.println("WARNING: Missing topic.");
    return;
  }
  String command = valueFromStringPayload(payload, "_command");
  if(command == ""){
    Serial.println("WARNING: Missing payload.");
    return;
  }

  Address_Segment address_segments[ADDRESS_SEGMENTS];
  char topic_char[topic.length() +1];
  topic.toCharArray(topic_char, topic.length() +1);
  parse_topic(config->subscribeprefix, topic_char, address_segments);

  Address_Segment host_all[ADDRESS_SEGMENTS] = {"hosts","_all"};
  if(compare_addresses(address_segments, host_all)){
    if(command == "solicit"){
      //Serial.println("Announce host.");
      String host_topic;
      String host_payload;
      toAnnounceHost(config, host_topic, host_payload);
      callback(host_topic, host_payload);
    } else if(command == "learn_all"){
      Serial.println("----------------learn_all-----------");
      //root_tag.sendDataRecursive(callback);

      tag_itterator.reset();
      tag_itterator_callback = callback;
      //TagBase* tag;
      //while((tag = tag_itterator.loop())){
      //  tag->sendData(callback);
      //}

    } else if(command == "learn"){
    }
  }

  for (int i = 0; i < MAX_DEVICES; ++i) {
		if(compare_addresses(address_segments, config->devices[i].address_segment)){
      //Serial.print("Matches: ");
			//Serial.println(i);

			if(command != "solicit"){
				io->changeState(config->devices[i], command);
      }

      String host_topic;
      String host_payload;
      io->toAnnounce(config->devices[i], host_topic, host_payload);
      callback(host_topic, host_payload);
		}
  }
}

void toAnnounceHost(Config* config, String& topic, String& payload){
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String parsed_mac = macToStr(mac);;
  parsed_mac.replace(":", "_");
  payload = "{\"_macaddr\":\"";
  payload += parsed_mac;

  payload += "\", \"_hostname\":\"";
  payload += config->hostname;
  payload += "\", \"_ip\":\"";
  payload += ip_to_string(WiFi.localIP());
  payload += "\"";

  payload += ", \"_subject\":\"hosts/";
  payload += config->hostname;
  payload += "\"";

  payload += "}";

  topic = config->publishprefix;
  topic += "/hosts/_announce";
}
