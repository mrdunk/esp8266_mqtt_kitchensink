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

#ifndef ESP8266__CONFIG_H
#define ESP8266__CONFIG_H


// Reset if unable to connect to WiFi after this many seconds.
#define RESET_ON_CONNECT_FAIL 20

// Maximum number of devices connected to IO pins.
#define MAX_DEVICES 4

// Maximum number of subscriptions to MQTT.
#define MAX_SUBSCRIPTIONS 10

// Length of name strings. (hostname, room names, lamp names, etc.)
#define HOSTNAME_LEN 32
#define NAME_LEN 16

// Each device has an address in the form 'role/location1/location2/etc'
// eg: 'io/kitchen/worktop/left'.
// These map to the second half of the MQTT topic.
#define ADDRESS_SEGMENTS 6

// MQTT topics will start with a prefix.
// eg: in the topic 'homeautomation/0/io/kitchen/worktop/left',
//     'homeautomation/0' maps to the prefix.
#define PREFIX_LEN 32

// How many times to retry to upload firmware on failure.
#define UPLOAD_FIRMWARE_RETRIES 3

// Maximum length of any other string the user might configure.
#define STRING_LEN 64

// IO Pin that will enable configuration web page.
#define CONFIGURE_PIN 0

// Port for web interface.
#define HTTP_PORT 80

// Buffer size. Buffer is used by both mDNS and HTTP server.
#define BUFFER_SIZE 800

// Maximum length of a MQTT topic.
#define MAX_TOPIC_LENGTH  (PREFIX_LEN + ((NAME_LEN +1) * ADDRESS_SEGMENTS) +1)

// mDNS service_type for MQTT Broker.
#define QUESTION_SERVICE "_mqtt._tcp.local"

// Number of seconds before login will expire.
#define SESSION_TIMEOUT (60 * 60)


#endif  // ESP8266__CONFIG_H
