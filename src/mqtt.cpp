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

#include "host_attributes.h"
#include "devices.h"
#include "mqtt.h"
#include "message_parsing.h"

// TODO. These externs are lazy.
// Io depends on Mqtt. Mqtt depends on Io.
// Ideally we would decouple these.
extern Io io;
extern Config config;

// TODO. Make use of config.brokerip .


void Mqtt::loop(){
  if (!connected()) {
    if (was_connected){
      Serial.println("MQTT disconnected.");
      was_connected = false;
    }
    connect();
    if(count_loop++ % 10 == 0){
      Serial.print("-");
    }
    delay(10);
  } else {
    if (!was_connected){
      // Serial.println("MQTT connected.");
      was_connected = true;
    }
    subscribeOne();
    mqtt_client.loop();
  }
}


// Called whenever a MQTT topic we are subscribed to arrives.
void Mqtt::callback(const char* _topic, const byte* _payload, const unsigned int length) {
  String topic(_topic);
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String payload;
  for (unsigned int i = 0; i < length; i++) {
    payload += (char)_payload[i];
  }
  Serial.println(payload);

  auto publish_callback = [&](String& t, String& p) {publish(t, p);};  
  actOnMessage(&io, &config, topic, payload, publish_callback);
}

void Mqtt::publish(const String topic, const String payload){
  if(!connected() || topic == "" || payload == ""){
    return;
  }
  Serial.print("publish: ");
  Serial.print(topic);
  Serial.print("  :  ");
  Serial.println(payload);
  mqtt_client.publish(topic.c_str(), payload.c_str());
}

void Mqtt::queue_mqtt_subscription(const char* path){
  for(int i = 0; i < mqtt_subscription_count; i++){
    if(strncmp(&mqtt_subscriptions[i * MAX_TOPIC_LENGTH], path, MAX_TOPIC_LENGTH) == 0){
      return;
    }
  }
  if(mqtt_subscription_count < MAX_SUBSCRIPTIONS){
    Serial.print(mqtt_subscription_count);
    Serial.print(" ");
    Serial.println(path);
    strncpy(&mqtt_subscriptions[mqtt_subscription_count * MAX_TOPIC_LENGTH], path, MAX_TOPIC_LENGTH -1);
    mqtt_subscription_count++;
  } else {
    Serial.println("Error. Too many subscriptions.");
  }
  return;
}

void Mqtt::subscribeOne(){
  if(mqtt_subscription_count > mqtt_subscribed_count){
    Serial.print("* ");
    Serial.println(&mqtt_subscriptions[mqtt_subscribed_count * MAX_TOPIC_LENGTH]);
    mqtt_client.subscribe(&mqtt_subscriptions[mqtt_subscribed_count * MAX_TOPIC_LENGTH]);
    mqtt_subscribed_count++;
  }
}

void Mqtt::mqtt_clear_buffers(){
  for(int i = 0; i < MAX_SUBSCRIPTIONS; i++){
    mqtt_subscriptions[i * MAX_TOPIC_LENGTH] = '\0';
  }
  mqtt_subscription_count = 0;
  mqtt_subscribed_count = 0;
}

// Called whenever we want to make sure we are subscribed to necessary topics.
void Mqtt::connect() {
  Host broker = brokers->GetHost();
  IPAddress ip = broker.address;
  int port = broker.port;
  if(ip == IPAddress(0,0,0,0) || port == 0){
    // No valid broker.
    return;
  }
  mqtt_client.setServer(ip, port);
  mqtt_client.setCallback(registered_callback);

  if (mqtt_client.connect(config.hostname)) {
    if(mqtt_subscribed_count > 0){
      // In the event this is a re-connection, clear the subscription buffer.
      mqtt_clear_buffers();
    }

    char address[MAX_TOPIC_LENGTH];
   
    strncpy(address, config.subscribeprefix, MAX_TOPIC_LENGTH -1);
    strncat(address, "/_all/_all", MAX_TOPIC_LENGTH -1 -strlen(address));
    queue_mqtt_subscription(address);
    
    strncpy(address, config.subscribeprefix, MAX_TOPIC_LENGTH -1);
    strncat(address, "/hosts/_all", MAX_TOPIC_LENGTH -1 - strlen(address));
    queue_mqtt_subscription(address);

    strncpy(address, config.subscribeprefix, MAX_TOPIC_LENGTH -1);
    strncat(address, "/hosts/", MAX_TOPIC_LENGTH -1 - strlen(address));
    strncat(address, config.hostname, MAX_TOPIC_LENGTH -1 - strlen(address));
    queue_mqtt_subscription(address);

    for (int i = 0; i < MAX_DEVICES; ++i) {
      if (strlen(config.devices[i].address_segment[0].segment) > 0) {
        String fetch_topic;
        String fetch_payload;
				io.toAnnounce(config.devices[i], fetch_topic, fetch_payload);
        publish(fetch_topic, fetch_payload);

        strncpy(address, config.subscribeprefix, MAX_TOPIC_LENGTH -1);
        strncat(address, "/_all", MAX_TOPIC_LENGTH -1 - strlen(address));

        for(int j = 0; j < ADDRESS_SEGMENTS; j++){
          if(strlen(config.devices[i].address_segment[j].segment) <= 0) {
            break;
          }
          address[strlen(address) -4] = '\0';
          strncat(address, config.devices[i].address_segment[j].segment,
                  MAX_TOPIC_LENGTH -1 - strlen(address));
          if (j < (ADDRESS_SEGMENTS -1) &&
              strlen(config.devices[i].address_segment[j +1].segment) > 0) {
            strncat(address, "/_all", MAX_TOPIC_LENGTH -1 - strlen(address));
          }
          queue_mqtt_subscription(address);
        }
      }
    }
    String host_topic;
    String host_payload;
    toAnnounceHost(&config, host_topic, host_payload);
    publish(host_topic, host_payload);
  }
  brokers->RateHost(mqtt_client.connected());
  
  if (mqtt_client.connected()) {
    Serial.print("MQTT connected to: ");
    Serial.println(broker.address);
  }
}

