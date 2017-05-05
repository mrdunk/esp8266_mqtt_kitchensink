var types = ["root", "name", "contains", "not", "end"];

function Tag(){
  this.name = "";
  this.line = -1;
  this.pos = -1;
  this.type = null;
  this.children = [];
  this.markup = false;
  this.parent_ = undefined;
}

var Parser = 
{
  tag_root : new Tag,
  tag_pointer : undefined,
  re_tag : new RegExp('{{{?#?/?\\^?[\\w|.]+}?}}', 'g'),

  init : function(){
    this.tag_root.name = "root";
    this.tag_root.type = types[0];
    this.tag_pointer = this.tag_root;
    console.log(this.tag_pointer);
  },

  examineTag : function(tag){
    console.log(this.tag_pointer);
    tag.parent_ = this.tag_pointer;
    var name = tag.name.substr(2, tag.name.length -4);
    if(name.startsWith("{")){
      name = name.substr(1);
      tag.markup = true;
    }

    if(name.startsWith("#")){
      this.tag_pointer.children.push(tag);
      name = name.substr(1);
      tag.type = types[2];
      this.tag_pointer = tag;
    } else if(name.startsWith("^")){
      this.tag_pointer.children.push(tag);
      name = name.substr(1);
      tag.type = types[3];
      this.tag_pointer = tag;
    } else if(name.startsWith("/")){
      name = name.substr(1);
      tag.type = types[4];
      tag.parent_ = this.tag_pointer.parent_;
      this.tag_pointer = tag.parent_;
      this.tag_pointer.children.push(tag);
    } else {
      tag.type = types[1];
      this.tag_pointer.children.push(tag);
    }
    tag.name = name;
  },

  parse : function(template){
    var lines = template.split('\n');
    for(var i = 0;i < lines.length;i++){
      var line = lines[i];

      if(line.trim() !== ""){
        console.log(lines[i]);
        var matches;
        while((matches = this.re_tag.exec(line)) !== null){
          var tag = new Tag;
          tag.name = matches[0];
          tag.line = i;
          tag.pos = matches.index;
          this.examineTag(tag);
          console.log(tag);
        }
      }
    }
  },

  requestData : function(tag, parent_name=""){
    if(parent_name === "root"){
      parent_name = "";
    } else {
      parent_name += ".";
    }
    for(var i=0; i < tag.children.length; i++){
      var child = tag.children[i];
      var summary = {name: parent_name + child.name,
                     type: child.type,
                     _subject: "hosts/_all",
                     _command: "learn"};
      if(summary.type === "name"){
        wsQueueSend(summary);
      }
      this.requestData(child, summary.name);
    }
  }
}

window.addEventListener("load", function(){
  Parser.init();
  Parser.parse(document.getElementById("template").innerHTML);
  console.log(Parser.tag_root);
  Parser.requestData(Parser.tag_root, "root");
}, false);
