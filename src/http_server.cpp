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
#include "ipv4_helpers.h"
#include "config.h"
#include "serve_files.h"


extern void setPullFirmware(bool pull);
extern WiFiClient wifiClient;

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
  esp8266_http_server.on("/reset", [&]() {onReset();});
  esp8266_http_server.on("/reset/", [&]() {onReset();});
  esp8266_http_server.on("/get", [&]() {onFileOperations();});
  esp8266_http_server.on("/pull", [&]() {onPullFileMenu();});
  esp8266_http_server.on("/login", [&]() {onLogin();});
  // /filenames.sys provides a list of filenames in flash and is used by the
  // loader.html file menu.
  esp8266_http_server.on("/filenames.sys", [&]() {fileBrowser(true);});
  esp8266_http_server.onNotFound([&]() {onNotFound();});

  const char * headerkeys[] = {"User-Agent", "Cookie", "Referer"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  esp8266_http_server.collectHeaders(headerkeys, headerkeyssize);

  esp8266_http_server.begin();
  bufferClear();
}

void HttpServer::onPullFileMenu(){
  Serial.println("HttpServer.onPullFileMenu()");
  bufferClear();

  bool header_received = false;
  int status_code = 0;
  String content_type = "";

  char firmwarehost[64];
  strncpy(firmwarehost, config->firmwarehost, 64);
  uint16_t firmwareport = config->firmwareport;
  String firmwaredirectory = config->firmwaredirectory;

  if(esp8266_http_server.hasArg("firmwarehost")){
    esp8266_http_server.arg("firmwarehost").toCharArray(firmwarehost, 64);
  }
  if(esp8266_http_server.hasArg("firmwareport")){
    firmwareport = esp8266_http_server.arg("firmwareport").toInt();
  }
  if(esp8266_http_server.hasArg("firmwaredirectory")){
    firmwaredirectory = esp8266_http_server.arg("firmwaredirectory");
  }

  if(firmwareport == 0){
    firmwareport = 80;
  }
  if(firmwaredirectory == ""){
    firmwaredirectory = "/";
  }
  Serial.println(firmwarehost);
  Serial.println(firmwareport);
  Serial.println(firmwaredirectory);

  // Send headers to start http connection.
  esp8266_http_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  if(esp8266_http_server.hasArg("action") and
      esp8266_http_server.arg("action") == "compiled_source"){
    esp8266_http_server.send(200, "text/plain", "");
  } else {
    esp8266_http_server.send(200, "text/html", "");
  }

  // Fetch Web page from external file server so we can re-write it with our
  // pull requests.
  if(wifiClient.connect(firmwarehost, firmwareport)){
    // Make an HTTP GET request
    wifiClient.println(String("GET ") + firmwaredirectory + " HTTP/1.1");
    wifiClient.print("Host: ");
    wifiClient.println(firmwarehost);
    wifiClient.println("Connection: close");
    wifiClient.println();
  } else {
    return;
  }

  // If there are incoming bytes, print them
	while (wifiClient.connected()){
		if(wifiClient.available()){
			String line = wifiClient.readStringUntil('\n');
      
      if(!header_received){
        line.trim();
        if(line.startsWith("HTTP/") and line.endsWith("OK")){
          line = line.substring(line.indexOf(" "));
          line.trim();
          status_code = line.substring(0, line.indexOf(" ")).toInt();
        }
        if(line.startsWith("Content-type:")){
          content_type = line.substring(line.indexOf(" "));
          content_type.trim();
        }
        if(line == ""){
          if(status_code == HTTP_OK and content_type != ""){
            header_received = true;
            Serial.print("Status code: ");
            Serial.println(status_code);
            Serial.print("Content type: ");
            Serial.println(content_type);
            Serial.println();
          } else {
            Serial.println("Malformed header.");
            return;
          }
        }
      } else {
        // Any line that contains a link should be modified.
        int filename_start = line.indexOf("<a href=\"");
        int filename_end;
        String filename = "";
        if(filename_start >= 0){
          filename_start += strlen("<a href=\"");
          filename_end = line.substring(filename_start).indexOf("\"");

          filename = line.substring(filename_start, filename_start + filename_end);
          if(sanitizeFilename(filename)){
            String modified_line = line.substring(0, filename_start);
            modified_line += "./get?filename=" + filename + "&action=pull";
            modified_line += line.substring(filename_start + filename_end);
            esp8266_http_server.sendContent(modified_line + "\n");
          }
        } else {
          // Line does not contain link so don't modify it.
          esp8266_http_server.sendContent(line + "\n");
        }
      }
    }
  }

  esp8266_http_server.sendHeader("Connection", "close");
  wifiClient.stop();
}

void HttpServer::fetchCookie(){
  config->session_token_provided = 0;
  if (esp8266_http_server.hasHeader("Cookie")){
    if(esp8266_http_server.header("Cookie").startsWith("ESPSESSIONID=")){
      String cookie =
        esp8266_http_server.header("Cookie").substring(strlen("ESPSESSIONID="));
      config->session_token_provided = strtoul(cookie.c_str(), NULL, 10);
    }
  }
}

