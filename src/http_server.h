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

#define TAG_NAME_LEN 64
#define MAX_LIST_RECURSION 4 

enum tagType{
  tagUnset,
  tagPlain,
  tagList,
  tagInverted,
  tagEnd,
  tagListItem
};

struct List{
  char* buffer;
  int buffer_len;
  char tag[TAG_NAME_LEN];
  char parent[(TAG_NAME_LEN * MAX_LIST_RECURSION) + MAX_LIST_RECURSION];
  bool inverted;
};


class MyESP8266WebServer : public ESP8266WebServer{
 public:
  MyESP8266WebServer() : ESP8266WebServer(){ }
  MyESP8266WebServer(int port) : ESP8266WebServer(port){ }
  bool available(){
    return bool(_server.available());
  }
  uint8_t status(){
    return _server.status();
  }
};

/* Uses Mustache style templates for generating web pages.
 * http://mustache.github.io/mustache.5.html
 * */
class CompileMustache{
 public:
  CompileMustache(char* buffer,
                  const int _buffer_size,
                  Config* _config,
                  MdnsLookup* _brokers,
                  mdns::MDns* _mdns,
                  Mqtt* _mqtt,
                  Io* _io);

  void parseBuffer(char* buffer_in, int buffer_in_len,
                   char*& buffer_out, int& buffer_out_len,
                   int& list_depth, bool& parsing_list);

  /* Find a set of characters within buffer.
   * Returns: Pointer in buffer to start of first match. */
  char* findPattern(char* buff, const char* pattern, int line_len) const;
  char* findClosingTag(char* buffer_in, const int buffer_in_len, const char* tag);

  /* Replace a {{tag}} with the string in tag_content. */
  bool replaceTag(char* tag_position, const char* tag, char* tag_content, tagType type);
  bool replaceTag2(char* destination,
                   int& itterator,
                   int& element_count,
                   const char* tag,
                   const tagType type,
                   const int len,
                   const int list_depth);
  void enterList(char* parents, char* tag);
  void exitList(char* parents);

  /* Populate tag variable with the contents of a tag in buffer pointed to by
   * tag_position.
   * Returns: true/false depending on whether a {{tag}} was successfully parsed.*/
  bool tagName(char* tag_start, char* tag, tagType& type);

  bool duplicateList(char* tag_start, const char* tag, const int number);
  bool removeList(char* tag_start_buf, const char* tag);

 private:
  bool success;
  int buffer_size;
  int head;
  int tail;
  char* buffer;
  Config* config;
  MdnsLookup* brokers;
  mdns::MDns* mdns;
  Mqtt* mqtt;
  Io* io;
  int list_element[MAX_LIST_RECURSION];  
  int list_size[MAX_LIST_RECURSION];
  int list_cache_time[MAX_LIST_RECURSION];
  int list_depth;  // TODO remove
  char list_parent[128];
  
  bool myMemmove(char* destination, char* source, int len);

  List list_template[MAX_LIST_RECURSION];
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
  MyESP8266WebServer esp8266_http_server;
  void onTest();
  void handleNotFound();
  void onFileOperations(const String& _filename = "");
  void fileBrowser();
  void onRoot();
  const String mime(const String& filename);
  void onConfig();
  void onSet();
  void onReset();
  bool fileOpen(const String& filename);
  bool fileRead(char* buffer_in, const int buffer_in_len);
  void fileClose();
  File file;
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
};



#endif  // ESP8266__HTTP_SERVER__H
