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


void parse_topic(const char* subscribeprefix,
                 const char* topic,
                 Address_Segment* address_segments){
  // We only care about the part of the topic without the prefix
  // so check how many segments there are in subscribeprefix
  // so we can ignore that many segments later.
  int i, segment = 0;
  if(strlen(subscribeprefix)){
    for (i=0, segment=-1; subscribeprefix[i]; i++){
      segment -= (subscribeprefix[i] == '/');
    }
  }

  // Casting non-const here as we don't actually modify topic.
  char* p_segment_start = (char*)topic;
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
  Serial.println(payload);
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()){
    Serial.println("parseObject() failed");
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
                  String* return_topics, String* return_payloads)
{
  int return_pointer = 0;

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

  if(strncmp(address_segments[0].segment, "hosts", NAME_LEN) == 0 ||
      strncmp(address_segments[0].segment, "_all", NAME_LEN) == 0)
  {
    if(command == "solicit"){
      //Serial.println("Announce host.");
      toAnnounceHost(config, return_topics[return_pointer], return_payloads[return_pointer]);
      return_pointer++;
    }
  }

  for (int i = 0; i < MAX_DEVICES; ++i) {
		if(compare_addresses(address_segments, config->devices[i].address_segment)){
      //Serial.print("Matches: ");
			//Serial.println(i);

			if(command != "solicit"){
				io->changeState(config->devices[i], command);
      }
      io->toAnnounce(config->devices[i],
                     return_topics[return_pointer],
                     return_payloads[return_pointer]);
      return_pointer++;
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

  //payload += ", \"_subject\":\"hosts/";
  //payload += config->hostname;
  //payload += "\"";

  payload += "}";

  topic = config->publishprefix;
  topic += "/hosts/_announce";
}
