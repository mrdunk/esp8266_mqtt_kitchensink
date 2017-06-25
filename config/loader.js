var Loader =
{
  files: {},
  selected_file: "index.mustache",
  insert_content_timer: undefined,
  insert_content_callback_timer: undefined,

  init: function(){
    this.hashChange();
    var context = this;
    window.onhashchange = function(){console.log("hash change");
                                     context.hashChange();
                                     context.insertContent()};
    this.requestData();
    this.loadFile(this.selected_file);
    this.loadFilenames();
    ws_receive_callbacks.push(
        function loader_rx_callback(path){context.insertContent(path);});
  },

  hashChange: function(){
    console.log(window.location.hash);
    var url = window.location.hash;
    var start_filename = url.indexOf("filename=");
    if(start_filename > 0){
      start_filename += "filename=".length;
      var end_filename = url.indexOf("&");
      if(end_filename < 0){
        end_filename = url.length;
      }
      this.selected_file = (url.substring(start_filename, end_filename));
    }
  },

  loadFile: function(filename){
    console.log("loadFile(" + filename + ")");
    var oReq = new XMLHttpRequest();
    var context = this;
    oReq.addEventListener("load", function(){context.fileCallback(this, context, filename);});
    oReq.open("GET", filename);
    oReq.send();
  },

  loadFilenames: function(){
    this.loadFile("filenames.sys");
  },

  fileCallback: function(inner_context, outer_context, filename){
    if(filename === "filenames.sys"){
      var container = document.getElementById("files");
      container.innerHTML = "";
      var names = inner_context.responseText.split("\r\n");
      for(var i = 0; i < names.length; i++){
        var filename = names[i];
        var filename_split = filename.split(".");
        if(filename_split[filename_split.length -1] === "mustache"){
          var element = document.createElement("a");
          var text = document.createTextNode(filename);
          element.appendChild(text);
          element.setAttribute('href', "#filename=" + filename);
          container.appendChild(element);
          container.appendChild(document.createElement("br"));

          if(outer_context.files[filename] === undefined){
            outer_context.files[filename] = {};
          }
        }
      }
      outer_context.loadRemainingFiles();
    } else {
      if(outer_context.files[filename] === undefined){
        outer_context.files[filename] = {};
      }
      outer_context.files[filename].filename = filename;
      outer_context.files[filename].raw = inner_context.responseText;

      outer_context.files[filename].tag_root = new Tag;
      Parser.init(outer_context.files[filename].tag_root);
      Parser.parse(outer_context.files[filename].raw);

      console.log(filename, outer_context.files[filename]);

      if(filename === outer_context.selected_file){
        outer_context.insertContent();
      }
    }
  },

  loadRemainingFiles: function(){
    var names = Object.getOwnPropertyNames(this.files);
    var load_timers = 0;
    for(var i = 0; i < names.length; i++){
      var filename = names[i];
      var filename_split = filename.split(".");
      if(filename_split[filename_split.length -1] === "mustache"){
        if(this.files[filename].raw === undefined){
          var context = this;
          load_timers++;
          this.loadFile(filename);
        }
      }
    }
  },

  insertContent: function(){
    if(this.insert_content_timer !== undefined &&
        (Date.now() - this.insert_content_timer < 1000))
    {
      window.clearTimeout(this.insert_content_callback_timer);
      this.insert_content_callback_timer = window.setTimeout(this.insertContent.bind(this), 1000);
      return;
    }
    this.insert_content_timer = Date.now();

    var filename = this.selected_file;
    if(this.files[filename] === undefined || this.files[filename].raw === undefined){
      console.log(filename + " not loaded yet");
      return;
    }
    console.log("insertContent(" + filename + ")", ws_data);

    var lines = this.files[filename].raw.split('\n');
    var children = this.files[filename].tag_root.children;
    var new_lines = [];
    var last_line = 0;
    var last_pos = 0;
    
    this.contentChunk(children, last_line, last_pos, lines, new_lines, [],
        lines.length -1, lines[lines.length -1].length);

    
    var container = document.getElementById("content");
    container.innerHTML = "";

    if(true){
      var div = document.createElement('div');
      var content = "";
      for(var i=0; i < new_lines.length; i++){
        content += new_lines[i];
      }
      div.innerHTML = content;
      container.appendChild(div);
    } 
    if(false){
      for(var i=0; i < new_lines.length; i++){
        var text = document.createTextNode(new_lines[i]);
        container.appendChild(text);
        container.appendChild(document.createElement('br'));
      }
    }
  },

  // Search down the parent_path for a matching partial_path in the ws_data object.
  getFullPath : function(parent_path, partial_path){
    function validPath(path){
      var pointer = ws_data;
      for(var i = 0; i < path.length; i++){
        if(pointer instanceof Array){
          pointer = pointer[0];
        }
        if(pointer === undefined){
          return false;
        }
        var p = path[i];
        if(path[i].endsWith("_path")){
          p = path[i].substring(0, path[i].lastIndexOf("_"));
        }
        pointer = pointer[p];
        if(pointer === undefined){
          return false;
        }
      }
      return true;
    }

    partial_path = partial_path.split(".");
    var test_path; 
    for(var i = parent_path.length; i >= 0; i--){
      test_path = parent_path.slice(0, i).concat(partial_path);
      if(validPath(test_path)){
        break;
      }
    }
    return test_path;
  },

  contentChunk: function(children, last_line, last_pos, lines, new_lines,
                         parent_path, final_line, final_pos, index){
    if(index === undefined){
      index = [];
    }
    for(var i=0; i < children.length; i++){
      var child = children[i];

      path = this.getFullPath(parent_path, child.name);
      //console.log(i, parent_path, child.name.split("."), path, child.type);

      var this_line = child.line;
      for(var l = last_line; l < this_line; l++){
        if(last_pos === 0){
          new_lines.push("");
        }
        new_lines[new_lines.length -1] += lines[l].substring(last_pos, lines[l].length);
        last_pos = 0;
        last_line = l;
      }
      if(last_line !== this_line){
        new_lines.push("");
      }
      
      last_line = this_line;

      new_lines[new_lines.length -1] += lines[this_line].substring(last_pos, child.pos);
      last_pos = child.pos + child.name.length;
      if(child.type === "name"){
        new_lines[new_lines.length -1] += this.tagContent(path, index);
        // "name" tag is 4 bytes long: {{XXX}}
        last_pos += 4;
      } else if(child.type === "contains") {
        // Other tags are 5 bytes. eg: {{#XXX}}
        last_pos += 5;

        var grand_children = this.tagContent(path, index);
        if(grand_children instanceof Array){
          // pass
        } else if(grand_children === "0" || grand_children === "" ||
            grand_children === "n" || grand_children === "N"){
          grand_children = [];
        } else {
          grand_children = [true];
        }
        var last = {};
        if(grand_children.length === 0){
          var next_child = children[i +1];  // "end" tag.
          if(next_child !== undefined && next_child.line !== undefined &&
              next_child.pos !== undefined)
          {
            last.line = next_child.line;
            last.pos = next_child.pos;
          }
        }
        for(var c=0; c < grand_children.length ; c++){
          //console.log(c, ws_data);
          var child_last_line = last_line;
          var child_last_pos = last_pos;

          var closing_tag = children[i +1];

          path = parent_path.concat(child.name.split("."));
          last = this.contentChunk(child.children, child_last_line, child_last_pos,
              lines, new_lines, path, closing_tag.line, closing_tag.pos, index.concat(c));
          child_last_line = last.line;
          child_last_pos = last.pos;

          new_lines.push("");
        }
        last_line = last.line;
        last_pos = last.pos;
      } else if(child.type === "not") {
        // Other tags are 5 bytes. eg: {{#XXX}}
        last_pos += 5;

        var grand_children = this.tagContent(path, index);
        if(grand_children.length === 0 || grand_children === "0" || grand_children === ""){
          // No grandchildren so display line.
          var last = {"line": last_line, "pos": last_pos};
          
          var child_last_line = last_line;
          var child_last_pos = last_pos;

          var closing_tag = children[i +1];

          path = parent_path.concat(child.name.split("."));
          last = this.contentChunk(child.children, child_last_line, child_last_pos,
              lines, new_lines, path, closing_tag.line, closing_tag.pos, index.concat(0));

          new_lines.push("");

          last_line = last.line;
          last_pos = last.pos;
        } else {
          // Grandchildren exist so skip to end of line.
          var next_child = children[i +1];  // "end" tag.
          if(next_child !== undefined){
            last_line = next_child.line;
            last_pos = next_child.pos;
          }
        }
      } else {
        // Other tags are 5 bytes. eg: {{#XXX}}
        last_pos += 5;
      }
    }
    
    // Last bit with no tags in it.
    for(var l = last_line; l < final_line; l++){
      if(last_pos === 0){
        new_lines.push("");
      }
      if(l === final_line){
        // Only copy as far as final_pos the last time through.
        new_lines[new_lines.length -1] += lines[l].substring(last_pos, final_pos);
      } else {
        new_lines[new_lines.length -1] += lines[l].substring(last_pos, lines[l].length);
      }
      last_pos = 0;
    }
    return {"line": final_line, "pos": last_pos};
  },

  tagContent: function(path, index){
    var data_pointer = ws_data;
    var index_pointer = 0;
    var exact_path = '';
    for(var i=0; i < path.length; i++){
      if(data_pointer instanceof Array){
        data_pointer = data_pointer[index[index_pointer]];
        exact_path += index[index_pointer];
        exact_path += '.';
        index_pointer++;
      }
      exact_path += path[i];
      exact_path += '.';
      if(data_pointer === undefined || data_pointer[path[i]] === undefined){
        if(exact_path.endsWith('_path.')){
          return exact_path.split('_path.')[0];
        }
        return "(XXXX)";
      } else {
        data_pointer = data_pointer[path[i]];
      }
    }
    return data_pointer;
  },

  /* Request data from server. */
  requestData : function(){
    var summary = {_subject: "hosts/_all",
                   _command: "learn_all"};
    wsQueueSend(summary);
  },

  saveChanges : function(){
    var elements = document.getElementsByClassName("monitor_changes");
    for(var i = 0; i < elements.length; i++){
      var name = elements[i].name;
      if(name === undefined || name === ''){
        for(var c = 0; c < elements[i].classList.length; c++){
          if(elements[i].classList[c] !== 'monitor_changes'){
            name = elements[i].classList[c];
            break;
          }
        }
      }
        
      var value = elements[i].value;
      if(elements[i].type === "checkbox"){
        value = 1 * elements[i].checked;
      }

      if(value !== undefined && valueAtPath(name) !== undefined && 
          "" + value !== valueAtPath(name))
      {
        var topic;
        try {
          topic = ws_data.host.mqtt.publish_prefix;
          topic += "/hosts/";
          topic += ws_data.host.hostname;
        } catch(err){
          topic = "";
        }
        var summary = {_subject: topic,
                       _command: "update",
                       path: name,
                       value: value};
        wsQueueSend(summary);
        console.log(name, valueAtPath(name), value, elements[i].type);
      }
    }
  }

};

window.addEventListener("load", function(){
  wsInit();
  Loader.init();
}, false);

