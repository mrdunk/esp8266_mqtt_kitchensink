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
  tag_root : undefined,
  tag_pointer : undefined,
  re_tag : new RegExp('{{{?#?/?\\^?[\\w|.]+}?}}', 'g'),

  init : function(tag_root){
    if(tag_root === undefined){
      tag_root = new Tag;
    }
    this.tag_root = tag_root;
    this.tag_root.name = "root";
    this.tag_root.type = types[0];
    this.tag_pointer = this.tag_root;
    console.log(this.tag_pointer);
  },

  examineTag : function(tag){
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

  /* Read through file line by line, identify tags and store in recursive tag
     structure.*/
  parse : function(template){
    var lines = template.split('\n');
    for(var i = 0; i < lines.length; i++){
      var line = lines[i];

      if(line.trim() !== ""){
        var matches;
        while((matches = this.re_tag.exec(line)) !== null){
          var tag = new Tag;
          tag.name = matches[0];
          tag.line = i;
          tag.pos = matches.index;
          this.examineTag(tag);
        }
      }
    }
  },

}
