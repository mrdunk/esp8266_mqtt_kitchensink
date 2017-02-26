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

#ifndef ESP8266__HTTP_SERVER__H
#define ESP8266__HTTP_SERVER__H

#include "html_primatives.h"
#include "host_attributes.h"
#include "mdns_actions.h"

enum tagType{
  tagUnset,
  tagPlain,
  tagList,
  tagInverted,
  tagEnd,
  tagListItem
};


class HttpServer{
 public:
  HttpServer(char* _buffer, 
             const int _buffer_size,
             Config* _config,
             MdnsLookup* _brokers,
             mdns::MDns* _mdns,
             Mqtt* _mqtt,
             Io* _io,
             int* _allow_config);
  void loop();
 private:
  ESP8266WebServer esp8266_http_server;
  void onTest();
  void handleNotFound();
  void onFileOperations(const String& _filename = "");
  void fileBrowser();
  void onRoot();
  const String mime(const String& filename);
  void onConfig();
  void onSet();
  void onReset();
  bool readFile(const String& filename);
  char* buffer;
  const int buffer_size;
  Config* config;
  MdnsLookup* brokers;
  mdns::MDns* mdns;
  Mqtt* mqtt;
  Io* io;
  int* allow_config;
  void bufferClear();
  bool bufferAppend(const String& to_add);
  bool bufferAppend(const char* to_add);
  bool bufferInsert(const String& to_insert);
  bool bufferInsert(const char* to_insert);

  void mustacheCompile(char* buffer);

  /* Search through buffer for a {{tag}} for line_len characters.
   * Returns: Pointer in buffer to end of first matching {{tag}}. */
  char* parseTag(char* line, int line_len);

  /* Find a set of characters within buffer.
   * Returns: Pointer in buffer to start of first match. */
  char* findPattern(char* buffer, const char* pattern, int line_len) const;

  /* Replace a {{tag}} with the string in tag_content. */
  void replaceTag(char* tag_position, const char* tag, char* tag_content, tagType type);

  /* Populate tag variable with the contents of a tag in bugger pointed to by
   * tag_position.
   * Returns: true/false depending on whether a {{tag}} was successfully parsed.*/
  bool tagName(char* tag_start, char* tag, tagType& type);

  void duplicateList(char* tag_start, const char* tag, const int number);
  void removeList(char* tag_start_buf, const char* tag);
 
#define MAX_LIST_RECURSION 4 
  int list_element[MAX_LIST_RECURSION];  
  int list_size[MAX_LIST_RECURSION];
  int list_cache_time[MAX_LIST_RECURSION];
  int list_depth;
  char list_parent[128];
};



#endif  // ESP8266__HTTP_SERVER__H
