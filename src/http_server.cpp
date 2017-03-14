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


#include <ESP8266WebServer.h>
#include "FS.h"

#include "http_server.h"
#include "html_primatives.h"
#include "ipv4_helpers.h"
#include "config.h"
#include "serve_files.h"

extern void setPullFirmware(bool pull);

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
  esp8266_http_server = MyESP8266WebServer(HTTP_PORT);
  esp8266_http_server.on("/favicon.ico", [&]() {
      esp8266_http_server.send(404, "text/plain", "todo");});
  esp8266_http_server.on("/test", [&]() {onTest();});
  esp8266_http_server.on("/", [&]() {onRoot();});
  esp8266_http_server.on("/configure", [&]() {onConfig();});
  esp8266_http_server.on("/configure/", [&]() {onConfig();});
  esp8266_http_server.on("/set", [&]() {onSet();});
  esp8266_http_server.on("/set/", [&]() {onSet();});
  esp8266_http_server.on("/reset", [&]() {onReset();});
  esp8266_http_server.on("/reset/", [&]() {onReset();});
  esp8266_http_server.on("/get", [&]() {onFileOperations();});
  esp8266_http_server.onNotFound([&]() {handleNotFound();});

  esp8266_http_server.begin();
  bufferClear();
}


void HttpServer::loop(){
  esp8266_http_server.handleClient();
}

void  HttpServer::onRoot(){
  onFileOperations("index.mustache");
}

void HttpServer::onTest(){
  bufferAppend("testing");
  esp8266_http_server.send(200, "text/plain", buffer);
}

void HttpServer::handleNotFound(){
  String filename = esp8266_http_server.uri();
  filename.remove(0, 1); // Leading "/" character.
  onFileOperations(filename);
}

void HttpServer::onFileOperations(const String& _filename){
  bufferClear();

  String filename = "";
  if(_filename.length()){
    filename = _filename;
  } else  if(esp8266_http_server.hasArg("filename")){
    filename = esp8266_http_server.arg("filename");
  }

  if(filename.length()){
    if(esp8266_http_server.hasArg("action") and 
          esp8266_http_server.arg("action") == "pull"){
      // Pull file from server.
      bufferAppend("Pulling firmware from " +
          String(config->firmwarehost) + ":" + String(config->firmwareport) +
          String(config->firmwaredirectory) + filename + "\n");
      if(filename == "firmware.bin"){
        esp8266_http_server.send(200, "text/plain", buffer);
        // Set flag in persistent filesystem and reboot so we pull new firmware on
        // next boot.
        setPullFirmware(true);

        delay(100);
        ESP.reset();
      } else {
        if(!pullFile(filename, *config)){
          bufferAppend("\nUnsuccessful.\n");
          esp8266_http_server.send(404, "text/plain", buffer);
          return;
        }
        bufferAppend("Successfully got file from server.\n");
        esp8266_http_server.send(200, "text/plain", buffer);
        return;
      }
    } else if(esp8266_http_server.hasArg("action") and
        esp8266_http_server.arg("action") == "del"){
      bool result = SPIFFS.begin();
      if(!SPIFFS.exists(String("/") + filename)){
        bufferAppend("\nUnsuccessful.\n");
        esp8266_http_server.send(404, "text/plain", buffer);
        SPIFFS.end();
        return;
      }

      if(!SPIFFS.remove(String("/") + filename)){
        SPIFFS.end();
        bufferAppend("\nUnsuccessful.\n");
        esp8266_http_server.send(404, "text/plain", buffer);
        return;
      }
      SPIFFS.end();
      buffer[0] = '\0';
      bufferAppend("Successfully deleted ");
      bufferAppend(filename);
      esp8266_http_server.send(200, "text/plain", buffer);
    } else if(esp8266_http_server.hasArg("action") and
        esp8266_http_server.arg("action") == "raw"){

      esp8266_http_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      esp8266_http_server.send(200, "text/plain", "");

      // Callback to pass to the read() method.
      std::function<bool(char*, int)> callback = 
          [&server = esp8266_http_server](char* buffer, int len)
      {
        server.sendContent(buffer);
        buffer[0] = '\0';
        return true;
      };

      if(!fileOpen(filename)){
        bufferAppend("\nUnsuccessful.\n");
        esp8266_http_server.send(404, "text/plain", buffer);
        return;
      }
      
      while(fileRead(buffer, buffer_size)){
        Serial.println(strlen(buffer));
        esp8266_http_server.sendContent(buffer);
        bufferClear();
      }

      fileClose();
      return;
    } else {
      // Display default view of file.
      if(!fileOpen(filename)){
        bufferAppend("\nUnsuccessful.\n");
        esp8266_http_server.send(404, "text/plain", buffer);
        return;
      }
      
      esp8266_http_server.setContentLength(CONTENT_LENGTH_UNKNOWN);

      if(filename.endsWith(".css")){
        esp8266_http_server.sendHeader("Cache-Control", "max-age=3600");
      }


      if(esp8266_http_server.hasArg("action") and
          esp8266_http_server.arg("action") == "compiled_source"){
        Serial.print("Displaying compiled_source for: ");
        Serial.println(filename);
        esp8266_http_server.send(200, "text/plain", "");
      } else {
          Serial.print("Displaying: ");
          Serial.println(filename);
        esp8266_http_server.send(200, mime(filename), "");
      }

      CompileMustache compileMustache(buffer, buffer_size, config, brokers, mdns, mqtt, io);

      int list_depth = 0;
      bool parsing_list = false;
      int buffer_in_len = 200;
      char* buffer_in = (char*)malloc(buffer_in_len);
      int buffer_out_len = 2000;
      char* buffer_out = (char*)malloc(buffer_out_len);
      buffer_in[0] = '\0';
      buffer_out[0] = '\0';
      while(fileRead(buffer_in, buffer_in_len)){
        Serial.println("-----------------------");
        compileMustache.parseBuffer(buffer_in, buffer_in_len,
                                    buffer_out, buffer_out_len,
                                    list_depth, parsing_list);
        esp8266_http_server.sendContent(buffer_out);
        buffer_out[0] = '\0';
      }
      free(buffer_in);
      free(buffer_out);

      esp8266_http_server.sendHeader("Connection", "close");
      fileClose();

      Serial.println(filename);
      Serial.print("buffer length: ");
      Serial.println(strnlen(buffer, buffer_size));

      //esp8266_http_server.send(200, mime(filename), buffer);
      return;
    }
  } else {
    // Filename not specified.
    fileBrowser();
    return;
  }

  esp8266_http_server.send(200, "text/plain", buffer);
}

