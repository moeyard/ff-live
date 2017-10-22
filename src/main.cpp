#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <thread>
#include <chrono>
#include <iostream>
#include <ctime>
#include <mutex>
extern "C"{
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>

#define IO_SIZE 16*1024*1024

typedef int (*sendPacketCallback)(void * dat, int len);

typedef struct {
  AVStream        * pStream[2];
  AVFormatContext * pOutputFormatCtx;
  AVCodec         * pOutputCodec[2];
  AVCodecContext  * pOutputCodecCtx[2];
  
  AVFormatContext * pInputFormatCtx[2];
  AVCodec         * pInputCodec[2];
  AVCodecContext  * pInputCodecCtx[2];
  
  SwrContext      *pSwrCtx;
  struct SwsContext *pSwsCtx;
  AVDictionary    *in_dict[2];
  AVDictionary    *out_dict;
  
  int64_t vpts;
  int64_t apts;
  int64_t base;
  sendPacketCallback sendPacket;
} AVCtx;

AVCtx *pAVCtx;
AVPacket * pPacket[2];
AVFrame  * pFrame[2];
char buf[4*1024*4];
char tmp[4*1024*4];
int buf_nb=0;
int stop = 0;
std::thread * loop[2];
std::mutex mtx;



void ff_open_video(const char * fmt , const char * url,  const char * video_size, const char * framerate){
  int io_size = IO_SIZE;
  int ret ;
  pAVCtx->pInputFormatCtx[0] = avformat_alloc_context();
  av_dict_set(&pAVCtx->in_dict[0], "video_size", video_size, 0);
  av_dict_set(&pAVCtx->in_dict[0], "framerate", framerate, 0);
  ret = avformat_open_input(&pAVCtx->pInputFormatCtx[0],url,av_find_input_format(fmt),&pAVCtx->in_dict[0]);
  if(ret != 0) printf("Video ERROR\n");
  pAVCtx->pInputCodec[0] = avcodec_find_decoder(pAVCtx->pInputFormatCtx[0]->streams[0]->codecpar->codec_id);
  pAVCtx->pInputCodecCtx[0] =  avcodec_alloc_context3(pAVCtx->pInputCodec[0]);
  avcodec_parameters_to_context(pAVCtx->pInputCodecCtx[0], pAVCtx->pInputFormatCtx[0]->streams[0]->codecpar);
  avcodec_open2(pAVCtx->pInputCodecCtx[0], pAVCtx->pInputCodec[0], NULL);
  //av_dump_format(pAVCtx->pInputFormatCtx[0],0,NULL,0);
  return ;
}

void ff_open_audio(const char * fmt, const char * url){
  int io_size = IO_SIZE;
  int ret ;
  pAVCtx->pInputFormatCtx[1] = avformat_alloc_context();
  //for pcm_s16le
  av_dict_set(&pAVCtx->in_dict[1], "sample_rate", "48000", 0);
  av_dict_set(&pAVCtx->in_dict[1], "channels", "2", 0);
  //read first header packet
  ret = avformat_open_input(&pAVCtx->pInputFormatCtx[1],url,av_find_input_format(fmt),&pAVCtx->in_dict[1]);
  if(ret != 0) printf("Audio ERROR\n");
  pAVCtx->pInputCodec[1] = avcodec_find_decoder(pAVCtx->pInputFormatCtx[1]->streams[0]->codecpar->codec_id);
  pAVCtx->pInputCodecCtx[1] =  avcodec_alloc_context3(pAVCtx->pInputCodec[1]);
  avcodec_parameters_to_context(pAVCtx->pInputCodecCtx[1], pAVCtx->pInputFormatCtx[1]->streams[0]->codecpar);
  avcodec_open2(pAVCtx->pInputCodecCtx[1], pAVCtx->pInputCodec[1], NULL);
  //av_dump_format(pAVCtx->pInputFormatCtx[1],0,NULL,0);
  return ;
}


static int write_packet(void *opaque, uint8_t *buf, int buf_size){
  AVCtx * pAVCtx = (AVCtx*)opaque;
  return pAVCtx->sendPacket(buf,buf_size);
}

void ff_init(const char * video_fmt, const char * video_url , const char * audio_fmt, const char * audio_url , const char * video_size, int fps, int video_bitrate, int audio_bitrate, sendPacketCallback sendPacket){
  int ret;
  int io_size = IO_SIZE;
  int width; 
  int height;
  char framerate[128];
  if(!strcmp(video_size , "hd1080")){
    width = 1920;
    height = 1080;
  }else if(!strcmp(video_size , "hd720")){
    width = 1280;
    height = 720;
  }else {
    av_log(NULL,AV_LOG_ERROR,"Video size not support!\n");
    exit(1);
  }
  sprintf(framerate, "%d", fps); 

  av_register_all();
  avdevice_register_all() ;
  pAVCtx = (AVCtx*)malloc(sizeof(AVCtx));
  pAVCtx->vpts = 0;
  pAVCtx->apts = 0;
  pAVCtx->sendPacket = sendPacket;
  pAVCtx->in_dict[0] = NULL;
  pAVCtx->in_dict[1] = NULL;
  pAVCtx->out_dict = NULL;
  stop = 0;

  pPacket[0] = av_packet_alloc();
  av_init_packet(pPacket[0]);
  pPacket[0]->data= NULL;
  pPacket[0]->size =0;
  pFrame[0] = av_frame_alloc();

  pPacket[1] = av_packet_alloc();
  av_init_packet(pPacket[1]);
  pPacket[1]->data= NULL;
  pPacket[1]->size =0;
  pFrame[1] = av_frame_alloc();

  av_dict_set(&pAVCtx->out_dict, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
  av_dict_set(&pAVCtx->out_dict, "g", "30", 0);
  //video output -> h264
  pAVCtx->pOutputCodec[0] = avcodec_find_encoder(AV_CODEC_ID_H264);
  pAVCtx->pOutputCodecCtx[0] = avcodec_alloc_context3(pAVCtx->pOutputCodec[0]);
  pAVCtx->pOutputCodecCtx[0]->pix_fmt  = AV_PIX_FMT_YUV420P ;
  //pAVCtx->pOutputCodecCtx[0]->time_base= av_make_q(1,fps);
  pAVCtx->pOutputCodecCtx[0]->time_base= av_make_q(1,1e6);  //microsecond
  pAVCtx->pOutputCodecCtx[0]->framerate= av_make_q(fps,1);
  pAVCtx->pOutputCodecCtx[0]->bit_rate = video_bitrate ;
  pAVCtx->pOutputCodecCtx[0]->codec_id = AV_CODEC_ID_H264;
  pAVCtx->pOutputCodecCtx[0]->codec    = pAVCtx->pOutputCodec[0];
  pAVCtx->pOutputCodecCtx[0]->width    = width;
  pAVCtx->pOutputCodecCtx[0]->height   = height;
  pAVCtx->pOutputCodecCtx[0]->profile  = 110;  //High 10
  pAVCtx->pOutputCodecCtx[0]->level    = 31;   //Level 31
  pAVCtx->pOutputCodecCtx[0]->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  ret = avcodec_open2(pAVCtx->pOutputCodecCtx[0],NULL,&pAVCtx->out_dict);


  //rgb0->yuv420p
  pAVCtx->pSwsCtx = sws_getContext(
  width,
  height,
        AV_PIX_FMT_RGB32,
        pAVCtx->pOutputCodecCtx[0]->width,
        pAVCtx->pOutputCodecCtx[0]->height,
        pAVCtx->pOutputCodecCtx[0]->pix_fmt,
        //AV_PIX_FMT_RGB24,
        //AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR,
        NULL,
        NULL,
        NULL
    );
  //Audio output -> aac
  pAVCtx->pOutputCodec[1] = avcodec_find_encoder(AV_CODEC_ID_AAC);
  pAVCtx->pOutputCodecCtx[1] = avcodec_alloc_context3(pAVCtx->pOutputCodec[1]);
  pAVCtx->pOutputCodecCtx[1]->sample_fmt = AV_SAMPLE_FMT_FLTP;
  pAVCtx->pOutputCodecCtx[1]->sample_rate = 48000;
  pAVCtx->pOutputCodecCtx[1]->bit_rate  = audio_bitrate;
  //pAVCtx->pOutputCodecCtx[1]->time_base = av_make_q(1,48000);
  pAVCtx->pOutputCodecCtx[1]->time_base = av_make_q(1,1e6);
  pAVCtx->pOutputCodecCtx[1]->channel_layout = AV_CH_LAYOUT_STEREO;
  pAVCtx->pOutputCodecCtx[1]->channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
  pAVCtx->pOutputCodecCtx[1]->codec_id = AV_CODEC_ID_AAC;
  pAVCtx->pOutputCodecCtx[1]->codec = pAVCtx->pOutputCodec[1];
  //aac only support FLTP
  pAVCtx->pSwrCtx = swr_alloc_set_opts(
    NULL ,  
    AV_CH_LAYOUT_STEREO,   AV_SAMPLE_FMT_FLTP,  48000,  
    AV_CH_LAYOUT_STEREO,  AV_SAMPLE_FMT_S16,  48000,                
    0, NULL  );                
  swr_init(pAVCtx->pSwrCtx) ;
  avcodec_open2(pAVCtx->pOutputCodecCtx[1],NULL,NULL);


  //Output format -> fragment mp4
  pAVCtx->pOutputFormatCtx=avformat_alloc_context();
  ret = avformat_alloc_output_context2(&pAVCtx->pOutputFormatCtx,NULL,"mp4",NULL);

  pAVCtx->pOutputFormatCtx->oformat->flags |= AVFMT_NOFILE;
  //Stream 0 -> video 
  pAVCtx->pStream[0] = avformat_new_stream(pAVCtx->pOutputFormatCtx,pAVCtx->pOutputCodec[0]);
  pAVCtx->pStream[0]->codec = pAVCtx->pOutputCodecCtx[0];
  ret = avcodec_parameters_from_context( pAVCtx->pStream[0]->codecpar, pAVCtx->pOutputCodecCtx[0]);

  //Stream 1 -> audio 
  pAVCtx->pStream[1] = avformat_new_stream(pAVCtx->pOutputFormatCtx,pAVCtx->pOutputCodec[1]);
  pAVCtx->pStream[1]->codec = pAVCtx->pOutputCodecCtx[1];
  avcodec_parameters_from_context( pAVCtx->pStream[1]->codecpar, pAVCtx->pOutputCodecCtx[1]);
  //av_dump_format(pAVCtx->pOutputFormatCtx,0,NULL,1);
  pAVCtx->pOutputFormatCtx->pb =avio_alloc_context((unsigned char *)av_malloc(io_size),io_size, 1 , pAVCtx, NULL , &write_packet, NULL);
  ret  = avformat_write_header(pAVCtx->pOutputFormatCtx,&pAVCtx->out_dict);
  avio_flush(pAVCtx->pOutputFormatCtx->pb);
  ff_open_video(video_fmt,video_url,video_size,framerate);
  ff_open_audio(audio_fmt , audio_url);
  pAVCtx->base = av_gettime();
}

void ff_close(){
  av_write_trailer(pAVCtx->pOutputFormatCtx);
  avio_flush(pAVCtx->pOutputFormatCtx->pb);
}

void ff_quit(){
  ff_close();
  av_frame_unref(pFrame[0]);
  av_frame_unref(pFrame[1]);
  av_packet_unref(pPacket[0]);
  av_packet_unref(pPacket[1]);
  swr_free(&pAVCtx->pSwrCtx);
  sws_freeContext(pAVCtx->pSwsCtx);
  
  avcodec_close(pAVCtx->pInputCodecCtx[0]);
  avcodec_close(pAVCtx->pInputCodecCtx[1]);
  avcodec_close(pAVCtx->pOutputCodecCtx[0]);
  avcodec_close(pAVCtx->pOutputCodecCtx[1]);
  avformat_close_input(&pAVCtx->pInputFormatCtx[0]);
  avformat_close_input(&pAVCtx->pInputFormatCtx[1]);
  avio_context_free(&pAVCtx->pOutputFormatCtx->pb);
  avformat_free_context(pAVCtx->pOutputFormatCtx);
  free(pAVCtx);
  pAVCtx=NULL;
  pFrame[0] =NULL;
  pFrame[1] =NULL;
  pPacket[0] =NULL;
  pPacket[1] =NULL;
  buf_nb = 0;
  return ;
}


int  ff_demux_video(){
  int ret = av_read_frame(pAVCtx->pInputFormatCtx[0],pPacket[0]); 
  if(ret == 0) {
      pAVCtx->vpts = pPacket[0]->pts - pAVCtx->base;
  }
  return ret;
}

int  ff_demux_audio(){
  static int first = 1;
  static int64_t ts = 0;
  int ret = av_read_frame(pAVCtx->pInputFormatCtx[1],pPacket[1]); 
  if(ret == 0) {
      pAVCtx->apts = pPacket[1]->pts - pAVCtx->base;
  }

  return ret;
}


int ff_decode_video(){
  int ret;
  do{
    ret = avcodec_send_packet(pAVCtx->pInputCodecCtx[0], pPacket[0]);
    if (ret == 0) {
      ret = avcodec_receive_frame(pAVCtx->pInputCodecCtx[0],pFrame[0]);
    }
  }while(ret == AVERROR(EAGAIN));
  return ret;
}

int ff_decode_audio(){
  int ret;
  do{
    ret = avcodec_send_packet(pAVCtx->pInputCodecCtx[1], pPacket[1]);
    if (ret == 0) {
      ret = avcodec_receive_frame(pAVCtx->pInputCodecCtx[1],pFrame[1]);
    }
  }while(ret == AVERROR(EAGAIN));
  return ret;
}

int ff_encode_video(){
  int ret;
  do{
   pFrame[0]->pts = pAVCtx->vpts; //avoid warning
   ret = avcodec_send_frame(pAVCtx->pOutputCodecCtx[0],pFrame[0]);
   if(ret == 0) {
     pFrame[0]->pts = pAVCtx->vpts;
     ret = avcodec_receive_packet(pAVCtx->pOutputCodecCtx[0],pPacket[0]);
     if(ret == 0){
       pPacket[0]->stream_index = pAVCtx->pStream[0]->index;
       pPacket[0]->pts = pAVCtx->apts;
       pPacket[0]->dts = pPacket[0]->pts;
       av_packet_rescale_ts(pPacket[0], pAVCtx->pOutputCodecCtx[0]->time_base, pAVCtx->pStream[0]->time_base);
     }
   }
 }while(ret == AVERROR(EAGAIN));
 return ret;
}

int ff_encode_audio(){
  int ret;
  do{
   pFrame[1]->pts = pAVCtx->apts; //avoid warning
   ret = avcodec_send_frame(pAVCtx->pOutputCodecCtx[1],pFrame[1]);
   if(ret == 0) {
     pFrame[1]->pts = pAVCtx->apts;
     ret = avcodec_receive_packet(pAVCtx->pOutputCodecCtx[1],pPacket[1]);
     if(ret == 0){
       pPacket[1]->stream_index = pAVCtx->pStream[1]->index;
       pPacket[1]->pts = pAVCtx->apts+ 2100000;
       pPacket[1]->dts = pPacket[1]->pts;
       av_packet_rescale_ts(pPacket[1], pAVCtx->pOutputCodecCtx[1]->time_base, pAVCtx->pStream[1]->time_base);
     }
   }
 }while(ret == AVERROR(EAGAIN));
 return ret;
}



int ff_mux_video(){
  int ret;
  std::lock_guard<std::mutex> lock(mtx);
  ret = av_interleaved_write_frame(pAVCtx->pOutputFormatCtx,pPacket[0]);
  avio_flush(pAVCtx->pOutputFormatCtx->pb);
  return ret;
}

int ff_mux_audio(){
  int ret;
  std::lock_guard<std::mutex> lock(mtx);
  ret = av_interleaved_write_frame(pAVCtx->pOutputFormatCtx,pPacket[1]);
  avio_flush(pAVCtx->pOutputFormatCtx->pb);
  return ret;
}



int ff_resample(){
  AVFrame *  pSwrFrame=NULL;
  int nb_samples=0;
  //resample S16P->FLTP
  if(pFrame[1]->nb_samples > sizeof(buf)/4){
  //if(pFrame[1]->nb_samples > 1024){
    av_frame_unref(pFrame[1]);
    return -1;
  }
  if((buf_nb + pFrame[1]->nb_samples) > sizeof(buf)/4){
    buf_nb = sizeof(buf)/4 - pFrame[1]->nb_samples;
  }
  if(buf_nb){
    memcpy(tmp,buf, buf_nb*4);
    memcpy(tmp+buf_nb*4, pFrame[1]->data[0], pFrame[1]->nb_samples*4);
    buf_nb += pFrame[1]->nb_samples;
  }else{
    memcpy(tmp,pFrame[1]->data[0],pFrame[1]->nb_samples*4);
    buf_nb = pFrame[1]->nb_samples;
  }

  if(buf_nb  > 1024){
      memcpy(buf,tmp+4096, buf_nb*4 -4096);
      nb_samples = 1024;
      buf_nb  -= 1024;
  }else{
      memcpy(buf,tmp, buf_nb*4);
      av_frame_unref(pFrame[1]);
      return -1;
  }
  void * ptmp = tmp;
  size_t buf_size = av_get_bytes_per_sample(pAVCtx->pOutputCodecCtx[1]->sample_fmt) * swr_get_out_samples(pAVCtx->pSwrCtx,nb_samples);
  pSwrFrame = av_frame_alloc();
  pSwrFrame->data[0] = (uint8_t *)av_malloc(buf_size);
  pSwrFrame->data[1] = (uint8_t *)av_malloc(buf_size);
  pSwrFrame->nb_samples = swr_convert(pAVCtx->pSwrCtx, (uint8_t**)pSwrFrame->data,nb_samples ,(const uint8_t**)&ptmp, nb_samples);
  pSwrFrame->pts = pAVCtx->apts;
  av_frame_unref(pFrame[1]);
  pFrame[1] = pSwrFrame;
  return 0;
}


int ff_scale(){
  AVFrame * pSwsFrame =NULL;
  pSwsFrame =  av_frame_alloc();
  pSwsFrame->width =  pAVCtx->pOutputCodecCtx[0]->width;
  pSwsFrame->height = pAVCtx->pOutputCodecCtx[0]->height;
  pSwsFrame->format = pAVCtx->pOutputCodecCtx[0]->pix_fmt;

  av_image_alloc(pSwsFrame->data, pSwsFrame->linesize, pSwsFrame->width, pSwsFrame->height, (AVPixelFormat)pSwsFrame->format,4);

  sws_scale(
    pAVCtx->pSwsCtx,
    (uint8_t const * const *)pFrame[0]->data,
    pFrame[0]->linesize,
    0,
    pSwsFrame->height,
    pSwsFrame->data,
    pSwsFrame->linesize
  ); 
  av_frame_unref(pFrame[0]);

  pFrame[0] = pSwsFrame;


  return 0;
}



int update_video(){
  FILE * fd=NULL;
  char path[128];
  int ret;
  ff_demux_video();
  ff_decode_video();
  av_packet_unref(pPacket[0]);
  ff_scale();
  ff_encode_video();
  av_freep(&pFrame[0]->data[0]);
  av_frame_unref(pFrame[0]);
  ff_mux_video();
  av_packet_unref(pPacket[0]);
  return 0;
}



int update_audio(){
  FILE * fd=NULL;
  char path[128];
  int ret;
  int len = 0;
  void * tmp_dat;
  int tmp_len=0;
  do{
    ff_demux_audio();
    ff_decode_audio();
    av_packet_unref(pPacket[1]);
  }while(ff_resample() <0);
  ff_encode_audio();
  av_free(pFrame[1]->data[0]);
  av_free(pFrame[1]->data[1]);
  av_frame_unref(pFrame[1]);
  ff_mux_audio();
  av_packet_unref(pPacket[1]);
  return 0;
}

int ff_compare(){
  return (av_compare_ts(pAVCtx->vpts, pAVCtx->pOutputCodecCtx[0]->time_base , pAVCtx->apts, pAVCtx->pOutputCodecCtx[1]->time_base)<=0);
}




void start_loop(){
  loop[0] = new std::thread([](){
    while(!stop){
      update_video();
    }
  }); 

  loop[1] = new std::thread([](){
    while(!stop){
      auto t_start = std::chrono::high_resolution_clock::now();       
      update_audio();
      auto t_end   = std::chrono::high_resolution_clock::now();       
    }
  }); 
}

void stop_loop(){
  stop = 1;
  loop[0]->join();
  loop[1]->join();
  stop = 0;
  delete loop[0];
  delete loop[1];
}

void wait_loop(){
  loop[0]->join();
  loop[1]->join();
  delete loop[0];
  delete loop[1];
}

}



