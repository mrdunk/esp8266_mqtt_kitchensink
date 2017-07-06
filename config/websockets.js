var log;
var websocket = null;
var ws_data_timer;
var ws_start_timer;
var MAX_TIME = 10 * 1000;
var KEEPALIVE_TIME = 2000;
var counter = 0;
var wsMessages = {};
var wsMessagesTimer;
var ws_receive_callbacks = [];
var ws_data = {};

if(localStorage.esp8266_kitchensink === ""){
  localStorage.esp8266_kitchensink = "{}";
} else {
  console.log(localStorage.esp8266_kitchensink);
  ws_data = JSON.parse(localStorage.esp8266_kitchensink);
}

function wsCodes(value){
  var codes = ["CONNECTING", "OPEN", "CLOSING", "CLOSED"];
  return codes[value];
}

function parsePayload(payload, log){
  var topic = payload.split(":", 1)[0];
  payload = payload.slice(payload.indexOf(":") +1);
  var message;
  try{
    message = JSON.parse(payload);
    if(log && !message._ack){
      log.innerHTML += "<p style='color: blue;'>> JSON received: " + payload + "</p>";
    }
  } catch(err){
    if(log){
      log.innerHTML += "<p style='color: black;'>> text received: " + payload + "</p>";
    }
    // console.log(err);
    // console.log(payload);
    return;
  }

  message.topic = topic;
  return message;
}

function setIo(){
  console.log(this);
  var payload = {_command: !Boolean(parseInt(this.value)),
                 _subject: this.topic};
  console.log(payload);
  wsQueueSend(payload);
}

function updatePage(payload){
  var class_key = "";

  if(payload._iopin){
    class_key = "ws_iopin_" + payload._iopin + "_zero";
    for(var i=0; i < document.getElementsByClassName(class_key).length; i++){
      var match = document.getElementsByClassName(class_key)[i];
      if(payload._state === "0"){
        match.classList.add("io_visible");
        match.classList.remove("io_hidden");
      } else {
        match.classList.add("io_hidden");
        match.classList.remove("io_visible");
      }
    }

    class_key = "ws_iopin_" + payload._iopin + "_positive";
    for(var i=0; i < document.getElementsByClassName(class_key).length; i++){
      var match = document.getElementsByClassName(class_key)[i];
      if(payload._state !== "0"){
        match.classList.add("io_visible");
        match.classList.remove("io_hidden");
      } else {
        match.classList.add("io_hidden");
        match.classList.remove("io_visible");
      }
    }

    class_key = "ws_iopin_" + payload._iopin + "_value";
    for(var i=0; i < document.getElementsByClassName(class_key).length; i++){
      var match = document.getElementsByClassName(class_key)[i];
      match.innerHTML = payload._state;
    }

    class_key = "ws_iopin_" + payload._iopin + "_set_toggle";
    for(var i=0; i < document.getElementsByClassName(class_key).length; i++){
      var match = document.getElementsByClassName(class_key)[i];
      match.style.cursor = "pointer";
      match.onclick = setIo.bind({topic: match.getAttribute("topic"), value: payload._state});
    }
  }
}

function updateHealth(){
  var class_key = "ws_health";
  for(var i=0; i < document.getElementsByClassName(class_key).length; i++){
    var match = document.getElementsByClassName(class_key)[i];
    var time = (Date.now() - ws_data_timer);
    match.innerHTML = Math.round(time / 100) / 10;
    if(time >= MAX_TIME){
      match.style["background-color"] = "red";
    } else if(time >= MAX_TIME /2){
      match.style["background-color"] = "orange";
    } else {
      match.style["background-color"] = "white";
    }
  }
  class_key = "ws_state";
  for(var i=0; i < document.getElementsByClassName(class_key).length; i++){
    var match = document.getElementsByClassName(class_key)[i];
    var time = (Date.now() - ws_data_timer);
    match.innerHTML = (!websocket || wsCodes(websocket.readyState));
    if(match.innerHTML === "OPEN"){
      match.style["background-color"] = "green";
    }else if(match.innerHTML === "CONNECING"){
      match.style["background-color"] = "orange";
    } else {
      match.style["background-color"] = "red";
    }
  }
}

function wsCleanup(){
  if(websocket){
    console.log("wsCleanup()");
    websocket.close();
    websocket.onclose = null;
    websocket = null;
  }
}