void HttpServer::fileBrowser(){
  bool result = SPIFFS.begin();
  Dir dir = SPIFFS.openDir("/");
  while(dir.next()){
    String filename = dir.fileName();
    filename.remove(0, 1);

    File file = dir.openFile("r");
    String size(file.size());
    file.close();

    bufferAppend(link(filename, "get?filename=" + filename) + "\t" + size + "\n");
  }
  SPIFFS.end();
  esp8266_http_server.send(200, "text/html", buffer);
  return;
}

bool HttpServer::fileOpen(const String& filename){
	bool result = SPIFFS.begin();
  if(!result){
		Serial.println("Unable to use SPIFFS.");
    bufferAppend("Unable to use SPIFFS.");
    SPIFFS.end();
    return false;
  }

	// this opens the file in read-mode
	file = SPIFFS.open("/" + filename, "r");

	if (!file) {
		Serial.print("File doesn't exist: ");
    Serial.println(filename);
    bufferAppend("File doesn't exist: " + filename);

    SPIFFS.end();
    return false;
  }
  return true;
}

bool HttpServer::fileRead(char* buffer_in, const int buffer_in_len){
  int available = file.available();
  int starting_len = strnlen(buffer_in, buffer_in_len);
  /*char* buffer_tail = buffer_in + strnlen(buffer_in, buffer_in_len);
  int buffer_remaining = (buffer_in_len /2) - strlen(buffer_in);
  while(available) {
    int got = file.readBytesUntil('\n', buffer_tail, min(buffer_remaining, available));
    buffer_tail[got] = '\0';

    if(buffer_tail[got -1] != 13 || got == 0){
      Serial.print("break ");
      Serial.print(strlen(buffer_in));
      Serial.print(" ");
      Serial.println(available);
      Serial.println(buffer_in);
      buffer_tail[0] = '\0';
      file.seek(-got, SeekCur);
      break;
    }
    buffer_tail += got;
    buffer_remaining -= got;
    available = file.available();
  }*/
  int got = file.readBytesUntil('\0', buffer_in + starting_len,
                                min((buffer_in_len - starting_len -1), available));
  buffer_in[starting_len + got] = '\0';

  return ((available > 0) || (strnlen(buffer_in, buffer_in_len) > 0));
}

void HttpServer::fileClose(){
	if(file){
    file.close();
  }
  SPIFFS.end();
}

const String HttpServer::mime(const String& filename){
  if(filename.endsWith(".css")){
    return "text/css";
  } else if(filename.endsWith(".js")){
    return "application/javascript";
  } else if(filename.endsWith(".htm") ||
            filename.endsWith(".html") ||
            filename.endsWith(".mustache"))
  {
    return "text/html";
  }
  return "text/plain";
}

