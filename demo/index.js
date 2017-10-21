var express = require('express');
var app = express();
var expressWs = require('express-ws')(app);
var bodyParser = require('body-parser')
var ff = require('../');

app.use( bodyParser.json());
app.use((req,res,next)=>{
  return next();
});
app.ws('/stream',(ws,wq)=>{
  console.log('connection');

  (function(ws){ff.ff_init('hd1080',30,10000000,256000,(chunk)=>{
    if(ws.readyState === 1)
      ws.send(chunk);
    })})(ws);
  ff.start_loop();
   ws.on('close',(msg)=>{
    ff.stop_loop();
    ff.ff_quit();
    console.log('disconnect'); 
  });
  ws.on('error',(msg)=>{
    ff.stop_loop();
    ff.ff_quit();
    console.log('error'); 
  });
});
app.listen(3000);