function wsStart(){
  if((Date.now() - ws_start_timer) < MAX_TIME){
    return;
  }
  console.log("wsStart()");

  ws_start_timer = Date.now();
  wsCleanup();
  if(log){
    log.innerHTML += "<p>> New WS</p>";
  }

  console.log("new ws");
  var url = "ws://" + location.hostname + ":81";
  try{
    websocket = new WebSocket(url);
  } catch(e) {
    console.log(e);
    wsCleanup();
    return;
  }

	websocket.onopen = function() {
    console.log("websocket.onopen");
    ws_data_timer = Date.now();
    if(log){
		  log.innerHTML += "<p>> WS Connected</p>";
    }
    var publishprefix = "homeautomation/0/";
    var solicit_topic = publishprefix + "_all/_all";
    var message = {_command: "solicit",
                   _subject : solicit_topic};
    wsQueueSend(message);
	};

  websocket.onmessage = function(evt) {
    ws_data_timer = Date.now();
    var payload = parsePayload(evt.data, log);
    if(payload !== undefined){
      updatePage(payload);
      if(payload._command === "teach"){
        //console.log("websocket.onmessage", payload);

        // Remove Incoming message if it is a reply to one in the send queue.
        wsDeQueue(payload);

        var container = ws_data;
        var path = payload.name.split(".");
        console.log(path.join("."));

        for(var i = 0; i < path.length; i++){
          var depth;
          var name = path[i];
          if(parseInt(path[i +1], 10).toString() === path[i +1]){
            depth = parseInt(path[i +1], 10);
            i++;
          } else {
            depth = undefined;
          }

          if(i < path.length -1){
            if(typeof(container[name]) !== "object"){
              if(depth === undefined){
                container[name] = {};
              } else {
                container[name] = [];
              }
            }
          } else if(container[name] === undefined ||
              typeof(container[name]) === "string")
          {
            container[name] = payload.content;
          }

          if(depth === undefined){
            container = container[name];
          } else {
            if(!container.hasOwnProperty(name)){
              container[name] = [];
            }
            if(!container[name].hasOwnProperty(depth)){
              container[name][depth] = {};
            }
            container = container[name][depth];
          }
        }

        localStorage.esp8266_kitchensink = JSON.stringify(ws_data);

        for(var j = 0; j < ws_receive_callbacks.length; j++){
          ws_receive_callbacks[j](path);
        }

        var message = {_command: "ack",
                       path : payload.name,
                       id: payload.id,
                       sequence: payload.sequence,
                       _subject: "hosts/_all"};
        wsQueueSend(message);
      }
    }
  };

  websocket.onerror = function(evt) {
    console.log("websocket.onerror", evt);
    if(log){
      log.innerHTML += "<p style='color: red;'>> WS ERROR: " + evt.data + "</p>";
    }
  };

  websocket.onclose = function(evt) {
    console.log("websocket.onclose", evt);
    if(log){
      log.innerHTML += "<p style='color: red;'>> WS closed: " + evt.code + "</p>";
    }
  };
}

function wsCheck(){
  //console.log(!websocket || websocket.readyState);
  if(websocket && websocket.readyState === WebSocket.OPEN &&
      Date.now() - ws_data_timer >= KEEPALIVE_TIME){
    counter += 1;
    websocket.send("{\"_ping\":\"" + counter + "\"}");
  }
  if(!websocket || websocket.readyState === WebSocket.CLOSED ||
      websocket.readyState === WebSocket.CLOSING)
  {
    wsStart();
  }
  if(websocket && websocket.readyState === WebSocket.OPEN &&
      Date.now() - ws_data_timer > MAX_TIME)
  {
    console.log("wsCheck() fail");
    if(log){
      log.innerHTML += "<p style='color: red;'>> WS timeout</p>";
    }
    wsStart();
  }
  updateHealth();
}

function wsInit() {
  console.log("wsInit()");

  log = document.getElementById("log");
  if(log){
    log.innerHTML = "Log:";
  }
  ws_data_timer = Date.now();
  wsStart();
  window.setInterval(wsCheck, KEEPALIVE_TIME);
};

function wsQueueSend(message){
  //console.log("wsQueueSend(", message, ")", message.name, wsMessagesTimer);

  if(message !== undefined){
    if(message.name === undefined){
      wsMessages[JSON.stringify(message)] = message;
    } else {
      wsMessages[message.name] = message;
    }
  }
  if(wsMessagesTimer === undefined){
    wsSend();
  }
}

function wsSend(){
  //console.log("wsSend()", Object.getOwnPropertyNames(wsMessages).length,
  //            websocket.readyState, wsMessagesTimer);
  if(Object.getOwnPropertyNames(wsMessages).length === 0){
    // Nothing in send queue.
    clearTimeout(wsMessagesTimer);
    wsMessagesTimer = undefined;
    return;
  }
  if(websocket.readyState !== WebSocket.OPEN){
    clearTimeout(wsMessagesTimer);
    wsMessagesTimer = setTimeout(wsSend, 1000);
    wsCheck();
    console.log("  websocket not open. TODO: reopen?");
    return;
  }

  var popped;
  try{
    var name = Object.getOwnPropertyNames(wsMessages)[0];
    popped = wsMessages[name];
    websocket.send(JSON.stringify(popped));
    if(log){
      log.innerHTML += "<p style='color: green;'>> WS publish: " +
                       JSON.stringify(popped) + "</p>";
    }

    // Leave "learn" and "update" requests in the send buffer until a reply arrives.
    if(popped._command !== "learn" && popped._command !== "update"){
      wsDeQueue(popped);
    }
  }catch(err){
    console.log(err);
  }

  clearTimeout(wsMessagesTimer);
  wsMessagesTimer = setTimeout(wsSend, 1000);
}

function wsDeQueue(message){
  var name;
  if(message.name === undefined){
    name = JSON.stringify(message);
  } else {
    name = message.name;
  }
  if(wsMessages[name] !== undefined){
    delete wsMessages[name];
    wsSend();
  }
}

function valueAtPath(path){
  path = path.split('.');
  var pointer = ws_data;
  for(var i = 0; i < path.length; i++){
    if(pointer[path[i]]){
      pointer = pointer[path[i]];
    } else {
      return undefined;
    }
  }
  if(pointer instanceof Array){
    for(var i = 0; i < pointer.length; i++){
      if(pointer[i].selected === "1"){
        pointer = pointer[i].value;
        break;
      }
    }
  }
  return pointer;
}
