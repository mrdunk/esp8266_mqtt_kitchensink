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

#include "websocket.h"
#include "message_parsing.h"


extern WebSocket webSocket;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  webSocket.onEvent(num, type, payload, length);
}


WebSocket::WebSocket(Io* io_, Config* config_) :
  status(false),
  websocket(WebSocketsServer(81)),
  io(io_),
  config(config_)
{ }

void WebSocket::parseIncoming(uint8_t num, uint8_t * payload, size_t length) {
  String topic = "";

  String sequence = value_from_payload(payload, length, "_ping");
  if(sequence != ""){
    websocket.sendTXT(num, ". : {\"_ack\":\"" + sequence + "\"}");
  } else {
    std::function< void(String&, String&) > sendTXT_callback = 
      [&, num](String& t, String& p) {
                                if(status){
                                  websocket.broadcastTXT(t + " : " + p);
                                  //websocket.sendTXT(num, t + " : " + p);
                                }
                              };
    actOnMessage(io, config, topic, (char*)payload, sendTXT_callback);
  }
}

void WebSocket::publish(String& topic, String& payload){
  Serial.print("wsPublish(");
  Serial.print(topic);
  Serial.print(", ");
  Serial.print(payload);
  Serial.println(")");

  websocket.broadcastTXT(topic + " : " + payload);
}

void WebSocket::onEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[%u] Disconnected!\n", num);
      status = false;
			break;
		case WStype_CONNECTED:
			{
        status = true;
				IPAddress ip = websocket.remoteIP(num);
				Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n",
						num, ip[0], ip[1], ip[2], ip[3], payload);

				// send message to client
				websocket.sendTXT(num, "Connected");
			}
			break;
		case WStype_TEXT:
			//Serial.printf("[%u] get Text: %s\n\r", num, payload);

      parseIncoming(num, payload, length);

			// send data to all connected clients
			// websocket.broadcastTXT("message here");
			break;
		default:
			Serial.printf("[%u] unexpected type: %u\n", num, length);
	}
}

