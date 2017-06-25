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

#include "host_attributes.h"
#include "mdns_actions.h"
#include "mqtt.h"


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

class HttpServer{
 public:
  HttpServer(char* _buffer, 
             const int _buffer_size,
             Config* _config,
             MdnsLookup* _brokers,
             mdns::MDns* _mdns,
             Mqtt* _mqtt,
             Io* _io);
  void loop();
 private:
  MyESP8266WebServer esp8266_http_server;
  void onTest();
  void onRoot();
  void onReset();
  void onFileOperations(const String& _filename = "");
  void onErase();
  void onPullFileMenu();
  void onLogin();
  void onNotFound();
  void fileBrowser(bool names_only=false);
  
  void fetchCookie();
  bool isAuthentified(bool redirect=true);
  
  const String mime(const String& filename);
  bool fileOpen(const String& filename);
  //bool fileRead(char* buffer_in, const int buffer_in_len);
  void fileClose();

  File file;
  char* buffer;
  const int buffer_size;
  Config* config;
  MdnsLookup* brokers;
  mdns::MDns* mdns;
  Mqtt* mqtt;
  Io* io;

  void bufferClear();
  bool bufferAppend(const String& to_add);
  bool bufferAppend(const char* to_add);
};


#endif  // ESP8266__HTTP_SERVER__H
