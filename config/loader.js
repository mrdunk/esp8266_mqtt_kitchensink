var Loader =
{
  files: {},
  selected_file: "index.mustache",

  init: function(){
    console.log(window.location.search);
    var url = window.location.search;
    var start_filename = url.indexOf("filename=");
    if(start_filename > 0){
      start_filename += "filename=".length;
      var end_filename = url.indexOf("&");
      if(end_filename < 0){
        end_filename = url.length;
      }
      this.selected_file = (url.substring(start_filename, end_filename));
    }

    this.loadFile(this.selected_file);
    //this.loadFilenames();
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
    console.log(inner_context);
    console.log(outer_context);
    if(filename === "filenames.sys"){
      var names = inner_context.responseText.split("\r\n");
      for(var i = 0; i < names.length; i++){
        var filename = names[i];
        if(filename.split(".")[1] === "mustache" &&
            outer_context.files[filename] === undefined){
          console.log(filename);
          outer_context.files[filename] = {};
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
      Parser.requestData(Parser.tag_root, "root");

      console.log(outer_context.files[filename]);

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
      if(filename.split(".")[1] === "mustache"){
        if(this.files[filename].raw === undefined){
          var context = this;
          load_timers++;
          setTimeout(function(){context.loadFile(filename);}, 11000 * load_timers);
          //this.loadFile(filename);
        }
      }
    }
  },

  insertContent: function(){
    var container = document.getElementById("content");
    container.innerHTML = "";
    
    var filename = this.selected_file;
    var lines = this.files[filename].raw.split('\n');
    var children = this.files[filename].tag_root.children;
    var new_lines = [];
    var last_line = 0;
    var last_pos = 0;
    
      var last = this.contentChunk(children, last_line, last_pos, lines, new_lines, [],
                                   lines.length -1, lines[lines.length -1].length);
      last_line = last.line;
      last_pos = last.pos;

    //var content = "";
    for(var i=0; i < new_lines.length; i++){
      var text = document.createTextNode(new_lines[i]);
      container.appendChild(text);
      container.appendChild(document.createElement('br'));
      //content += new_lines[i];
    }
    //container.innerHTML = content;
  },

  contentChunk: function(children, last_line, last_pos, lines, new_lines,
                         parent_path, final_line, final_pos){
    for(var i=0; i < children.length; i++){
      var child = children[i];

      path = parent_path.concat(child.name.split("."));

      console.log(i, path.join("."), last_line, last_pos, child.line);

      var this_line = child.line;
      for(var l = last_line; l < this_line; l++){
        if(last_pos === 0){
          new_lines.push(l + " | ");
        }
        new_lines[new_lines.length -1] += lines[l].substring(last_pos, lines[l].length);
        last_pos = 0;
        last_line = l;
      }
      if(last_line !== this_line){
        new_lines.push(this_line + " | ");
      }
      
      last_line = this_line;

      new_lines[new_lines.length -1] += lines[this_line].substring(last_pos, child.pos);
      last_pos = child.pos + child.name.length;
      if(child.type === "name"){
        //console.log(child);
        new_lines[new_lines.length -1] += this.tagContent(path);
        // "name" tag is 4 bytes long: {{XXX}}
        last_pos += 4;
      } else if(child.type === "contains") {
        // Other tags are 5 bytes. eg: {{#XXX}}
        last_pos += 5;

        var grand_children = this.tagContent(path);
        //console.log(child);
        console.log(grand_children);
        var last = {"line": last_line, "pos": last_pos};
        for(var c=0; grand_children instanceof Array && c < grand_children.length; c++){
          var child_last_line = last_line;
          var child_last_pos = last_pos;

          var closing_tag = children[i +1];
          console.log(closing_tag);

          path = parent_path.concat(child.name.split("."));
          last = this.contentChunk(child.children, child_last_line, child_last_pos,
              lines, new_lines, path, closing_tag.line, closing_tag.pos);
          child_last_line = last.line;
          child_last_pos = last.pos;


          new_lines.push(this_line + " | ");

        }
        last_line = last.line;
        last_pos = last.pos;
      } else {
        // Other tags are 5 bytes. eg: {{#XXX}}
        last_pos += 5;
      }
    }
    
    // Last bit with no tags in it.
    for(var l = last_line; l <= final_line; l++){
      if(last_pos === 0){
        new_lines.push(l + " | ");
      }
      if(l == final_line){
        // Only copy as far as final_pos the last time through.
        new_lines[new_lines.length -1] += lines[l].substring(last_pos, final_pos);
      } else {
        new_lines[new_lines.length -1] += lines[l].substring(last_pos, lines[l].length);
      }
      last_pos = 0;
    }
    return {"line": last_line, "pos": last_pos};
  },

  tagContent: function(path){
    var data_pointer = ws_data;
    for(var i=0; i < path.length; i++){
      if(data_pointer instanceof Array){
        //return "(Array)";
        console.log(data_pointer);
        data_pointer = data_pointer[0];
        //return data_pointer;
      }
      if(data_pointer === undefined || data_pointer[path[i]] === undefined){
        return "(XXXX)";
      } else {
        data_pointer = data_pointer[path[i]];
      }
    }
    return data_pointer;
  }
};

window.addEventListener("load", function(){
  wsInit();
  Loader.init();
  setTimeout(function(){Loader.insertContent();}, 10000);
}, false);

