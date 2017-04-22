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
//#include "html_primatives.h"
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
                       Io* _io) :
    buffer(_buffer),
    buffer_size(_buffer_size),
    config(_config),
    brokers(_brokers),
    mdns(_mdns),
    mqtt(_mqtt),
    io(_io)
{
  esp8266_http_server = MyESP8266WebServer(HTTP_PORT);
  esp8266_http_server.on("/test", [&]() {onTest();});
  esp8266_http_server.on("/", [&]() {onRoot();});
  esp8266_http_server.on("/set", [&]() {onSet();});
  esp8266_http_server.on("/set/", [&]() {onSet();});
  esp8266_http_server.on("/reset", [&]() {onReset();});
  esp8266_http_server.on("/reset/", [&]() {onReset();});
  esp8266_http_server.on("/get", [&]() {onFileOperations();});
  esp8266_http_server.on("/login", [&]() {handleLogin();});
  esp8266_http_server.on("/isauth", [&]() {if(isAuthentified()){
                                             esp8266_http_server.send(200, "text/plain", "auth OK");
                                           } else {
                                           esp8266_http_server.send(200, "text/plain", "no auth"); 
                                           }
                                           });
  esp8266_http_server.on("/auth", [&]() {if(!esp8266_http_server.authenticate("test", "test"))
                                           return esp8266_http_server.requestAuthentication();
                                         esp8266_http_server.send(200, "text/plain", "Login OK");});
  esp8266_http_server.onNotFound([&]() {handleNotFound();});

  const char * headerkeys[] = {"User-Agent", "Cookie", "Referer"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  esp8266_http_server.collectHeaders(headerkeys, headerkeyssize);

  esp8266_http_server.begin();
  bufferClear();
}

//Check if header is present and correct
bool HttpServer::isAuthentified(bool redirect){
  Serial.println("isAuthentified()");
  if (esp8266_http_server.hasHeader("Cookie")){
    if(config->sessionValid(esp8266_http_server.header("Cookie"))){
      Serial.println("Authentication Successful");
      return true;
    }
  }
  Serial.println("Authentication Failed");
  if(redirect){
    esp8266_http_server.sendHeader("Location","/login");
    esp8266_http_server.sendHeader("Cache-Control","no-cache");
    esp8266_http_server.sendHeader("Set-Cookie","ESPSESSIONID=0");
    esp8266_http_server.send(307);
  }
  return false;
}

//login page, also called for disconnect
void HttpServer::handleLogin(){
  Serial.println("handleLogin()");
  String msg;
  String action("/login");

  if(esp8266_http_server.header("Referer")){
    action = esp8266_http_server.header("Referer");
  }

  if (esp8266_http_server.hasHeader("Cookie")){
    Serial.print("Found cookie: ");
    String cookie = esp8266_http_server.header("Cookie");
    Serial.println(cookie);
  }
  if (esp8266_http_server.hasArg("DISCONNECT")){
    Serial.println("Disconnection");
    esp8266_http_server.sendHeader("Location", action);
    esp8266_http_server.sendHeader("Cache-Control","no-cache");
    esp8266_http_server.sendHeader("Set-Cookie","ESPSESSIONID=0");
    esp8266_http_server.send(301);
    return;
  }
  if (esp8266_http_server.hasArg("DISCONNECT_ALL")){
    // Disconnect every bodies sessions.
    Serial.println("Disconnection");
    esp8266_http_server.sendHeader("Location", action);
    esp8266_http_server.sendHeader("Cache-Control","no-cache");
    esp8266_http_server.sendHeader("Set-Cookie","ESPSESSIONID=0");
    esp8266_http_server.send(301);
    config->session_token = 0;
    config->session_override = false;
    return;
  }
  if (esp8266_http_server.hasArg("USERNAME") && esp8266_http_server.hasArg("PASSWORD")){
    if (esp8266_http_server.arg("USERNAME") == "admin" && 
        esp8266_http_server.arg("PASSWORD") == "admin" )
    {
      if(config->sessionExpired() || config->session_token <= 0){
        config->session_token = (uint32_t)secureRandom(UINT32_MAX);
      }
      config->session_time = millis() / 1000;

      esp8266_http_server.sendHeader("Location", action);
      esp8266_http_server.sendHeader("Cache-Control","no-cache");
      String cookie_header("ESPSESSIONID=");
      cookie_header += config->session_token;
      esp8266_http_server.sendHeader("Set-Cookie", cookie_header);
      esp8266_http_server.send(301);
      Serial.println("Log in Successful");
      return;
    }
    msg = "Wrong username/password! try again.";
    Serial.println("Log in Failed");
  }
  String content = "<html><body><form action='/login' method='POST'>";
  content += "User:<input type='text' name='USERNAME' placeholder='user name'><br>";
  content += "Password:<input type='password' name='PASSWORD' placeholder='password'><br>";
  content += "<input type='submit' name='SUBMIT' value='Submit'></form>" + msg + "<br>";
  content += "</body></html>";
  esp8266_http_server.send(200, "text/html", content);
}

void HttpServer::loop(){
  esp8266_http_server.handleClient();
}

void  HttpServer::onRoot(){
  onFileOperations("index.mustache");
}

void HttpServer::onTest(){
  bufferAppend(" testing ");
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
  } else if(esp8266_http_server.hasArg("filename")){
    filename = esp8266_http_server.arg("filename");
  }

  if(filename.length()){
    if(esp8266_http_server.hasArg("action") and 
          esp8266_http_server.arg("action") == "pull")
    {
      // Pull file from server.
      Serial.print("Pull file from server: ");
      Serial.println(filename);

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
        esp8266_http_server.arg("action") == "del")
    {
      Serial.print("Deleting: ");
      Serial.println(filename);

      if(!SPIFFS.begin()){
        Serial.println("WARNING: Unable to SPIFFS.begin()");
        return;
      }
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
        esp8266_http_server.arg("action") == "raw")
    {
      Serial.print("Displaying raw mustache file: ");
      Serial.println(filename);

      if(!fileOpen(filename)){
        bufferAppend("\nUnsuccessful.\n");
        esp8266_http_server.send(404, "text/plain", buffer);
        return;
      }
      
      esp8266_http_server.streamFile(file, "text/plain");

      fileClose();
      return;
    } else if (!filename.endsWith(".mustache")){
      Serial.print("Displaying file: ");
      Serial.println(filename);

      if(!fileOpen(filename)){
        bufferAppend("\nUnsuccessful.\n");
        esp8266_http_server.send(404, "text/plain", buffer);
        return;
      }
      
      esp8266_http_server.streamFile(file, mime(filename));

      fileClose();
      return;
    } else {
      // Perform markup on .mustache template file.
      if(!fileOpen(filename)){
        bufferAppend("\nUnsuccessful.\n");
        esp8266_http_server.send(404, "text/plain", buffer);
        return;
      }
      
      esp8266_http_server.setContentLength(CONTENT_LENGTH_UNKNOWN);

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

      String session_token = esp8266_http_server.header("Cookie");
      CompileMustache compileMustache(buffer, buffer_size, config, brokers, mdns,
          mqtt, io, session_token);

      int list_depth = 0;
      bool parsing_list = false;
      int buffer_in_len = 500;
      char* buffer_in = (char*)malloc(buffer_in_len);
      int buffer_out_len = 1000;
      char* buffer_out = (char*)malloc(buffer_out_len);
      buffer_in[0] = '\0';
      buffer_out[0] = '\0';
      while(fileRead(buffer_in, buffer_in_len)){
        compileMustache.parseBuffer(buffer_in, buffer_in_len,
            buffer_out, buffer_out_len,
            list_depth, parsing_list);
        if(strlen(buffer_out)){
          esp8266_http_server.sendContent(buffer_out);
        }
        buffer_out[0] = '\0';
      }
      free(buffer_in);
      free(buffer_out);

      esp8266_http_server.sendHeader("Connection", "close");
      fileClose();

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
  if(!SPIFFS.begin()){
    Serial.println("WARNING: Unable to SPIFFS.begin()");
    return;
  }
  Dir dir = SPIFFS.openDir("/");
  while(dir.next()){
    String filename = dir.fileName();
    filename.remove(0, 1);

    File file = dir.openFile("r");
    String size(file.size());
    file.close();

    bufferAppend("<a href='get?filename=" + filename + "'>" + filename + "</a>" +
        "\t" + size + "<br>");
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
  } else if(filename.endsWith(".ico")){
    return "image/x-icon";
  }
  return "text/plain";
}

void HttpServer::onSet(){
  Serial.println("onSet() +");
  bool sucess = true;
  bufferClear();
  
  if(!config->sessionValid(esp8266_http_server.header("Cookie"))){
    Serial.println("Not allowed to onSet()");
    esp8266_http_server.send(401, "text/html", "Not allowed to onSet()");
    return;
  }

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
  esp8266_http_server.send(200, "text/plain", "restarting host");
  Serial.println("restarting host");
  delay(100);
  ESP.reset();
}

void HttpServer::bufferClear(){
  //Serial.println("bufferClear");
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
                                 Io* _io,
                                 const String& _session_token) :
  buffer(_buffer),  
  buffer_size(_buffer_size),
  config(_config),
  brokers(_brokers),
  mdns(_mdns),
  mqtt(_mqtt),
  io(_io),
  list_parent(),
  session_token(_session_token)
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

void CompileMustache::parseBuffer(char* buffer_in, int buffer_in_len,
                                  char*& buffer_out, int& buffer_out_len,
                                  int& list_depth, bool& parsing_list)
{
  char tag[TAG_NAME_LEN];
  tagType type;

  char* buffer_tail = buffer_in;

  while(buffer_out && strlen(buffer_tail)){
    wdt_reset();

    char* tag_start = findPattern(buffer_tail, "{{",
        strnlen(buffer_tail, buffer_in + buffer_in_len - buffer_tail) -1);
    if(!tag_start && *(buffer_tail + strlen(buffer_tail) -1) == '{'){
      // Special case where last character is the start of a tag.
      tag_start = buffer_tail + strlen(buffer_tail) -1;
    }

    if(parsing_list){
      // In process of copying to separate list buffer.

      char* close_tag = findClosingTag(buffer_tail,
          buffer_in + buffer_in_len - buffer_tail,
          list_template[list_depth].tag);

      unsigned int len = 0;
      if(close_tag){
        len = close_tag - buffer_tail;
      } else {
        char* last_tag_start = tag_start;
        char* tmp_last_tag_start = tag_start;

        while(tmp_last_tag_start){
          tmp_last_tag_start =
            findPattern(tmp_last_tag_start +1, "{{", strnlen(tmp_last_tag_start +1, 
                        buffer_in + buffer_in_len - tmp_last_tag_start -1));
          if(tmp_last_tag_start){
            last_tag_start = tmp_last_tag_start;
          }
        }
        

        len = strlen(buffer_tail);
        if(last_tag_start && 
            !findPattern(last_tag_start +1, "}}", strnlen(last_tag_start +1,
                         buffer_in + buffer_in_len - tmp_last_tag_start -1)))
        {
          // The closing tag either overlaps buffer boundary or not in buffer at all.
          len = last_tag_start - buffer_tail;
        }
      }

      unsigned int len_buff_space =
          list_template[list_depth].buffer_len - strlen(list_template[list_depth].buffer) -1;
      if(len > len_buff_space){
        // Only enough space in output buffer for content before {{tag}}.
        list_template[list_depth].buffer_len += len;
        list_template[list_depth].buffer =
          (char*)realloc((void*)list_template[list_depth].buffer,
          list_template[list_depth].buffer_len);
        if(!list_template[list_depth].buffer){
          Serial.println("ERROR: Failed realloc()");
          parsing_list = false;
          break;
        }
      }

      strncat(list_template[list_depth].buffer, buffer_tail, len);

      if(!close_tag){
        if(len == strlen(buffer_tail)){
          // Whole buffer is added to list_template[list_depth].buffer.
          buffer_tail = buffer_in;
          buffer_tail[0] = '\0';
        } else {
          buffer_tail += len;
        }

        if(list_depth > 1){
          Serial.print("WARNING: Can not find closing tag: ");
          Serial.print("{{/");
          Serial.print(list_template[list_depth].tag);
          Serial.println("}}");
        }
        break;
      }

      char tag_content[128];
      int element_count = 0;
      replaceTag(tag_content, element_count,
                  list_template[list_depth].tag, tagList, 128, list_depth);

      if(list_template[list_depth].inverted){
        element_count = !bool(element_count);
      }
      int tmp_element_count;
      for(int i = 0; i < element_count; i++){
        replaceTag(tag_content, tmp_element_count,
                    list_template[list_depth].tag, tagListItem, 128, list_depth);

        int tmp_buffer_in_len = list_template[list_depth].buffer_len;
        char* tmp_buffer_in = (char*)malloc(tmp_buffer_in_len);
        memcpy(tmp_buffer_in, list_template[list_depth].buffer, tmp_buffer_in_len);
        bool parsing_list_inner = false;
        parseBuffer(tmp_buffer_in, tmp_buffer_in_len,
                    buffer_out, buffer_out_len, list_depth, parsing_list_inner);
        free(tmp_buffer_in);
      }

      buffer_tail = close_tag + strlen(list_template[list_depth].tag) + strlen("{{/}}");
      
      free(list_template[list_depth].buffer);
      list_template[list_depth].buffer = NULL;
      list_template[list_depth].buffer_len = 0;
      exitList(list_parent);
      list_depth--;
      parsing_list = false;
        
    } else if(tag_start && tagName(tag_start, tag, type)){
      // {{tag}} found.

      int len_to_tag = tag_start - buffer_tail;
      int len_buff_space = buffer_out_len - strlen(buffer_out) -1;
      if(len_to_tag > len_buff_space){
        // Only enough space in output buffer for content before {{tag}}.
        buffer_out_len += len_to_tag + TAG_NAME_LEN;
        buffer_out = (char*)realloc((void*)buffer_out, buffer_out_len);
        if(!buffer_out){
          Serial.println("ERROR: Failed realloc()");
          break;
        }
      }

      strncat(buffer_out, buffer_tail, len_to_tag);
      len_buff_space -= len_to_tag;

      char tag_content[128];
      int element_count = 0;

      if(type == tagPlain){
        replaceTag(tag_content, element_count, tag, type, 128, list_depth);
        if((int)strlen(tag_content) > len_buff_space){
          len_buff_space = strlen(tag_content);
          buffer_out_len += strlen(tag_content);
          buffer_out = (char*)realloc((void*)buffer_out, buffer_out_len);
          if(!buffer_out){
            Serial.println("ERROR: Failed realloc()");
            break;
          }
        }
        strncat(buffer_out, tag_content, len_buff_space);
        buffer_tail += len_to_tag + strnlen(tag, 128) + strlen("{{}}");
      } else if(type == tagList || type == tagInverted){

        parsing_list = true;
        list_depth++;
        enterList(list_parent, tag);

        strncpy(list_template[list_depth].tag, tag, TAG_NAME_LEN);
        list_template[list_depth].inverted = (type == tagInverted);
        list_template[list_depth].buffer = (char*)malloc(1);
        list_template[list_depth].buffer_len = 1;
        list_template[list_depth].buffer[0] = '\0';
        buffer_tail = tag_start + strlen(tag) + strlen("{{#}}");
      }
    } else if(tag_start &&
        tag_start >= (buffer_in + buffer_in_len - TAG_NAME_LEN - strlen("{{#}}")))
    {
      // File content with no {{tag}} followed by a {{tag}} near the end.
      // Since the {{tag}} is possibly not completely in the buffer_in, only
      // process everything up to {{tag}}.
      if(tag_start - buffer_tail <= 0){
        // All data worth sending has been added to buffer_out.
        break;
      }
      if(tag_start - buffer_tail > 
          buffer_out_len - (int)strlen(buffer_out) -1)
      {
        buffer_out_len += tag_start - buffer_tail;
        buffer_out = (char*)realloc((void*)buffer_out, buffer_out_len);
      }
      strncat(buffer_out, buffer_tail, tag_start - buffer_tail);
      buffer_tail += tag_start - buffer_tail;
    } else {
      // No {{tag}} in buffer_in. Copy everything to buffer_out.
      if(strlen(buffer_tail) > buffer_out_len - strlen(buffer_out) -1){
        buffer_out_len += strlen(buffer_tail);
        buffer_out = (char*)realloc((void*)buffer_out, buffer_out_len);
      }
      strncat(buffer_out, buffer_tail, strlen(buffer_tail));
      buffer_tail += strlen(buffer_tail);
    }
  }

  memmove(buffer_in, buffer_tail, strlen(buffer_tail) +1);
}

char* CompileMustache::findClosingTag(char* buffer_in, const int buffer_in_len,
                                      const char* tag)
{
  char full_tag[TAG_NAME_LEN + strlen("{{/}}") +1];
  full_tag[0] = '\0';
  strcat(full_tag, "{{/");
  strcat(full_tag, tag);
  strcat(full_tag, "}}");
  return findPattern(buffer_in, full_tag, buffer_in_len);
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
        *tag_start != '#' and *tag_start != '^'){
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
    }

    tag_start ++;
  }
  if(type == tagUnset){
    type = tagPlain;
  }

  if(findPattern(tag_start, "}}", TAG_NAME_LEN)){
    strncpy(tag, tag_start, TAG_NAME_LEN);
    char* end_pattern = findPattern(tag, "}}", TAG_NAME_LEN);
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

void CompileMustache::replaceTag(char* destination,
                                  int& element_count,
                                  const char* tag,
                                  const tagType type,
                                  const int len,
                                  const int list_depth)
{
  /*Serial.print("t  |");
  Serial.print(tag);
  Serial.print("  ");
  Serial.println(type);*/

  memset(destination, '\0', len);
  element_count = 0;

  if(strcmp(tag, "session.browser_auth_token") == 0){
    session_token.toCharArray(destination, len);
    element_count = 1;
  } else if(strcmp(tag, "session.current_auth_token") == 0){
    String current_session_token("ESPSESSIONID=");
    current_session_token += config->session_token;
    current_session_token.toCharArray(destination, len);
    element_count = 1;
  } else if(strcmp(tag, "session.override_auth") == 0){
    String(config->session_override).toCharArray(destination, len);
    element_count = (int)config->session_override;
  } else if(strcmp(tag, "session.valid_until") == 0){
    String(config->session_time + SESSION_TIMEOUT).toCharArray(destination, len);
    element_count = 1;
  } else if(strcmp(tag, "session.valid") == 0){
    const unsigned int now = millis() / 1000;
    const int remaining = config->session_time + SESSION_TIMEOUT - now;
    if(config->sessionValid(session_token) && remaining > 0){
      String(config->session_time + SESSION_TIMEOUT - now).toCharArray(destination, len);
      element_count = 1;
    } else {
      String("expired").toCharArray(destination, len);
      element_count = 0;
    }
  } else if(strcmp(tag, "session.login_form") == 0){

  } else if(strcmp(tag, "host.mac") == 0){
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String mac_str = macToStr(mac);
    mac_str.toCharArray(destination, len);
    element_count = 1;
  } else if(strcmp(tag, "config.hostname") == 0){
    strncpy(destination, config->hostname, min(HOSTNAME_LEN, len));
    element_count = (bool)strlen(destination);
  } else if(strcmp(tag, "host.ip") == 0){
    ip_to_string(WiFi.localIP()).toCharArray(destination, len);
    element_count = (bool)WiFi.localIP();
  } else if(strcmp(tag, "config.ip") == 0){
    ip_to_string(config->ip).toCharArray(destination, len);
    element_count = (bool)config->ip;
  } else if(strcmp(tag, "config.subnet") == 0){
    ip_to_string(config->subnet).toCharArray(destination, len);
    element_count = (bool)config->subnet;
  } else if(strcmp(tag, "config.gateway") == 0){
    ip_to_string(config->gateway).toCharArray(destination, len);
    element_count = (bool)config->gateway;
  } else if(strcmp(tag, "config.brokerip") == 0){
    ip_to_string(config->brokerip).toCharArray(destination, len);
    element_count = (bool)config->brokerip;
  } else if(strcmp(tag, "config.brokerport") == 0){
    String(config->brokerport).toCharArray(destination, len);
    element_count = config->brokerport > 0;
  } else if(strcmp(tag, "config.subscribeprefix") == 0){
    String(config->subscribeprefix).toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "config.publishprefix") == 0){
    String(config->publishprefix).toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "config.firmwarehost") == 0){
    String(config->firmwarehost).toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "config.firmwaredirectory") == 0){
    String(config->firmwaredirectory).toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "config.firmwareport") == 0){
    String(config->firmwareport).toCharArray(destination, len);
    element_count = config->firmwareport > 0;
  } else if(strcmp(tag, "config.enablepassphrase") == 0){
    String(config->enablepassphrase).toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "host.rssi") == 0){
    String(WiFi.RSSI()).toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "host.cpu_freqency") == 0){
    String(ESP.getCpuFreqMHz()).toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "host.flash_size") == 0){
    String(ESP.getFlashChipSize()).toCharArray(destination, len);
    element_count = ESP.getFlashChipSize() > 0;
  } else if(strcmp(tag, "host.flash_space") == 0){
    String(ESP.getFreeSketchSpace()).toCharArray(destination, len);
    element_count = ESP.getFreeSketchSpace() > 0;
  } else if(strcmp(tag, "host.flash_ratio") == 0){
    element_count = 0;
    if(ESP.getFlashChipSize() > 0){
      String(int(100 * ESP.getFreeSketchSpace() / ESP.getFlashChipSize()))
        .toCharArray(destination, len);
      element_count = (100 * ESP.getFreeSketchSpace() / ESP.getFlashChipSize()) >= 1;
    }
  } else if(strcmp(tag, "host.flash_speed") == 0){
    String(ESP.getFlashChipSpeed()).toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "host.free_memory") == 0){
    String(ESP.getFreeHeap()).toCharArray(destination, len);
    element_count = ESP.getFreeHeap() >= 1;
  } else if(strcmp(tag, "host.sdk_version") == 0){
    strncpy(destination, ESP.getSdkVersion(), len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "host.core_version") == 0){
    ESP.getCoreVersion().toCharArray(destination, len);
    element_count = (bool)strnlen(destination, len);
  } else if(strcmp(tag, "io.analogue_in") == 0){
    int val = analogRead(A0);
    String(val).toCharArray(destination, len);
    element_count = val > 0;
  } else if(strcmp(tag, "host.uptime") == 0){
    String(millis() / 1000).toCharArray(destination, len);
    element_count = (millis() / 1000) >= 10;
  } else if(strcmp(tag, "host.buffer_size") == 0){
    String(buffer_size).toCharArray(destination, len);
    element_count = buffer_size > 0;
  } else if(strcmp(tag, "mdns.packet_count") == 0){
    String(mdns->packet_count).toCharArray(destination, len);
    element_count = mdns->packet_count > 0;
  } else if(strcmp(tag, "mdns.buffer_fail") == 0){
    String(mdns->buffer_size_fail).toCharArray(destination, len);
    element_count = 0;
  } else if(strcmp(tag, "mdns.buffer_sucess") == 0){
    String(mdns->packet_count - mdns->buffer_size_fail).toCharArray(destination, len);
    element_count = (mdns->packet_count - mdns->buffer_size_fail) > 0;
  } else if(strcmp(tag, "mdns.largest_packet_seen") == 0){
    String(mdns->largest_packet_seen).toCharArray(destination, len);
    element_count = mdns->largest_packet_seen > 0;
  } else if(strcmp(tag, "mdns.success_rate") == 0){
    element_count = 0;
    if(mdns->packet_count > 0){
      String(100 - (100 * mdns->buffer_size_fail / mdns->packet_count))
        .toCharArray(destination, len);
      element_count = (100 * mdns->buffer_size_fail / mdns->packet_count) >= 1;
    }
  } else if(strcmp(tag, "fs.files") == 0){
    unsigned int now = millis() / 10000;  // 10 Second intervals.
    if(list_size[list_depth] < 0 || now != list_cache_time[list_depth]){
      list_cache_time[list_depth] = now;
      list_size[list_depth] = 0;
    
      Dir dir = SPIFFS.openDir("/");
      while(dir.next()){
        list_size[list_depth]++;
      }
    }
    if(type == tagListItem){
      list_element[list_depth]++;
    } else {
      list_element[list_depth] = -1;
      String(list_size[list_depth]).toCharArray(destination, len);
      element_count = list_size[list_depth];
    }
  } else if(strcmp(tag, "fs.space.used") == 0){
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    String(fs_info.usedBytes).toCharArray(destination, len);
    element_count = fs_info.usedBytes > 0;
  } else if(strcmp(tag, "fs.space.size") == 0){
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    String(fs_info.totalBytes).toCharArray(destination, len);
    element_count = fs_info.totalBytes > 0;
  } else if(strcmp(tag, "fs.space.remaining") == 0){
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    String(fs_info.totalBytes - fs_info.usedBytes).toCharArray(destination, len);
    element_count = (fs_info.totalBytes - fs_info.usedBytes) > 0;
  } else if(strcmp(tag, "fs.space.ratio.used") == 0){
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(fs_info.totalBytes > 0){
      String(100.0 * fs_info.usedBytes / fs_info.totalBytes).toCharArray(destination, len);
      element_count = (100.0 * fs_info.usedBytes / fs_info.totalBytes) >= 1;
    }
  } else if(strcmp(tag, "fs.space.ratio.remaining") == 0){
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    if(fs_info.totalBytes > 0){
      String(100.0 * (fs_info.totalBytes - fs_info.usedBytes) / fs_info.totalBytes)
        .toCharArray(destination, len);
      element_count =
        (100.0 * (fs_info.totalBytes - fs_info.usedBytes) / fs_info.totalBytes) >= 1;
    }
  } else if(strcmp(tag, "fs.max_filename_len") == 0){
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    String(fs_info.maxPathLength -2).toCharArray(destination, len);
  } else if(strcmp(tag, "servers.mqtt") == 0){
    Host* p_host;
    bool active;
    if(type == tagListItem){
      brokers->IterateHosts(&p_host, &active);
      list_element[list_depth]++;
    } else {
      list_size[list_depth] = 0;
      brokers->ResetIterater();
      while(brokers->IterateHosts(&p_host, &active)){
        list_size[list_depth]++;
      }
      brokers->ResetIterater();
      list_element[list_depth] = -1;
      String(list_size[list_depth]).toCharArray(destination, len);
      element_count = list_size[list_depth];
    }
  } else if(strcmp(tag, "host.ssids") == 0){
    unsigned int now = millis() / 10000;  // 10 Second intervals.
    if(list_size[list_depth] < 0 || now != list_cache_time[list_depth]){
      list_cache_time[list_depth] = now;
      list_size[list_depth] = WiFi.scanNetworks();
    }

    if(type == tagListItem){
      list_element[list_depth]++;
    } else {
      list_element[list_depth] = -1;
      String(list_size[list_depth]).toCharArray(destination, len);
      element_count = list_size[list_depth];
    }
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

    if(type == tagListItem){
      list_element[list_depth]++;
      element_count = list_size[list_depth];
    } else {
      list_element[list_depth] = -1;
      String(list_size[list_depth]).toCharArray(destination, len);
      element_count = list_size[list_depth];
    }
  } else if(strcmp(tag, "iopin") == 0){
    list_size[list_depth] = 11;  // 11 IO pins available on the esp8266.
    if(type == tagListItem){
      list_element[list_depth]++;
      element_count = list_size[list_depth];
    } else if(type == tagEnd || type == tagList){
      list_element[list_depth] = -1;
      String(list_size[list_depth]).toCharArray(destination, len);
      element_count = list_size[list_depth];
    } else {
      int context_depth = depthOfParent(list_parent, "|io.entry");
      int selected_value = config->devices[list_element[context_depth]].iopin;
      String(selected_value).toCharArray(destination, len);
      element_count = 1;
    }
  } else if(strcmp(tag, "iotype") == 0){
    list_size[list_depth] = 6;
    if(type == tagListItem){
      list_element[list_depth]++;
      element_count = list_size[list_depth];
    } else if(type == tagEnd || type == tagList){
      list_element[list_depth] = -1;
      String(list_size[list_depth]).toCharArray(destination, len);
      element_count = list_size[list_depth];
    } else {
      char values[][12] = {"test", "onoff", "pwm", "inputpullup", "input", "timer"};
      int context_depth = depthOfParent(list_parent, "|io.entry");
      Io_Type selected_value = config->devices[list_element[context_depth]].io_type;
      String(values[selected_value]).toCharArray(destination, len);
      element_count = 1;
    }
  } else if(strcmp(tag, "ws_health") == 0){
    String div = "<span class=\"ws_health\">ws_health</span>";
    div.toCharArray(destination, len);
    element_count = 1;
  } else if(strcmp(tag, "ws_state") == 0){
    String div = "<span class=\"ws_state\">ws_state</span>";
    div.toCharArray(destination, len);
    element_count = 1;
  }

  // The following tags work inside lists.
  if(strstr(list_parent, "|io.entry")){
    int parent_depth = depthOfParent(list_parent, "|io.entry");

    // Not every config->devices entry is populated.
    int index = config->labelToIndex(list_element[parent_depth]);

    if(strcmp(tag, "index") == 0){
      String(index).toCharArray(destination, len);
      element_count = index > 0;
    } else if(strcmp(tag, "topic") == 0){
      DeviceAddress(config->devices[index]).toCharArray(destination, len);
      element_count = strlen(destination) > 0;
    } else if(strcmp(tag, "default") == 0){
      String(config->devices[index].io_default).toCharArray(destination, len);
      element_count = strlen(destination) > 0;
    } else if(strcmp(tag, "inverted") == 0){
      String(config->devices[index].inverted ? "Y":"N").toCharArray(destination, len);
      element_count = config->devices[index].inverted;
    } else if(strcmp(tag, "is_populated") == 0){
      bool is_populated = (bool)DeviceAddress(config->devices[index]).length();
      String(is_populated ? "Y":"N").toCharArray(destination, len);
      element_count = (int)is_populated;
    } else if(strcmp(tag, "is_input") == 0){
      bool is_input = (config->devices[index].io_type == Io_Type::inputpullup ||
                      config->devices[index].io_type == Io_Type::input);
      String(is_input ? "Y":"N").toCharArray(destination, len);
      element_count = (int)is_input;
    } else if(strcmp(tag, "is_output") == 0){
      bool is_output = (config->devices[index].io_type == Io_Type::onoff ||
                      config->devices[index].io_type == Io_Type::pwm);
      String(is_output ? "Y":"N").toCharArray(destination, len);
      element_count = (int)is_output;
    } else if(strcmp(tag, "value") == 0){
      int output = 0;
      if(config->devices[index].io_type == Io_Type::inputpullup ||
          config->devices[index].io_type == Io_Type::input ||
          config->devices[index].io_type == Io_Type::onoff)
      {
        output = (bool)config->devices[index].io_value ^ config->devices[index].inverted;
      } else if(config->devices[index].io_type == Io_Type::pwm)
      {
        output = config->devices[index].io_value;
      }
      String(output).toCharArray(destination, len);
      element_count = (int)output;
    } else if(strcmp(tag, "ws_value") == 0){
      String div = "<div class=\"ws_iopin_";
      div += String(config->devices[index].iopin);
      div += "_value\">iopin_";
      div += String(config->devices[index].iopin);
      div += "_value</div>";
      div.toCharArray(destination, len);
      element_count = 1;
    } else if(strcmp(tag, "ws_set_high") == 0){
      String div = "<div class=\"ws_iopin_";
      div += String(config->devices[index].iopin);
      div += "_set_high\">click me</div>";
      div.toCharArray(destination, len);
      element_count = 1;
    } else if(strcmp(tag, "ws_set_low") == 0){
      String div = "<div class=\"ws_iopin_";
      div += String(config->devices[index].iopin);
      div += "_set_low\">click me</div>";
      div.toCharArray(destination, len);
      element_count = 1;
    } else if(strcmp(tag, "ws_set_toggle") == 0){
      String class_id = "ws_iopin_";
      class_id += String(config->devices[index].iopin);
      class_id += "_set_toggle";
      class_id.toCharArray(destination, len);
      element_count = 1;
    }
  }

  if(strstr(list_parent, "|iotype")){
    int parent_depth = depthOfParent(list_parent, "|iotype");
    char values[][12] = {"test", "onoff", "pwm", "inputpullup", "input", "timer"};
    
    if(strcmp(tag, "value") == 0){
      element_count = list_element[list_depth] > 0;
      String(values[list_element[list_depth]]).toCharArray(destination, len);
    } else if(strcmp(tag, "selected") == 0){
      int context_depth = depthOfParent(list_parent, "|io.entry");
      Io_Type selected_value = config->devices[list_element[context_depth]].io_type;

      String(values[selected_value]).toCharArray(destination, len);
      // XXX Is it ok to compare int with Io_Type ?
      element_count = (list_element[parent_depth] == selected_value);
    }
  }

  if(strstr(list_parent, "|iopin")){
    int parent_depth = depthOfParent(list_parent, "|iopin");
    int values[] = {0,1,2,3,4,5,12,13,14,15,16};  // Valid output pins.
    
    if(strcmp(tag, "value") == 0){
      element_count = list_element[list_depth] > 0;
      String(values[list_element[list_depth]]).toCharArray(destination, len);
    } else if(strcmp(tag, "selected") == 0){
      // iopin can be used from more than one context.
      // We need to determine which context and calculate the selected value
      // based on that.
      // TODO: It would be preferable to pass in a value somehow.

      int selected_value = 0;
      if(strstr(list_parent, "|io.entry")){
        // Within the {{#io.entry} context.
        int context_depth = depthOfParent(list_parent, "|io.entry");
        selected_value = config->devices[list_element[context_depth]].iopin;
      } else {
        // Otherwise it's probably the enableiopin context.
        selected_value = config->enableiopin;
      }

      String(selected_value).toCharArray(destination, len);
      element_count = (values[list_element[parent_depth]] == selected_value);
    }
  }

  if(strstr(list_parent, "|host.ssids")){
    if(strcmp(tag, "name") == 0){
      WiFi.SSID(list_element[list_depth]).toCharArray(destination, len);
    } else if(strcmp(tag, "signal") == 0){
      String(WiFi.RSSI(list_element[list_depth])).toCharArray(destination, len);
    }
  }

  if(strstr(list_parent, "|fs.files")){
    int parent_depth = depthOfParent(list_parent, "|fs.files");

    Dir dir = SPIFFS.openDir("/");
    for(int i=0; i <= list_element[parent_depth] && dir.next(); i++){ }

    if(strcmp(tag, "filename") == 0){
      if(type == tagPlain){
        File file = dir.openFile("r");
        String filename = file.name();
        filename.remove(0, 1);
        filename.toCharArray(destination, len);
        file.close();
      }
    } else if(strcmp(tag, "size") == 0){
      if(type == tagPlain){
        File file = dir.openFile("r");
        String(file.size()).toCharArray(destination, len);
        file.close();
      }
    } else if(strcmp(tag, "is_mustache") == 0){
      File file = dir.openFile("r");
      String filename = file.name();
      file.close();
      bool test = filename.endsWith(".mustache");

      if(type == tagListItem){
        element_count = list_element[parent_depth];
      } else {
        element_count = (int)test;
        String(test ? "Y":"N").toCharArray(destination, len);
      }
    }
  }

  if(strstr(list_parent, "|servers.mqtt")){
    Host* p_host;
    bool active;
    brokers->GetLastHost(&p_host, active);
    if(strcmp(tag, "service_name") == 0){
      if(type == tagPlain){
        p_host->service_name.toCharArray(destination, len);
      }
    } else if(strcmp(tag, "host_name") == 0){
      if(type == tagPlain){
        p_host->host_name.toCharArray(destination, len);
      }
    } else if(strcmp(tag, "address") == 0){
      if(type == tagPlain){
        ip_to_string(p_host->address).toCharArray(destination, len);
      }
    } else if(strcmp(tag, "port") == 0){
      if(type == tagPlain){
        String(p_host->port).toCharArray(destination, len);
      }
    } else if(strcmp(tag, "service_valid_until") == 0){
      if(type == tagPlain){
        String(p_host->service_valid_until).toCharArray(destination, len);
      }
    } else if(strcmp(tag, "host_valid_until") == 0){
      if(type == tagPlain){
        String(p_host->host_valid_until).toCharArray(destination, len);
      }
    } else if(strcmp(tag, "ipv4_valid_until") == 0){
      if(type == tagPlain){
        String(p_host->ipv4_valid_until).toCharArray(destination, len);
      }
    } else if(strcmp(tag, "success_counter") == 0){
      if(type == tagPlain){
        String(p_host->success_counter).toCharArray(destination, len);
      } else if(type == tagList){
      } else if(type == tagInverted){
      }
    } else if(strcmp(tag, "fail_counter") == 0){
      if(type == tagPlain){
        String(p_host->fail_counter).toCharArray(destination, len);
      } else if(type == tagList){
      } else if(type == tagInverted){
      }
    } else if(strcmp(tag, "connection_attempts") == 0){
      if(type == tagPlain){
        String(p_host->success_counter + p_host->fail_counter).toCharArray(destination, len);
      } else if(type == tagList){
      } else if(type == tagInverted){
      }
    } else if(strcmp(tag, "active") == 0){
      if(type == tagPlain){
        if(active){
          strncpy(destination, "active", len);
        }
      } else if(type == tagListItem){
        int parent_depth = depthOfParent(list_parent, "|fs.files");
        element_count = list_element[parent_depth];
      } else {
        element_count = (int)active;
        String(active ? "Y":"N").toCharArray(destination, len);
      }
    }
  }

  /*Serial.print("td |");
  Serial.println(destination);*/
}

void CompileMustache::enterList(char* parents, char* tag){
  strcat(parents, "|");
  strcat(parents, tag);
}

void CompileMustache::exitList(char* parents){
  char* tmp = strrchr(parents, '|');
  if(tmp){
    *tmp = '\0';
  }
}


char* CompileMustache::findPattern(char* buff, const char* pattern, int line_len)
  const{
  for(int i = 0; i < line_len; i++){
    if(strncmp (buff + i, pattern, strlen(pattern)) == 0){
      return buff +i;
    }
  }
  return NULL;
}
