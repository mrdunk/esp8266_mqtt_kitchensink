/* Copyright <YEAR> <COPYRIGHT HOLDER>
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


#include <ESP8266WebServer.h>

#include "http_server.h"
#include "html_primatives.h"
#include "ipv4_helpers.h"
#include "persist_data.h"
#include "persist_data.cpp"   // Template arguments confuse the linker so need to include .cpp .
#include "config.h"

HttpServer::HttpServer(char* _buffer,
                       const int _buffer_size,
                       Config* _config,
                       MdnsLookup* _brokers,
                       mdns::MDns* _mdns,
                       Mqtt* _mqtt,
                       Io* _io,
                       int* _allow_config) : 
    buffer(_buffer),
    buffer_size(_buffer_size),
    config(_config),
    brokers(_brokers),
    mdns(_mdns),
    mqtt(_mqtt),
    io(_io),
    allow_config(_allow_config)
{
  esp8266_http_server = ESP8266WebServer(HTTP_PORT);
  esp8266_http_server.on("/test", [&]() {onTest();});
  esp8266_http_server.on("/", [&]() {onRoot();});
  esp8266_http_server.on("/script.js", [&]() {onScript();});
  esp8266_http_server.on("/style.css", [&]() {onStyle();});
  esp8266_http_server.on("/configure", [&]() {onConfig();});
  esp8266_http_server.on("/configure/", [&]() {onConfig();});
  esp8266_http_server.on("/set", [&]() {onSet();});
  esp8266_http_server.on("/set/", [&]() {onSet();});
  esp8266_http_server.on("/pullfirmware", [&]() {onPullFirmware();});
  esp8266_http_server.on("/pullfirmware/", [&]() {onPullFirmware();});
  esp8266_http_server.on("/reset", [&]() {onReset();});
  esp8266_http_server.on("/reset/", [&]() {onReset();});

  esp8266_http_server.begin();

  bufferClear();
}

void HttpServer::loop(){
  esp8266_http_server.handleClient();
}

void HttpServer::onTest(){
  bufferAppend("testing");
  esp8266_http_server.send(200, "text/plain", buffer);
}

void HttpServer::onRoot(){
  Serial.println("onRoot() +");
  bool sucess = true;
  bufferClear();
  uint8_t mac[6];
  WiFi.macAddress(mac);
  sucess &= bufferAppend(descriptionListItem("MAC address", macToStr(mac)));
  sucess &= bufferAppend(descriptionListItem("Hostname", config->hostname));
  sucess &= bufferAppend(descriptionListItem("IP address", String(ip_to_string(WiFi.localIP()))));
  sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));

  sucess &= bufferAppend(descriptionListItem("WiFI RSSI", String(WiFi.RSSI())));
  
  byte numSsid = WiFi.scanNetworks();
  for (int thisNet = 0; thisNet<numSsid; thisNet++) {
    sucess &= bufferAppend(descriptionListItem("WiFi SSID", WiFi.SSID(thisNet) +
        "&nbsp&nbsp&nbsp(" + String(WiFi.RSSI(thisNet)) + ")"));
  }
  sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));

  sucess &= bufferAppend(descriptionListItem("CPU frequency", String(ESP.getCpuFreqMHz())));
  sucess &= bufferAppend(descriptionListItem("Flash size", String(ESP.getFlashChipSize())));
  sucess &= bufferAppend(descriptionListItem("Flash space",
      String(int(100 * ESP.getFreeSketchSpace() / ESP.getFlashChipSize())) + "%"));
  sucess &= bufferAppend(descriptionListItem("Flash speed", String(ESP.getFlashChipSpeed())));
  sucess &= bufferAppend(descriptionListItem("Free memory", String(ESP.getFreeHeap())));
  sucess &= bufferAppend(descriptionListItem("SDK version", ESP.getSdkVersion()));
  sucess &= bufferAppend(descriptionListItem("Core version", ESP.getCoreVersion()));
  sucess &= bufferAppend(descriptionListItem("Config version", config->config_version));
  sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));
  sucess &= bufferAppend(descriptionListItem("Analogue in", String(analogRead(A0))));
  sucess &= bufferAppend(descriptionListItem("System clock", String(millis() / 1000)));
  sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));
  
  String table = tableStart() + rowStart("") +
                 header("") + header("service_name") + header("port") +
                 header("hostname") + header("ip") + header("service valid until") +
                 header("host valid until") + header("ipv4 valid until") +
                 header("fail counter") +
                 rowEnd();
  Host* phost;
  bool active;
  while(brokers->IterateHosts(&phost, &active)){
    if(active){
      table += rowStart("highlight");
      table += cell("active");
    } else {
      table += rowStart("");
      table += cell("");
    }
    table += cell(phost->service_name);
    table += cell(String(phost->port));
    table += cell(phost->host_name);
    table += cell(ip_to_string(phost->address));
    table += cell(String(phost->service_valid_until));
    table += cell(String(phost->host_valid_until));
    table += cell(String(phost->ipv4_valid_until));
    table += cell(String(phost->fail_counter));

    table += rowEnd();
  }
  table += tableEnd();
  sucess &= bufferAppend(descriptionListItem("Brokers", table));
  
#ifdef DEBUG_STATISTICS
  if(mdns->packet_count != 0){
    sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));
    sucess &= bufferAppend(descriptionListItem("mDNS decode success rate",
        String(mdns->packet_count - mdns->buffer_size_fail) + " / " + 
        String(mdns->packet_count) + "&nbsp&nbsp&nbsp" +
        String(100 - (100 * mdns->buffer_size_fail / mdns->packet_count)) + "%"));
    sucess &= bufferAppend(descriptionListItem("Largest mDNS packet size",
        String(mdns->largest_packet_seen) + " / " + 
        String(BUFFER_SIZE) + " bytes"));
  }
#endif

  sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));
  sucess &= bufferAppend(descriptionListItem("Configure", link("go", "configure")));

  sucess &= bufferInsert(listStart());
  sucess &= bufferAppend(listEnd());
  
  sucess &= bufferInsert(pageHeader("style.css", ""));
  sucess &= bufferAppend(pageFooter());

  Serial.println(sucess);
  Serial.println("onRoot() -");
  esp8266_http_server.send((sucess ? 200 : 500), "text/html", buffer);
}

void HttpServer::onScript(){
  Serial.println("onScript() +");
  // strncpy_P() to copy from program memory as javasctipt is PROGMEM.
  strncpy_P(buffer, javascript, BUFFER_SIZE);
  buffer[BUFFER_SIZE -1] = '\0';
  Serial.println(strlen(buffer));
  Serial.println("onScript() -");
  esp8266_http_server.send(200, "text/javascript", buffer);
}

void HttpServer::onStyle(){
  Serial.println("onStyle() +");
  // strncpy_P() to copy from program memory as style is PROGMEM.
  strncpy_P(buffer, style, BUFFER_SIZE);
  buffer[BUFFER_SIZE -1] = '\0';
  Serial.println(strlen(buffer));
  Serial.println("onStyle() -");
  esp8266_http_server.send(200, "text/css", buffer);
}

void HttpServer::onConfig(){
  Serial.println("onConfig() +");
  bool sucess = true;
  bufferClear();

  if(*allow_config){
    (*allow_config)--;
  }
  if(*allow_config <= 0 && esp8266_http_server.hasArg("enablepassphrase") &&
      config->enable_passphrase != "" &&
      esp8266_http_server.arg("enablepassphrase") == config->enable_passphrase){
    *allow_config = 1;
  }
  Serial.print("allow_config: ");
  Serial.println(*allow_config);
  
  if(*allow_config){
    uint8_t mac[6];
    WiFi.macAddress(mac);
    sucess &= bufferAppend(descriptionListItem("mac_address", macToStr(mac)));
    
    if(config->ip == IPAddress(0, 0, 0, 0)) {
      sucess &= bufferAppend(descriptionListItem("IP address by DHCP",
                                     String(ip_to_string(WiFi.localIP()))));
    }
    sucess &= bufferAppend(descriptionListItem("hostname", 
        textField("hostname", "hostname", config->hostname, "hostname") +
        submit("Save", "save_hostname" , "save('hostname')")));
    sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));

    sucess &= bufferAppend(descriptionListItem("IP address",
        ipField("ip", ip_to_string(config->ip), ip_to_string(config->ip), "ip") +
        submit("Save", "save_ip" , "save('ip')") +
        String("(0.0.0.0 for DHCP. Static boots quicker.)")));
    if(config->ip != IPAddress(0, 0, 0, 0)) {
      sucess &= bufferAppend(descriptionListItem("Subnet mask",
          ipField("subnet", ip_to_string(config->subnet), ip_to_string(config->subnet), "subnet") +
          submit("Save", "save_subnet" , "save('subnet')")));
      sucess &= bufferAppend(descriptionListItem("Gateway",
          ipField("gateway", ip_to_string(config->gateway),
            ip_to_string(config->gateway), "gateway") +
          submit("Save", "save_gateway" , "save('gateway')")));
    }
    sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));

    sucess &= bufferAppend(descriptionListItem("MQTT broker hint",
        ipField("broker_ip", ip_to_string(config->broker_ip),
                ip_to_string(config->broker_ip), "brokerip") +
        submit("Save", "save_brokerip" , "save('brokerip')") +
        String("(0.0.0.0 to use mDNS auto discovery)")));
    sucess &= bufferAppend(descriptionListItem("MQTT subscription prefix",
        textField("subscribeprefix", "subscribeprefix", config->subscribe_prefix,
          "subscribeprefix") +
        submit("Save", "save_subscribeprefix" , "save('subscribeprefix')")));
    sucess &= bufferAppend(descriptionListItem("MQTT publish prefix",
        textField("publishprefix", "publishprefix", config->publish_prefix,
          "publishprefix") +
        submit("Save", "save_publishprefix" , "save('publishprefix')")));
    sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));
    
    sucess &= bufferAppend(descriptionListItem("HTTP Firmware URL",
        textField("firmware_server", "firmware_server", config->firmware_server,
          "firmwareserver") +
        submit("Save", "save_firmwareserver" , "save('firmwareserver')")));
    sucess &= bufferAppend(descriptionListItem("Config enable passphrase",
        textField("enable_passphrase", "enable_passphrase", config->enable_passphrase,
          "enablepassphrase") +
        submit("Save", "save_enablepassphrase" , "save('enablepassphrase')")));
    sucess &= bufferAppend(descriptionListItem("Config enable IO pin",
        ioPin(config->enable_io_pin, "enableiopin") +
        submit("Save", "save_enableiopin" , "save('enableiopin')")));


    sucess &= bufferAppend(tableStart());

    sucess &= bufferAppend(rowStart("") + header("index") + header("Topic") + header("type") + 
        header("IO pin") + header("Default val") + header("Inverted") +
        header("") + header("") + rowEnd());

    int empty_device = -1;
    for (int i = 0; i < MAX_DEVICES; ++i) {
      if (strlen(config->devices[i].address_segment[0].segment) > 0) {
        sucess &= bufferAppend(rowStart("device_" + String(i)));
        sucess &= bufferAppend(cell(String(i)));
        String name = "topic_";
        name.concat(i);
        sucess &= bufferAppend(cell(config->subscribe_prefix + String("/") +
            textField(name, "some/topic", DeviceAddress(config->devices[i]),
              "device_" + String(i) + "_topic")));
        if (config->devices[i].iotype == Io_Type::pwm) {
          sucess &= bufferAppend(cell(outletType("pwm", "device_" + String(i) + "_iotype")));
        } else if (config->devices[i].iotype == Io_Type::onoff) {
          sucess &= bufferAppend(cell(outletType("onoff", "device_" + String(i) + "_iotype")));
        } else if (config->devices[i].iotype == Io_Type::input) {
          sucess &= bufferAppend(cell(outletType("input", "device_" + String(i) + "_iotype")));
        } else {
          sucess &= bufferAppend(cell(outletType("test", "device_" + String(i) + "_iotype")));
        }
        sucess &= bufferAppend(cell(ioPin(config->devices[i].io_pin,
              "device_" + String(i) + "_io_pin")));
        sucess &= bufferAppend(cell(ioValue(config->devices[i].io_default,
              "device_" + String(i) + "_io_default")));
        sucess &= bufferAppend(cell(ioInverted(config->devices[i].inverted,
              "device_" + String(i) + "_inverted")));

        sucess &= bufferAppend(cell(submit("Save", "save_" + String(i),
                                 "save('device_" + String(i) +"')")));
        sucess &= bufferAppend(cell(submit("Delete", "del_" + String(i),
                                  "del('device_" + String(i) +"')")));
        sucess &= bufferAppend(rowEnd());
      } else if (empty_device < 0){
        empty_device = i;
      }
    }
    if (empty_device >= 0){
      // An empty slot for new device.
      sucess &= bufferAppend(rowStart("device_" + String(empty_device)));
      sucess &= bufferAppend(cell(String(empty_device)));
      String name = "address_";
      name.concat(empty_device);
      sucess &= bufferAppend(cell(config->subscribe_prefix + String("/") +
          textField(name, "new/topic", "", "device_" + String(empty_device) + "_topic")));
      sucess &= bufferAppend(cell(outletType("onoff", "device_" +
                                              String(empty_device) + "_iotype")));
      name = "pin_";
      name.concat(empty_device);
      sucess &= bufferAppend(cell(ioPin(0, "device_" + String(empty_device) + "_io_pin")));
      sucess &= bufferAppend(cell(ioValue(0, "device_" + String(empty_device) + "_io_default")));
      sucess &= bufferAppend(cell(ioInverted(false, "device_" +
                                             String(empty_device) + "_inverted")));
      sucess &= bufferAppend(cell(submit("Save", "save_" + String(empty_device),
            "save('device_" + String(empty_device) + "')")));
      sucess &= bufferAppend(cell(""));
      sucess &= bufferAppend(rowEnd());
    }
    
    sucess &= bufferAppend(tableEnd());

    sucess &= bufferAppend(descriptionListItem("Pull firmware", link("go", "pullfirmware")));
  
    sucess &= bufferInsert(listStart());
    sucess &= sucess &= bufferAppend(listEnd());
  } else {
    Serial.println("Not allowed to onConfig()");
    sucess &= bufferAppend("Configuration mode not enabled.<br>Press button connected to IO ");
    sucess &= bufferAppend(String(config->enable_io_pin));
    sucess &= bufferAppend(
        "<br>or append \"?enablepassphrase=PASSWORD\" to this URL<br>and reload.");
    sucess &= bufferInsert(pageHeader("", ""));
    sucess &= bufferAppend(pageFooter());
    esp8266_http_server.send(401, "text/html", buffer);
    return;
  }

  
  sucess &= bufferInsert(pageHeader("style.css", "script.js"));
  sucess &= bufferAppend(pageFooter());


  Serial.println(sucess);
  Serial.println(strlen(buffer));
  Serial.println("onConfig() -");
  esp8266_http_server.send((sucess ? 200 : 500), "text/html", buffer);
}

void HttpServer::onSet(){
  Serial.println("onSet() +");
  bool sucess = true;
  bufferClear();

  if(*allow_config <= 0){
    Serial.println("Not allowed to onSet()");
    esp8266_http_server.send(401, "text/html", "Not allowed to onSet()");
    return;
  }

  const unsigned int now = millis() / 1000;

  for(int i = 0; i < esp8266_http_server.args(); i++){
    sucess &= bufferInsert(esp8266_http_server.argName(i));
    sucess &= bufferInsert("\t");
    sucess &= bufferInsert(esp8266_http_server.arg(i));
    sucess &= bufferInsert("\n");
  }
  sucess &= bufferInsert("\n");

  if (esp8266_http_server.hasArg("test_arg")) {
    sucess &= bufferInsert("test_arg: " + esp8266_http_server.arg("test_arg") + "\n");
  } else if (esp8266_http_server.hasArg("ip")) {
    config->ip = string_to_ip(esp8266_http_server.arg("ip"));
    sucess &= bufferInsert("ip: " + esp8266_http_server.arg("ip") + "\n");
  } else if (esp8266_http_server.hasArg("gateway")) {
    config->gateway = string_to_ip(esp8266_http_server.arg("gateway"));
    sucess &= bufferInsert("gateway: " + esp8266_http_server.arg("gateway") + "\n");
  } else if (esp8266_http_server.hasArg("subnet")) {
    config->subnet = string_to_ip(esp8266_http_server.arg("subnet"));
    sucess &= bufferInsert("subnet: " + esp8266_http_server.arg("subnet") + "\n");
  } else if (esp8266_http_server.hasArg("brokerip")) {
    config->broker_ip = string_to_ip(esp8266_http_server.arg("brokerip"));
    sucess &= bufferInsert("broker_ip: " + esp8266_http_server.arg("brokerip") + "\n");
  } else if (esp8266_http_server.hasArg("hostname")) {
    char tmp_buffer[HOSTNAME_LEN];
    esp8266_http_server.arg("hostname").toCharArray(tmp_buffer, HOSTNAME_LEN);
    SetHostname(tmp_buffer);
    sucess &= bufferInsert("hostname: " + esp8266_http_server.arg("hostname") + "\n");
  } else if (esp8266_http_server.hasArg("publishprefix")) {
    char tmp_buffer[PREFIX_LEN];
    esp8266_http_server.arg("publishprefix").toCharArray(tmp_buffer, PREFIX_LEN);
    SetPrefix(tmp_buffer, config->publish_prefix);
    sucess &= bufferInsert("publishprefix: " + esp8266_http_server.arg("publishprefix") + "\n");
  } else if (esp8266_http_server.hasArg("subscribeprefix")) {
    char tmp_buffer[PREFIX_LEN];
    esp8266_http_server.arg("subscribeprefix").toCharArray(tmp_buffer, PREFIX_LEN);
    SetPrefix(tmp_buffer, config->subscribe_prefix);
    sucess &= bufferInsert("subscribeprefix: " + esp8266_http_server.arg("subscribeprefix") + "\n");
  } else if (esp8266_http_server.hasArg("firmwareserver")) {
    char tmp_buffer[FIRMWARE_SERVER_LEN];
    esp8266_http_server.arg("firmwareserver").toCharArray(tmp_buffer, FIRMWARE_SERVER_LEN);
    SetFirmwareServer(tmp_buffer, config->firmware_server);
    sucess &= bufferInsert("firmwareserver: " + esp8266_http_server.arg("firmwareserver") + "\n");
  } else if (esp8266_http_server.hasArg("enablepassphrase")) {
    esp8266_http_server.arg("enablepassphrase").toCharArray(config->enable_passphrase, NAME_LEN);
    sucess &= bufferInsert("enablepassphrase: " + esp8266_http_server.arg("enablepassphrase") + "\n");
  } else if (esp8266_http_server.hasArg("enableiopin")) {
    config->enable_io_pin = esp8266_http_server.arg("enableiopin").toInt();
    sucess &= bufferInsert("enableiopin: " + esp8266_http_server.arg("enableiopin") + "\n");
  } else if (esp8266_http_server.hasArg("device") and esp8266_http_server.hasArg("address_segment") and
      esp8266_http_server.hasArg("iotype") and esp8266_http_server.hasArg("io_pin")) {
    unsigned int index = esp8266_http_server.arg("device").toInt();
    Connected_device device;

    int segment_counter = 0;
    for(int i = 0; i < esp8266_http_server.args(); i++){
      if(esp8266_http_server.argName(i) == "address_segment" && segment_counter < ADDRESS_SEGMENTS){
        esp8266_http_server.arg(i).toCharArray(device.address_segment[segment_counter].segment, NAME_LEN);
        sanitizeTopicSection(device.address_segment[segment_counter].segment);
        segment_counter++;
      }
    }
    for(int i = segment_counter; i < ADDRESS_SEGMENTS; i++){
      device.address_segment[segment_counter++].segment[0] = '\0';
    }

    if(esp8266_http_server.hasArg("iotype")){
      if (esp8266_http_server.arg("iotype") == "pwm") {
        device.iotype = Io_Type::pwm;
      } else if (esp8266_http_server.arg("iotype") == "onoff") {
        device.iotype = Io_Type::onoff;
      } else if (esp8266_http_server.arg("iotype") == "input") {
        device.iotype = Io_Type::input;
      } else {
        device.iotype = Io_Type::test;
      }
    }

    if(esp8266_http_server.hasArg("io_pin")){
      device.io_pin = esp8266_http_server.arg("io_pin").toInt();
    }
    if(esp8266_http_server.hasArg("io_default")){
      device.io_default = esp8266_http_server.arg("io_default").toInt();
    }
    if(esp8266_http_server.hasArg("inverted")){
      if(esp8266_http_server.arg("inverted") == "true"){
        device.inverted = true;
      } else if(esp8266_http_server.arg("inverted") == "false"){
        device.inverted = false;
      } else {
        device.inverted = esp8266_http_server.arg("inverted").toInt();
      }
    }

    SetDevice(index, device);

    sucess &= bufferInsert("device: " + esp8266_http_server.arg("device") + "\n");

    // Force reconnect to MQTT so we subscribe to any new addresses.
    mqtt->forceDisconnect();
    io->setup();
  }
  
  Persist_Data::Persistent<Config> persist_config(config);
  persist_config.writeConfig();

  Serial.println(buffer);

  Serial.println("onSet() -");
  esp8266_http_server.send((sucess ? 200 : 500), "text/plain", buffer);
}

// Set the config->pull_firmware bit in flash and reboot so we pull new firmware on
// next boot.
void HttpServer::onPullFirmware(){
  String message = "Pulling firmware\n";
  esp8266_http_server.send(200, "text/plain", message);
  
  config->pull_firmware = true;
  Persist_Data::Persistent<Config> persist_config(config);
  persist_config.writeConfig();
  
  delay(100);
  ESP.reset();
}

void HttpServer::onReset() {
  Serial.println("restarting host");
  delay(100);
  ESP.reset();

  esp8266_http_server.send(200, "text/plain", "restarting host");
}

void HttpServer::bufferClear(){
  buffer[0] = '\0';
}

bool HttpServer::bufferAppend(const String& to_add){
  char char_array[to_add.length() +1];
  to_add.toCharArray(char_array, to_add.length() +1);
  return bufferAppend(char_array);
}

bool HttpServer::bufferAppend(const char* to_add){
  strncat(buffer, to_add, buffer_size - strlen(buffer) -1);
  return ((buffer_size - strlen(buffer) -1) >= strlen(to_add));
}

bool HttpServer::bufferInsert(const String& to_insert){
  char char_array[to_insert.length() +1];
  to_insert.toCharArray(char_array, to_insert.length() +1);
  return bufferInsert(char_array);
}

bool HttpServer::bufferInsert(const char* to_insert){
  if((buffer_size - strlen(buffer) -1) >= strlen(to_insert)){
    *(buffer + strlen(to_insert) + strlen(buffer)) = '\0';
    memmove(buffer + strlen(to_insert), buffer, strlen(buffer));
    memcpy(buffer, to_insert, strlen(to_insert));
    return true;
  }
  return false;
}

