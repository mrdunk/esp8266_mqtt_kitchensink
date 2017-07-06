function logCatch(error){
  console.log(error);
  //throw error;
}

function getUrl(url){
  //console.log("getUrl(" + url + ")");

	return new Promise(function(resolve, reject) {
    // Do the usual XHR stuff
    var req = new XMLHttpRequest();
    req.open('GET', url);

    req.onload = function() {
      if(Math.random() > 0.3){
        reject(Error("Simulated Network Error"));
      }
      // This is called even on 404 etc
      // so check the status
      if (req.status == 200) {
        // Resolve the promise with the response text
        resolve(req.response);
      }
      else {
        // Otherwise reject with the status text
        // which will hopefully be a meaningful error
        reject(Error(req.statusText));
      }
    };

    // Handle network errors
    req.onerror = function() {
      reject(Error("Network Error"));
    };

    // Make the request
    req.send();
  });
}

function rejectDelay(delay, payload) {
  //console.log("rejectDelay(", delay, ")");

  return new Promise(function(resolve, reject) {
    setTimeout(reject.bind(null, payload), delay); 
  });
}

function getUrlRetry(url, retries, delay){
  console.log("getUrlRetry(" + url + ", " + retries + ", " + delay + ")");

  let p = Promise.reject();
  for(var i=0; i < retries; i++) {
    p = p.catch(getUrl.bind(null, url))
         .catch(function(payload){
             console.log(url, " : ", payload.message);
             return Promise.reject(payload);
          })
         .catch(rejectDelay.bind(null, delay));
  }
  return p;
}

function processTemplate(url, template_content){
  //console.log("loaded: " + template_url + " " + template_content.length);
  console.log(url, " : loaded: " + template_content.length);
}

function getTemplates(){
	getUrlRetry("filenames.sys", 10, 1000).then(function(filenames){
    let filenames_array = filenames.match(/[^\r\n]+/g);
    let promise_list = [];
    for(template_url of filenames_array){
      if(template_url.endsWith(".mustache")){
        promise_list.push(Promise.resolve().then(getUrlRetry.bind(null, template_url, 5, 2000))
                        .then(processTemplate.bind(null, template_url))
                        .catch(logCatch));
      }
    };
    return Promise.all(promise_list);
  }).then(function() {
    // And we're all done!
    console.log("Finished loading templates.");
  }).catch(function(err) {
    // Catch any error that happened along the way
    console.log("Problem loading templates: " + err.message);
  });
}


window.addEventListener("load", function(){
  getTemplates();
  console.log("done");
}, false);
