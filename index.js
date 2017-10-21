const ffi = require('ffi');
const ref = require('ref');
const os = require('os');

const libff = ffi.Library(__dirname + '/libff.so',{
  'ff_init': ['void',['string','string', 'string' , 'string' , 'string', 'int', 'int','int','pointer']],
  'ff_quit': ['void',[]],
  'update_video' : ['int',[]],
  'update_audio' : ['int',[]],
  'ff_compare' : ['int',[]],
  'start_loop' : ['void',[]],
  'wait_loop' : ['void',[]],
  'stop_loop' : ['void',[]]
}
);

var Callbacks = {sendPacket:null};
module.exports.ff_init = (video_size,fps, video_rate, audio_rate, sendPacket)=>{
  var ret = 1;
  Callbacks.sendPacket = ffi.Callback('int',['pointer','int'],(dat,len)=>{ sendPacket(ref.reinterpret(dat,len,0)); });
  if(os.type == 'Linux')
    libff.ff_init('x11grab', ':0.0' , 'pulse', 'default' , video_size,fps,video_rate,audio_rate, Callbacks.sendPacket);
  else if(os.type == 'Windows_NT')
    libff.ff_init('gdigrab', ':desktop' , 'openal', '' , video_size,fps,video_rate,audio_rate, Callbacks.sendPacket);
  else{
    console.log('Not Suport Platform: ' + os.type);
    ret = 0;
  }
  return ret;
}

module.exports.ff_quit = ()=>{
  libff.ff_quit();
}

module.exports.update_video = ()=>{
  libff.update_video();
}
  
module.exports.update_audio = ()=>{
  libff.update_audio();
}

module.exports.ff_compare= ()=>{
  return libff.ff_compare();
}

module.exports.start_loop= ()=>{
  return libff.start_loop();
}

module.exports.stop_loop= ()=>{
  return libff.stop_loop();
}

module.exports.wait_loop= ()=>{
  return libff.wait_loop();
}
process.on('exit',()=>{
  Callbacks;
});
