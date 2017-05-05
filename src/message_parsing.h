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

#ifndef ESP8266__MESSAGE_PARSING_H
#define ESP8266__MESSAGE_PARSING_H

/* This library does some standard parsing on incoming messages. */

#include <ESP8266WiFi.h>
#include "config.h"
#include "host_attributes.h"
#include "devices.h"
#include "mdns_actions.h"
#include "tags.h"

// Convert full topic into tokens, separated by "/".
//void parse_topic(const char* subscribeprefix,
//                 const char* topic,
//                 Address_Segment* address_segments);

//void parse_tag_name(const char* tag_name, String* name_list);

bool compare_addresses(const Address_Segment* address_1, const Address_Segment* address_2);

// Presuming payload is JSON formatted, return value for corresponding key.
String valueFromStringPayload(const String& payload, const String& key);
String value_from_payload(const byte* payload, const unsigned int length, const String key);

void actOnMessage(Io* io, Config* config, String& topic, const String& payload,
                  String* return_topics, String* return_payloads);

void toAnnounceHost(Config* config, String& topic, String& payload);

#endif  // ESP8266__MESSAGE_PARSING_H
