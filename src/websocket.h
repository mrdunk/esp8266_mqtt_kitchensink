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

#ifndef ESP8266__WEBSOCKET_H
#define ESP8266__WEBSOCKET_H

#include <WebSocketsServer.h>   // WebSockets library.
#include "devices.h"
#include "host_attributes.h"
#include "config.h"

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

class WebSocket{
 public:
  WebSocket(Io* io_, Config* config_);
  void parseIncoming(uint8_t num, uint8_t * payload, size_t length);
  void publish(String& topic, String& payload);
  void onEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
  void begin(){
    websocket.begin(); 
    websocket.onEvent(webSocketEvent);
  }
  void loop(){ websocket.loop(); }
  bool status;
 private:
  WebSocketsServer websocket;
  Io* io;
  Config* config;
};


#endif  // ESP8266__WEBSOCKET_H
