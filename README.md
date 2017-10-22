# ff-live
## 基于FFmpeg，nodejs , websocket , MSE 的web直播程序
目前仍然在初期开发阶段。

## Build

1. Use packege manager（apt/yum...) to install ffmpeg library or compile from source with libx264 ,libpulse(linux)/openal(windows) support.
2. `npm install` 
3. `cd src`
4. `make libff.so && make install`(linux) or `make libff.dll && make win_install`(windows)


## Usage
```
var ff = require('ff-live');

//register parameter and callback for output 
//( video_size , framerate , video_bitrate, audio_bitrate, sendPacketCallback)
ff.ff_init( 'hd1080', 30 , 1000000 , 128000, (chunk) =>{
  //deal with the fragment mp4 segment.
  ...
});

//start capture and call callback when encoded one packet , this function is unblock
ff.start_loop();

//stop  capture,  block until finish
ff.stop_loop();

//free memory 
ff.quit();

```

## Demo usage
1. `cd demo`
2. `npm install`
3. `node index.js &`
4. `npm run web`
5. open http://localhost:8080/ in browser
6. click connect or disconnect 

## Demo preview
### linux 
 ![linux](https://github.com/moeyard/ff-live/raw/master/demo/img/linux.png)
### windows 
![windows](https://github.com/moeyard/ff-live/raw/master/demo/img/windows.png)
