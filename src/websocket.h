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
#include "message_parsing.h"

extern WebSocketsServer webSocket;
extern Io io;
extern Config config;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  String return_topics[MAX_DEVICES +1];
  String return_payloads[MAX_DEVICES +1];
  String topic = "";
  String payload_string((char*)payload);

	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[%u] Disconnected!\n", num);
			break;
		case WStype_CONNECTED:
			{
				IPAddress ip = webSocket.remoteIP(num);
				Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n",
						num, ip[0], ip[1], ip[2], ip[3], payload);

				// send message to client
				webSocket.sendTXT(num, "Connected");
			}
			break;
		case WStype_TEXT:
			Serial.printf("[%u] get Text: %s\n", num, payload);

      actOnMessage(&io, &config, topic, payload_string, return_topics, return_payloads);
      for(int i = 0; i < MAX_DEVICES +1; i++){
        if(return_topics[i] != "" || return_payloads[i] != ""){
          // send message to client
          webSocket.sendTXT(num, return_topics[i]);
          webSocket.sendTXT(num, return_payloads[i]);
        }
      }

			// send data to all connected clients
			// webSocket.broadcastTXT("message here");
			break;
		default:
			Serial.printf("[%u] unexpected type: %u\n", num, length);
	}
}


#endif  // ESP8266__WEBSOCKET_H