//Check if header is present and correct
bool HttpServer::isAuthentified(bool redirect){
  Serial.println("isAuthentified()");
  fetchCookie();

  if (config->session_token != 0 &&
      config->session_token == config->session_token_provided){
    Serial.println("Authentication Successful");
    Serial.println(config->session_token);
    return true;
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
void HttpServer::onLogin(){
  Serial.println("onLogin()");
  
  if(isAuthentified()){
    esp8266_http_server.send(200, "text/plain", "logged in");
  }

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
    config->session_token_provided = 0;
    esp8266_http_server.send(301);
    return;
  }
  if (esp8266_http_server.hasArg("DISCONNECT_ALL")){
    // Disconnect every bodies sessions.
    Serial.println("Disconnection");
    esp8266_http_server.sendHeader("Location", action);
    esp8266_http_server.sendHeader("Cache-Control","no-cache");
    esp8266_http_server.sendHeader("Set-Cookie","ESPSESSIONID=0");
    config->session_token_provided = 0;
    config->session_token = 0;
    config->session_override = false;
    esp8266_http_server.send(301);
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
      config->session_token_provided = config->session_token;
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

void HttpServer::onTest(){
  bufferAppend(" testing ");
  config->save2();
  bufferAppend(" testing. ");
  esp8266_http_server.send(200, "text/plain", buffer);
}

void  HttpServer::onRoot(){
  onFileOperations("loader.html");
}

void HttpServer::onReset() {
  esp8266_http_server.send(200, "text/plain", "restarting host");
  Serial.println("restarting host");
  delay(100);
  ESP.reset();
}

void HttpServer::onNotFound(){
  String filename = esp8266_http_server.uri();
  filename.remove(0, 1); // Leading "/" character.
  onFileOperations(filename);
}

void HttpServer::onFileOperations(const String& _filename){
  bufferClear();
  fetchCookie();

  String filename = "";
  if(_filename.length()){
    filename = _filename;
  } else if(esp8266_http_server.hasArg("filename")){
    filename = esp8266_http_server.arg("filename");
  }

  if(filename.length() && sanitizeFilename(filename)){
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
        config->files = "0";
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
      config->files = "0";
      buffer[0] = '\0';
      bufferAppend("Successfully deleted ");
      bufferAppend(filename);
      esp8266_http_server.send(200, "text/plain", buffer);
    } else {
      Serial.print("Displaying file: ");
      Serial.print(filename);
      
      if(!fileOpen(filename)){
        bufferAppend("\nUnsuccessful.\n");
        esp8266_http_server.send(404, "text/plain", buffer);
        return;
      }
      
      String mime_type = mime(filename);
      if(esp8266_http_server.hasArg("action") and
          esp8266_http_server.arg("action") == "raw")
      {
        Serial.print("  (raw)");
        mime_type = "text/plain";
      }
      Serial.println();

      esp8266_http_server.sendHeader("Cache-Control", "max-age=3600");
      esp8266_http_server.streamFile(file, mime_type);

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

void HttpServer::fileBrowser(bool names_only){
  if(!SPIFFS.begin()){
    Serial.println("WARNING: Unable to SPIFFS.begin()");
    return;
  }
  Dir dir = SPIFFS.openDir("/");
  while(dir.next()){
    String filename = dir.fileName();
    filename.remove(0, 1);
    //Serial.println(filename);

    File file = dir.openFile("r");
    String size((float)file.size() / 1024);
    file.close();

    if(names_only){
      bufferAppend(filename + "\r\n");
    } else {
      bufferAppend("<a href='" + filename + "'>" + filename + "</a>\t" + size + "k<br>");
    }
  }
  SPIFFS.end();
  if(names_only){
    esp8266_http_server.send(200, "text/plain", buffer);
  } else {
    esp8266_http_server.send(200, "text/html", buffer);
  }
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

/*bool HttpServer::fileRead(char* buffer_in, const int buffer_in_len){
  int available = file.available();
  int starting_len = strnlen(buffer_in, buffer_in_len);
  
  int got = file.readBytesUntil('\0', buffer_in + starting_len,
                                min((buffer_in_len - starting_len -1), available));
  buffer_in[starting_len + got] = '\0';

  return ((available > 0) || (strnlen(buffer_in, buffer_in_len) > 0));
}*/

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

void HttpServer::bufferClear(){
  //Serial.println("bufferClear");
  buffer[0] = '\0';
}

bool HttpServer::bufferAppend(const String& to_add){
  return bufferAppend(to_add.c_str());
}

bool HttpServer::bufferAppend(const char* to_add){
  strncat(buffer, to_add, buffer_size - strlen(buffer) -1);
  if((buffer_size - strlen(buffer) -1) >= strlen(to_add)){
    return true;
  }
  Serial.println("HttpServer::bufferAppend  Buffer full.");
  return false;
}