void HttpServer::onConfig(){
  Serial.println("onConfig() +");
  bool sucess = true;
  bufferClear();

  if(*allow_config){
    (*allow_config)--;
  }
  if(*allow_config <= 0 && esp8266_http_server.hasArg("enablepassphrase") &&
      config->enablepassphrase != "" &&
      esp8266_http_server.arg("enablepassphrase") == config->enablepassphrase){
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

    sucess &= bufferAppend(descriptionListItem("Static MQTT broker ip",
        ipField("brokerip", ip_to_string(config->brokerip),
                ip_to_string(config->brokerip), "brokerip") +
        submit("Save", "save_brokerip" , "save('brokerip')") +
        String("(0.0.0.0 to use mDNS auto discovery)")));
    sucess &= bufferAppend(descriptionListItem("Static MQTT broker port",
        portValue(config->brokerport, "brokerport") +
        submit("Save", "save_brokerport" , "save('brokerport')")));
    sucess &= bufferAppend(descriptionListItem("MQTT subscription prefix",
        textField("subscribeprefix", "subscribeprefix", config->subscribeprefix,
          "subscribeprefix") +
        submit("Save", "save_subscribeprefix" , "save('subscribeprefix')")));
    sucess &= bufferAppend(descriptionListItem("MQTT publish prefix",
        textField("publishprefix", "publishprefix", config->publishprefix,
          "publishprefix") +
        submit("Save", "save_publishprefix" , "save('publishprefix')")));
    sucess &= bufferAppend(descriptionListItem("&nbsp", "&nbsp"));
    
    sucess &= bufferAppend(descriptionListItem("HTTP Firmware host",
        textField("firmwarehost", "firmwarehost", config->firmwarehost,
          "firmwarehost") +
        submit("Save", "save_firmwarehost" , "save('firmwarehost')")));
    sucess &= bufferAppend(descriptionListItem("HTTP Firmware directory",
        textField("firmwaredirectory", "firmwaredirectory", config->firmwaredirectory,
          "firmwaredirectory") +
        submit("Save", "save_firmwaredirectory" , "save('firmwaredirectory')")));
    sucess &= bufferAppend(descriptionListItem("HTTP Firmware port",
        portValue(config->firmwareport, "firmwareport") +
        submit("Save", "save_firmwareport" , "save('firmwareport')")));
    sucess &= bufferAppend(descriptionListItem("Config enable passphrase",
        textField("enablepassphrase", "enablepassphrase", config->enablepassphrase,
          "enablepassphrase") +
        submit("Save", "save_enablepassphrase" , "save('enablepassphrase')")));
    sucess &= bufferAppend(descriptionListItem("Config enable IO pin",
        ioPin(config->enableiopin, "enableiopin") +
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
        sucess &= bufferAppend(cell(config->subscribeprefix + String("/") +
            textField(name, "some/topic", DeviceAddress(config->devices[i]),
              "device_" + String(i) + "_topic")));
        sucess &= bufferAppend(cell(outletType(TypeToString(config->devices[i].io_type),
                                               "device_" + String(i) + "_iotype")));
        sucess &= bufferAppend(cell(ioPin(config->devices[i].iopin,
              "device_" + String(i) + "_iopin")));
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
      sucess &= bufferAppend(cell(config->subscribeprefix + String("/") +
          textField(name, "new/topic", "", "device_" + String(empty_device) + "_topic")));
      sucess &= bufferAppend(cell(outletType("onoff", "device_" +
                                              String(empty_device) + "_iotype")));
      name = "pin_";
      name.concat(empty_device);
      sucess &= bufferAppend(cell(ioPin(0, "device_" + String(empty_device) + "_iopin")));
      sucess &= bufferAppend(cell(ioValue(0, "device_" + String(empty_device) + "_io_default")));
      sucess &= bufferAppend(cell(ioInverted(false, "device_" +
                                             String(empty_device) + "_inverted")));
      sucess &= bufferAppend(cell(submit("Save", "save_" + String(empty_device),
            "save('device_" + String(empty_device) + "')")));
      sucess &= bufferAppend(cell(""));
      sucess &= bufferAppend(rowEnd());
    }
    
    sucess &= bufferAppend(tableEnd());

    sucess &= bufferAppend(descriptionListItem("firmware.bin",
          link("view", "get?filename=firmware.bin") + "&nbsp;&nbsp;&nbsp;" +
          link("pull_from_server", "/get?action=pull&filename=firmware.bin")));
    sucess &= bufferAppend(descriptionListItem("config.cfg",
          link("view", "get?filename=config.cfg") + "&nbsp;&nbsp;&nbsp;" +
          link("pull_from_server", "get?action=pull&filename=config.cfg")));
    sucess &= bufferAppend(descriptionListItem("script.js",
          link("view", "get?filename=script.js") + "&nbsp;&nbsp;&nbsp;" +
          link("pull_from_server", "get?action=pull&filename=script.js")));
    sucess &= bufferAppend(descriptionListItem("style.css",
          link("view", "get?filename=style.css") + "&nbsp;&nbsp;&nbsp;" +
          link("pull_from_server", "get?action=pull&filename=style.css")));
  
    sucess &= bufferInsert(listStart());
    sucess &= sucess &= bufferAppend(listEnd());
  } else {
    Serial.println("Not allowed to onConfig()");
    sucess &= bufferAppend("Configuration mode not enabled.<br>Press button connected to IO ");
    sucess &= bufferAppend(String(config->enableiopin));
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
  
  Serial.print("allow_config: ");
  Serial.println(*allow_config);

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
    config->brokerip = string_to_ip(esp8266_http_server.arg("brokerip"));
    sucess &= bufferInsert("brokerip: " + esp8266_http_server.arg("brokerip") + "\n");
  } else if (esp8266_http_server.hasArg("brokerport")) {
    config->brokerport = esp8266_http_server.arg("brokerport").toInt();
    sucess &= bufferInsert("brokerport: " + esp8266_http_server.arg("brokerport") + "\n");
  } else if (esp8266_http_server.hasArg("hostname")) {
    SetHostname(esp8266_http_server.arg("hostname").c_str());
    sucess &= bufferInsert("hostname: " + esp8266_http_server.arg("hostname") + "\n");
  } else if (esp8266_http_server.hasArg("publishprefix")) {
    SetPrefix(esp8266_http_server.arg("publishprefix").c_str(), config->publishprefix);
    sucess &= bufferInsert("publishprefix: " + esp8266_http_server.arg("publishprefix") + "\n");
  } else if (esp8266_http_server.hasArg("subscribeprefix")) {
    SetPrefix(esp8266_http_server.arg("subscribeprefix").c_str(), config->subscribeprefix);
    sucess &= bufferInsert(
        "subscribeprefix: " + esp8266_http_server.arg("subscribeprefix") + "\n");
  } else if (esp8266_http_server.hasArg("firmwarehost")) {
    SetFirmwareServer(esp8266_http_server.arg("firmwarehost").c_str(), config->firmwarehost);
    sucess &= bufferInsert("firmwarehost: " + esp8266_http_server.arg("firmwarehost") + "\n");
  } else if (esp8266_http_server.hasArg("firmwaredirectory")) {
    SetFirmwareServer(esp8266_http_server.arg("firmwaredirectory").c_str(),
                      config->firmwaredirectory);
    sucess &= bufferInsert("firmwaredirectory: " + 
                           esp8266_http_server.arg("firmwaredirectory") + "\n");
  } else if (esp8266_http_server.hasArg("firmwareport")) {
    config->firmwareport = esp8266_http_server.arg("firmwareport").toInt();
    sucess &= bufferInsert("firmwareport: " + esp8266_http_server.arg("firmwareport") + "\n");
  } else if (esp8266_http_server.hasArg("enablepassphrase")) {
    esp8266_http_server.arg("enablepassphrase").toCharArray(config->enablepassphrase,
                                                            STRING_LEN);
    sucess &= bufferInsert(
        "enablepassphrase: " + esp8266_http_server.arg("enablepassphrase") + "\n");
  } else if (esp8266_http_server.hasArg("enableiopin")) {
    config->enableiopin = esp8266_http_server.arg("enableiopin").toInt();
    sucess &= bufferInsert("enableiopin: " + esp8266_http_server.arg("enableiopin") + "\n");
  } else if (esp8266_http_server.hasArg("device") and
             esp8266_http_server.hasArg("address_segment") and
             esp8266_http_server.hasArg("iotype") and esp8266_http_server.hasArg("iopin")) {
    unsigned int index = esp8266_http_server.arg("device").toInt();
    Connected_device device;

    int segment_counter = 0;
    for(int i = 0; i < esp8266_http_server.args(); i++){
      if(esp8266_http_server.argName(i) == "address_segment" &&
          segment_counter < ADDRESS_SEGMENTS)
      {
        esp8266_http_server.arg(i).toCharArray(
            device.address_segment[segment_counter].segment, NAME_LEN);
        sanitizeTopicSection(device.address_segment[segment_counter].segment);
        segment_counter++;
      }
    }
    for(int i = segment_counter; i < ADDRESS_SEGMENTS; i++){
      device.address_segment[segment_counter++].segment[0] = '\0';
    }

    if(esp8266_http_server.hasArg("iotype")){
      device.setType(esp8266_http_server.arg("iotype"));
    }

    if(esp8266_http_server.hasArg("iopin")){
      device.iopin = esp8266_http_server.arg("iopin").toInt();
    }
    if(esp8266_http_server.hasArg("io_default")){
      device.io_default = esp8266_http_server.arg("io_default").toInt();
    }
    if(esp8266_http_server.hasArg("inverted")){
      device.setInverted(esp8266_http_server.arg("inverted"));
    }

    SetDevice(index, device);

    sucess &= bufferInsert("device: " + esp8266_http_server.arg("device") + "\n");

    // Force reconnect to MQTT so we subscribe to any new addresses.
    mqtt->forceDisconnect();
    io->setup();
  }
  

  Serial.println(buffer);

  Serial.println("onSet() -");
  if(sucess){
    //Persist_Data::Persistent<Config> persist_config(config);
    //persist_config.writeConfig();
    config->save();
  }
  esp8266_http_server.send((sucess ? 200 : 500), "text/plain", buffer);
}

void HttpServer::onReset() {
  Serial.println("restarting host");
  delay(100);
  ESP.reset();

  esp8266_http_server.send(200, "text/plain", "restarting host");
}

void HttpServer::bufferClear(){
  Serial.println("bufferClear");
  buffer[0] = '\0';
}

bool HttpServer::bufferAppend(const String& to_add){
  return bufferAppend(to_add.c_str());
}

bool HttpServer::bufferAppend(const char* to_add){
  strncat(buffer, to_add, buffer_size - strlen(buffer) -1);
  return ((buffer_size - strlen(buffer) -1) >= strlen(to_add));
}

bool HttpServer::bufferInsert(const String& to_insert){
  return bufferInsert(to_insert.c_str());
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

CompileMustache::CompileMustache(char* _buffer, 
                                 const int _buffer_size,
                                 Config* _config,
                                 MdnsLookup* _brokers,
                                 mdns::MDns* _mdns,
                                 Mqtt* _mqtt,
                                 Io* _io) :
  buffer(_buffer),  
  buffer_size(_buffer_size),
  config(_config),
  brokers(_brokers),
  mdns(_mdns),
  mqtt(_mqtt),
  io(_io),
  list_depth(0),
  list_parent()
{
  for(int i = 0; i < MAX_LIST_RECURSION; i++){
    list_element[i] = -1;
    list_size[i] = -1;
  }
  
  // Gets set false if buffer over-runs.
  success = true;
}

bool CompileMustache::myMemmove(char* destination, char* source, int len){
  if(!success){
    return false;
  }
  if(destination + len > buffer + buffer_size){
    Serial.println("Error: Exceeded buffer size.");
    len = buffer + buffer_size - destination;
    return false;
  }
  memmove(destination, source, len);
  return true;
}

void CompileMustache::parseBuffer(char* buffer_in, const int buffer_in_len,
                                  char* buffer_out, int& buffer_out_len,
                                  int& list_depth, bool& parsing_list)
{
  Serial.print("bi |");
  Serial.println(buffer_in);

  //buffer_out[0] = '\0';
  char tag[TAG_NAME_LEN];
  tagType type;

  char* buffer_tail = buffer_in;

  while(strlen(buffer_out) < buffer_out_len -1 && strlen(buffer_tail)){
    char* tag_start = findPattern(buffer_tail, "{{",
        min((strnlen(buffer_tail, buffer_in + buffer_in_len - buffer_tail) -1),
          (buffer_out_len - strlen(buffer_out) -1)), false);

    if(parsing_list){
      // In process of copying to separate list buffer.
      Serial.println(0);

      char* close_tag = findClosingTag(buffer_tail,
          buffer_in + buffer_in_len - buffer_tail,
          list_template[list_depth].tag);

      int len;
      if(close_tag){
        len = close_tag - buffer_tail;
      } else {
        char* last_tag_start = tag_start;
        char* tmp_last_tag_start = tag_start;

        while(tmp_last_tag_start){
          tmp_last_tag_start =
            findPattern(tmp_last_tag_start +1, "{{",
                        strnlen(tmp_last_tag_start +1, 
                                buffer_in + buffer_in_len - tmp_last_tag_start -1),
                        false);
          if(tmp_last_tag_start){
            last_tag_start = tmp_last_tag_start;
          }
        }
        
        len = buffer_in_len;
        if(last_tag_start && 
            !findPattern(last_tag_start +1, "}}",
                         strnlen(last_tag_start +1,
                                buffer_in + buffer_in_len - tmp_last_tag_start -1),
                         false))
        {
          // The closing tag overlaps buffer boundary.
          len = last_tag_start - buffer_in;
        }
      }

      list_template[list_depth].buffer_len = list_template[list_depth].buffer_len + len;
      list_template[list_depth].buffer =
        (char*)realloc((void*)list_template[list_depth].buffer,
            list_template[list_depth].buffer_len);
      strncat(list_template[list_depth].buffer, buffer_tail, len);

      if(!close_tag){
        buffer_tail += len;
        if(len == buffer_in_len){
          // Whole buffer is added to list_template[list_depth].buffer.
          buffer_tail[0] = '\0';
        }
        Serial.print("** |");
        Serial.println(buffer_tail);
        break;
      }

      Serial.print("bt |");
      Serial.println(list_template[list_depth].buffer);

      Serial.print("+");
      Serial.print(list_depth);
      Serial.println("~~~~~~~~~~~~~~~~~~~");
      bool parsing_list_inner = false;
      parseBuffer(list_template[list_depth].buffer, list_template[list_depth].buffer_len,
          buffer_out, buffer_out_len, list_depth, parsing_list_inner);
      Serial.print("-");
      Serial.print(list_depth);
      Serial.println("~~~~~~~~~~~~~~~~~~~");

      buffer_tail = close_tag + strlen(list_template[list_depth].tag) + strlen("{{/}}");
      
      free(list_template[list_depth].buffer);
      list_template[list_depth].buffer = NULL;
      list_depth--;
      parsing_list = false;
    } else if(tag_start && tagName(tag_start, tag, type)){
      // {{tag}} found.
      Serial.println(1);
      int l = min((buffer_out_len - strlen(buffer_out) -1), (tag_start - buffer_tail));
      strncat(buffer_out, buffer_tail, l);

      Serial.print("tag: ");
      Serial.print(tag);
      Serial.print(" ");
      Serial.println((int)type);

      if(type == tagPlain){
        Serial.println(1.1);
        strncat(buffer_out, "TAG", (buffer_out_len - strlen(buffer_out) -1));
        buffer_tail += l + strlen(tag) + strlen("{{}}");
      } else if(type == tagList || type == tagInverted){
        Serial.println(1.2);
Serial.print("|| |");
Serial.println(buffer_tail);
Serial.println(buffer_in + buffer_in_len - buffer_tail);
Serial.println(tag);

        char* close_tag =
          findClosingTag(buffer_tail, buffer_in + buffer_in_len - buffer_tail, tag);

        parsing_list = true;
        list_depth++;
        int len;
        if(close_tag){
          len = close_tag - (tag_start + strlen(tag) + strlen("{{#}}"));
        } else {
          len = 
            buffer_in + buffer_in_len - (tag_start + strlen(tag) + strlen("{{#}}"));
        }

        list_template[list_depth].buffer = (char*)malloc(len +1);
        list_template[list_depth].buffer_len = len +1;
        strncpy(list_template[list_depth].buffer,
            tag_start + strlen(tag) + strlen("{{#}}"), len);
        list_template[list_depth].buffer[len] = '\0';

        if(!close_tag){
          buffer_tail = buffer_in + buffer_in_len -1;
          strncpy(list_template[list_depth].tag, tag, TAG_NAME_LEN);
          Serial.println(1.21);
          break;
        }

        Serial.print("bt |");
        Serial.println(list_template[list_depth].buffer);

      Serial.print("+");
      Serial.print(list_depth);
      Serial.println("~~~~~~~~~~~~~~~~~~~");
      bool parsing_list_inner = false;
      parseBuffer(list_template[list_depth].buffer, list_template[list_depth].buffer_len,
          buffer_out, buffer_out_len,
          list_depth, parsing_list_inner);
      Serial.print("-");
      Serial.print(list_depth);
      Serial.println("~~~~~~~~~~~~~~~~~~~");

        free(list_template[list_depth].buffer);
        list_template[list_depth].buffer = NULL;
        list_depth--;
        parsing_list = false;

        buffer_tail = close_tag + strlen(tag) + strlen("{{/}}");

        Serial.println(1.22);
      } else if(type == tagInverted){
        // TODO.
      } else if(type == tagEnd){
        // TODO.
      }
    } else if(tag_start &&
        tag_start >= (buffer_in + buffer_in_len - TAG_NAME_LEN - strlen("{{#}}")))
    {
      // File content with no {{tag}} followed by a {{tag}} near the end.
      // Since the {{tag}} is possibly not completely in the buffer_in, only
      // process everything until {{tag}}.
      Serial.println(2);
      int l = min((buffer_out_len - strlen(buffer_out) -1), tag_start - buffer_tail);
      if(l <= 0){
        break;
      }
      strncat(buffer_out, buffer_tail, l);
      buffer_tail += l;
    } else {
      // No {{tag}} in buffer_in. Copy everything to buffer_out.
      Serial.println(3);
      int l = min((buffer_out_len - strlen(buffer_out) -1), strlen(buffer_tail));
      strncat(buffer_out, buffer_tail, l);
      buffer_tail += l;
    }
  }

  memmove(buffer_in, buffer_tail, strlen(buffer_tail) +1);

  Serial.print("bo |");
  Serial.println(buffer_out);
}

char* CompileMustache::findClosingTag(char* buffer_in, const int buffer_in_len,
                                      const char* tag)
{
  char full_tag[TAG_NAME_LEN + strlen("{{/}}")];
  strcat(full_tag, "{{/");
  strcat(full_tag, tag);
  strcat(full_tag, "}}");
  return findPattern(buffer_in, full_tag, buffer_in_len, false);
}


bool CompileMustache::tagName(char* tag_start, char* tag, tagType& type){
  type = tagUnset;
  if(!tag_start || tag_start[0] != '{' || tag_start[1] != '{'){
    return false;
  }
  tag_start += 2;

  for(int i = 0; i < TAG_NAME_LEN; i++){
    // Skip through any "{{" deliminators and whitespace to start of tag name.
    if(*tag_start >= '!' and *tag_start <= 'z' and
        *tag_start != '#' and *tag_start != '/' and
        *tag_start != '^' and *tag_start != '+'){
      break;
    }
    switch(*tag_start)
    {
      case '#':
        if(type != tagUnset) {return false;}
        type = tagList;
        break;
      case '^':
        if(type != tagUnset) {return false;}
        type = tagInverted;
        break;
      case '/':
        if(type != tagUnset) {return false;}
        type = tagEnd;
        break;
      case '+':
        if(type != tagUnset) {return false;}
        type = tagListItem;
    }

    tag_start ++;
  }
  if(type == tagUnset){
    type = tagPlain;
  }

  if(findPattern(tag_start, "}}", TAG_NAME_LEN, false)){
    strncpy(tag, tag_start, TAG_NAME_LEN);
    char* end_pattern = findPattern(tag, "}}", TAG_NAME_LEN, false);
    if(end_pattern){
      *end_pattern = '\0';
      while(strlen(tag)){
        if(tag[strlen(tag)] == ' '){
          // Remove trailing whitespace.
          tag[strlen(tag)] = '\0';
        } else {
          break;
        }
      }
      return true;
    }
    // empty string.
    tag[0] = '\0';
  }
  return false;
}

bool CompileMustache::duplicateList(char* tag_start_buf, const char* tag, const int number){
  if(!success){
    Serial.println("ERROR: Previous failure.");
    return false;
  }

  tag_start_buf += strnlen(tag, TAG_NAME_LEN) + strlen("{{#}}"); // End of start tag.
  
  // Find the end of the list-item we want to duplicate.
  char end_tag[69];
  end_tag[0] = '\0';
  strcat(end_tag, "{{/");
  strcat(end_tag, tag);
  strcat(end_tag, "}}");
  end_tag[69] = '\0';
  char* tag_end_buf = findPattern(tag_start_buf, end_tag, strlen(tag_start_buf));
  if(!tag_end_buf){
    Serial.println(end_tag);
    Serial.println(tag_start_buf);
    Serial.println("Warning: could not find end_tag within buffer.");
    return false;
  }

  // Make a new tag to deliminate copies of the list-item.
  char tag_item[69];
  tag_item[0] = '\0';
  strcat(tag_item, "{{+");
  strcat(tag_item, tag);
  strcat(tag_item, "}}");
  tag_item[69] = '\0';

  // Insert new tag.
  success &= 
    myMemmove(tag_start_buf + strlen(tag_item), tag_start_buf, strlen(tag_start_buf) +1);
  success &= myMemmove(tag_start_buf, tag_item, strlen(tag_item));

  // Move remainder of buffer rightwards and insert multiple copies of the list-item.
  tag_end_buf += strlen(tag_item);
  int tag_length = tag_end_buf - tag_start_buf;
  int move_distance = tag_length * (number -1);
  
  success &= 
    myMemmove(tag_end_buf + move_distance, tag_end_buf, strlen(tag_end_buf) +1);
  for(int i = 1; i < number; i++){
    success &= 
      myMemmove(tag_start_buf + (tag_length * i), tag_start_buf, tag_length);
  }

  return success;
}

bool CompileMustache::removeList(char* tag_start_buf, const char* tag){
  tag_start_buf += strnlen(tag, TAG_NAME_LEN) + strlen("{{#}}"); // End of start tag.

  char end_tag[69];
  end_tag[0] = '\0';
  strcat(end_tag, "{{/");
  strcat(end_tag, tag);
  strcat(end_tag, "}}");
  end_tag[69] = '\0';

  char* tag_end_buf = findPattern(tag_start_buf, end_tag, strlen(tag_start_buf));
  if(!tag_end_buf){
    return false;
  }
  
  success &= myMemmove(tag_start_buf, tag_end_buf, strlen(tag_end_buf) +1);
  
  return success;
}

bool CompileMustache::replaceTag(char* tag_position, 
                                 const char* tag,
                                 char* tag_content,
                                 tagType type)
{
  //Serial.print("tag: ");
  //Serial.print(tag);
  //Serial.print("  list_parent: ");
  //Serial.println(list_parent);

  // These tags should work anywhere. (Either inside or outside lists.
  if(strcmp(tag, "host.mac") == 0){
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String mac_str = macToStr(mac);
    mac_str.toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.hostname") == 0){
    strncpy(tag_content, config->hostname, HOSTNAME_LEN);
  } else if(strcmp(tag, "host.ip") == 0){
    ip_to_string(WiFi.localIP()).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.ip") == 0){
    ip_to_string(config->ip).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.subnet") == 0){
    ip_to_string(config->subnet).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.gateway") == 0){
    ip_to_string(config->gateway).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.brokerip") == 0){
    ip_to_string(config->brokerip).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.brokerport") == 0){
    String(config->brokerport).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.subscribeprefix") == 0){
    String(config->subscribeprefix).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.publishprefix") == 0){
    String(config->publishprefix).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.firmwarehost") == 0){
    String(config->firmwarehost).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.firmwaredirectory") == 0){
    String(config->firmwaredirectory).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.firmwareport") == 0){
    String(config->firmwareport).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "config.enablepassphrase") == 0){
    String(config->enablepassphrase).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.rssi") == 0){
    String(WiFi.RSSI()).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.cpu_freqency") == 0){
    String(ESP.getCpuFreqMHz()).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.flash_size") == 0){
    String(ESP.getFlashChipSize()).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.flash_space") == 0){
    String(ESP.getFreeSketchSpace()).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.flash_ratio") == 0){
    if(ESP.getFlashChipSize() > 0){
      String(int(100 * ESP.getFreeSketchSpace() / ESP.getFlashChipSize()))
        .toCharArray(tag_content, 128);
    }
  } else if(strcmp(tag, "host.flash_speed") == 0){
    String(ESP.getFlashChipSpeed()).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.free_memory") == 0){
    String(ESP.getFreeHeap()).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.sdk_version") == 0){
    strncpy(tag_content, ESP.getSdkVersion(), 128);
  } else if(strcmp(tag, "host.core_version") == 0){
    ESP.getCoreVersion().toCharArray(tag_content, 128);
  } else if(strcmp(tag, "io.analogue_in") == 0){
    String(analogRead(A0)).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.uptime") == 0){
    String(millis() / 1000).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "host.buffer_size") == 0){
    String(buffer_size).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "mdns.packet_count") == 0){
    String(mdns->packet_count).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "mdns.buffer_fail") == 0){
    String(mdns->buffer_size_fail).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "mdns.buffer_sucess") == 0){
    String(mdns->packet_count - mdns->buffer_size_fail).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "mdns.largest_packet_seen") == 0){
    String(mdns->largest_packet_seen).toCharArray(tag_content, 128);
  } else if(strcmp(tag, "mdns.success_rate") == 0){
    if(mdns->packet_count > 0){
      String(100 - (100 * mdns->buffer_size_fail / mdns->packet_count))
        .toCharArray(tag_content, 128);
    }
  } else if(strcmp(tag, "fs.files") == 0){
    unsigned int now = millis() / 10000;  // 10 Second intervals.
    if(list_size[list_depth] < 0 || now != list_cache_time[list_depth]){
      list_cache_time[list_depth] = now;
      list_size[list_depth] = 0;
      bool result = SPIFFS.begin();
      if(!result){
        Serial.println("SPIFFS already mounted");
      }
      Dir dir = SPIFFS.openDir("/");
      while(dir.next()){
        list_size[list_depth]++;
      }
      if(result){
        SPIFFS.end();
      }
    }
    if(type == tagPlain){
      String(list_size[list_depth]).toCharArray(tag_content, 128);
    } else if(type == tagList){
      success &= duplicateList(tag_position, tag, list_size[list_depth]);
    } else if(type == tagInverted){
      if(list_size[list_depth]){
        success &= removeList(tag_position, tag);
      }
    } else if(type == tagListItem){
      list_element[list_depth]++;
    }
  } else if(strcmp(tag, "fs.space.used") == 0){
    bool result = SPIFFS.begin();
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(result){
      if(type == tagPlain){
        String(fs_info.usedBytes).toCharArray(tag_content, 128);
      }
    }
    SPIFFS.end();
  } else if(strcmp(tag, "fs.space.size") == 0){
    bool result = SPIFFS.begin();
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(result){
      if(type == tagPlain){
        String(fs_info.totalBytes).toCharArray(tag_content, 128);
      }
    }
    SPIFFS.end();
  } else if(strcmp(tag, "fs.space.remaining") == 0){
    bool result = SPIFFS.begin();
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(result){
      if(type == tagPlain){
        String(fs_info.totalBytes - fs_info.usedBytes).toCharArray(tag_content, 128);
      }
    }
    SPIFFS.end();
  } else if(strcmp(tag, "fs.space.ratio.used") == 0){
    bool result = SPIFFS.begin();
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(result && fs_info.totalBytes > 0){
      if(type == tagPlain){
        String(100.0 * fs_info.usedBytes / fs_info.totalBytes).toCharArray(tag_content, 128);
      }
    }
    SPIFFS.end();
  } else if(strcmp(tag, "fs.space.ratio.remaining") == 0){
    bool result = SPIFFS.begin();
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(result && fs_info.totalBytes > 0){
      if(type == tagPlain){
        String(100.0 * (fs_info.totalBytes - fs_info.usedBytes) / fs_info.totalBytes)
          .toCharArray(tag_content, 128);
      }
    }
    SPIFFS.end();
  } else if(strcmp(tag, "fs.max_filename_len") == 0){
    bool result = SPIFFS.begin();
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(result){
      if(type == tagPlain){
        String(fs_info.maxPathLength -2).toCharArray(tag_content, 128);
      }
    }
    SPIFFS.end();
  } else if(strcmp(tag, "io.entry") == 0){
    list_size[list_depth] = 0;
    for (int i = 0; i < MAX_DEVICES; ++i) {
      if (strlen(config->devices[i].address_segment[0].segment) > 0) {
        list_size[list_depth]++;
      }
    }
    if(list_size[list_depth] < MAX_DEVICES){
      list_size[list_depth]++;
    }
    if(type == tagPlain){
      String(list_size[list_depth]).toCharArray(tag_content, 128);
    } else if(type == tagList){
      //success &= duplicateList(tag_position, tag, list_size[list_depth]);
      if(!duplicateList(tag_position, tag, list_size[list_depth])){
        return false;
      }
    } else if(type == tagInverted){
      if(list_size[list_depth]){
        success &= removeList(tag_position, tag);
      }
    } else if(type == tagListItem){
      list_element[list_depth]++;
    }
  } else if(strcmp(tag, "servers.mqtt") == 0){
    if(type == tagPlain){
      String(list_size[list_depth]).toCharArray(tag_content, 128);
    } else if(type == tagList){
      list_size[list_depth] = 0;
      brokers->ResetIterater();
      Host* p_host;
      bool active;
      while(brokers->IterateHosts(&p_host, &active)){
        list_size[list_depth]++;
      }
      success &= duplicateList(tag_position, tag, list_size[list_depth]);
    } else if(type == tagInverted){
      if(list_size[list_depth]){
        success &= removeList(tag_position, tag);
      }
    } else if(type == tagListItem){
      Host* p_host;
      bool active;
      brokers->IterateHosts(&p_host, &active);
      list_element[list_depth]++;
    }
  } else if(strcmp(tag, "host.ssids") == 0){
    if(type == tagPlain){
      String(list_size[list_depth]).toCharArray(tag_content, 128);
    } else if(type == tagList){
      unsigned int now = millis() / 10000;  // 10 Second intervals.
      if(list_size[list_depth] < 0 || now != list_cache_time[list_depth]){
        list_cache_time[list_depth] = now;
        list_size[list_depth] = WiFi.scanNetworks();
      }
      success &= duplicateList(tag_position, tag, list_size[list_depth]);
    } else if(type == tagInverted){
      if(list_size[list_depth]){
        success &= removeList(tag_position, tag);
      }
    } else if(type == tagListItem){
      list_element[list_depth]++;
    }
  } else if(strcmp(tag, "iopin") == 0){
    if(type == tagPlain){
      String(list_size[list_depth]).toCharArray(tag_content, 128);
    } else if(type == tagList){
      list_size[list_depth] = 11;  // 11 IO pins available on the esp8266.
      success &= duplicateList(tag_position, tag, list_size[list_depth]);
    } else if(type == tagInverted){
      if(list_size[list_depth]){
        success &= removeList(tag_position, tag);
      }
    } else if(type == tagListItem){
      list_element[list_depth]++;
    }
  }

  // The following tags work inside lists.
  if(strstr(list_parent, "|io.entry")){
    // Not every config->devices entry is populated.
    int index = config->labelToIndex(list_element[list_depth]);

    if(strcmp(tag, "index") == 0){
      String(index).toCharArray(tag_content, 128);
    } else if(strcmp(tag, "topic") == 0){
      if(type == tagPlain){
        DeviceAddress(config->devices[index]).toCharArray(tag_content, 128);
      }
    } else if(strcmp(tag, "default") == 0){
      if(type == tagPlain){
        String(config->devices[index].io_default)
          .toCharArray(tag_content, 128);
      }
    } else if(strcmp(tag, "inverted") == 0){
      if(type == tagPlain){
        String(config->devices[index].inverted ? "Y":"N")
          .toCharArray(tag_content, 128);
      } else if(type == tagList){
        if(!config->devices[index].inverted){
          success &= removeList(tag_position, tag);
        }
      } else if(type == tagInverted){
        if(config->devices[index].inverted){
          success &= removeList(tag_position, tag);
        }
      }
    }
  }
  
  if(strstr(list_parent, "|iopin")){
    int values[] = {0,1,2,3,4,5,12,13,14,15,16};  // Valid output pins.
    
    // Value of the parent.
    int io_entry_index = config->labelToIndex(list_element[list_depth -1]);  
    
    if(strcmp(tag, "value") == 0){
      String(values[list_element[list_depth]]).toCharArray(tag_content, 128);
    } else if(strcmp(tag, "selected") == 0){
      // Look at parent's parent to see what context this has been called in.
      // TODO: It would be preferable to pass in a value somehow.
      char list_parent_copy[128];
      strcpy(list_parent_copy, list_parent);
      char* parent_end = strrchr(list_parent_copy, '|');
      *parent_end = '\0';
      int selected_value =
          (strrchr(list_parent_copy, '|') &&
          (strncmp(strrchr(list_parent_copy, '|'), "|io.entry", strlen("|io.entry")) == 0)) ?
          config->devices[io_entry_index].iopin  // {{#io.entry} context
          : config->enableiopin;                 // Otherwise it's probably the enableiopin.

      if(type == tagPlain){
        String(tag_content).toCharArray(tag_content, 128); 
      } else if(type == tagList){
        if(selected_value != values[list_element[list_depth]]){
          success &= removeList(tag_position, tag);
        }
      } else if(type == tagInverted){
        if(selected_value == values[list_element[list_depth]]){
          success &= removeList(tag_position, tag);
        }
      }
    }
  }
    
  if(strstr(list_parent, "|host.ssids")){
    if(strcmp(tag, "name") == 0){
      WiFi.SSID(list_element[list_depth]).toCharArray(tag_content, 128);
    } else if(strcmp(tag, "signal") == 0){
      String(WiFi.RSSI(list_element[list_depth])).toCharArray(tag_content, 128);
    }
  }
  
  if(strstr(list_parent, "|servers.mqtt")){
    Host* p_host;
    bool active;
    brokers->GetLastHost(&p_host, active);
    if(strcmp(tag, "service_name") == 0){
      if(type == tagPlain){
        p_host->service_name.toCharArray(tag_content, 128);
      }
    } else if(strcmp(tag, "host_name") == 0){
      if(type == tagPlain){
        p_host->host_name.toCharArray(tag_content, 128);
      }
    } else if(strcmp(tag, "address") == 0){
      if(type == tagPlain){
        ip_to_string(p_host->address).toCharArray(tag_content, 128);
      } else if(type == tagList){
      } else if(type == tagInverted){
      }
    } else if(strcmp(tag, "port") == 0){
      if(type == tagPlain){
        String(p_host->port).toCharArray(tag_content, 128);
      } else if(type == tagList){
      } else if(type == tagInverted){
      }
    } else if(strcmp(tag, "service_valid_until") == 0){
      if(type == tagPlain){
        String(p_host->service_valid_until).toCharArray(tag_content, 128);
      }
    } else if(strcmp(tag, "host_valid_until") == 0){
      if(type == tagPlain){
        String(p_host->host_valid_until).toCharArray(tag_content, 128);
      }
    } else if(strcmp(tag, "ipv4_valid_until") == 0){
      if(type == tagPlain){
        String(p_host->ipv4_valid_until).toCharArray(tag_content, 128);
      }
    } else if(strcmp(tag, "success_counter") == 0){
      if(type == tagPlain){
        String(p_host->success_counter).toCharArray(tag_content, 128);
      } else if(type == tagList){
      } else if(type == tagInverted){
      }
    } else if(strcmp(tag, "fail_counter") == 0){
      if(type == tagPlain){
        String(p_host->fail_counter).toCharArray(tag_content, 128);
      } else if(type == tagList){
      } else if(type == tagInverted){
      }
    } else if(strcmp(tag, "connection_attempts") == 0){
      if(type == tagPlain){
        String(p_host->success_counter + p_host->fail_counter).toCharArray(tag_content, 128);
      } else if(type == tagList){
      } else if(type == tagInverted){
      }
    } else if(strcmp(tag, "active") == 0){
      if(type == tagPlain){
        if(active){
          strncpy(tag_content, "active", 128);
        }
      } else if(type == tagList){
        if(!active){
          success &= removeList(tag_position, tag);
        }
      } else if(type == tagInverted){
        if(active){
          success &= removeList(tag_position, tag);
        }
      }
    }
  }

  if(strstr(list_parent, "|fs.files")){
    bool result = SPIFFS.begin();
    if(result){
      Dir dir = SPIFFS.openDir("/");
      for(int i=0; i <= list_element[1] && dir.next(); i++){ }

      if(strcmp(tag, "filename") == 0){
        if(type == tagPlain){
          File file = dir.openFile("r");
          String filename = file.name();
          filename.remove(0, 1);
          filename.toCharArray(tag_content, 128);
          file.close();
        }
      } else if(strcmp(tag, "size") == 0){
        if(type == tagPlain){
          File file = dir.openFile("r");
          String(file.size()).toCharArray(tag_content, 128);
          file.close();
        }
      } else if(strcmp(tag, "is_mustache") == 0){
        File file = dir.openFile("r");
        String filename = file.name();
        file.close();
        bool test = filename.endsWith(".mustache");
        if(type == tagPlain){
          strncpy(tag_content, (test ? "Y":"N"), 128);
        } else if(type == tagList){
          if(!test){
            success &= removeList(tag_position, tag);
          }
        } else if(type == tagInverted){
          if(test){
            success &= removeList(tag_position, tag);
          }
        }
      }
    }
    SPIFFS.end();
  }

  int tag_len_diff = strlen(tag_content) - strlen(tag) - strlen("{{}}");
  char* buffer_remainder = tag_position + strlen(tag) + strlen("{{}}");
  if(type != tagPlain){
    // The other tag types are 1 char longer than tagPlain.
    tag_len_diff--;
    buffer_remainder++;
  }
  success &= 
    myMemmove(buffer_remainder + tag_len_diff, buffer_remainder, strlen(buffer_remainder) +1);
  success &= myMemmove(tag_position, tag_content, strlen(tag_content));
  return true;
}

char* CompileMustache::findPattern(char* buff, const char* pattern, int line_len, bool test)
  const{
  for(int i = 0; i < line_len; i++){
    if(test and ((int)buff + i) > ((int)buffer + buffer_size)){
      Serial.print("ERROR: over-ran buffer in findPattern() ");
      Serial.print((int)buff + i);
      Serial.print(" ");
      Serial.print((int)buffer + buffer_size);
      Serial.print(" ");
      Serial.print(line_len);
      Serial.print(" ");
      Serial.println(i);
      return NULL;
    }
    if(strncmp (buff + i, pattern, strlen(pattern)) == 0){
      return buff +i;
    }
  }
  return NULL;
}
