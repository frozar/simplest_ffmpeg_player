/**
 * 最简单的基于FFmpeg的视频播放器 2
 * Simplest FFmpeg Player 2
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * 第2版使用SDL2.0取代了第一版中的SDL1.2
 * Version 2 use SDL 2.0 instead of SDL 1.2 in version 1.
 *
 * 本程序实现了视频文件的解码和显示(支持HEVC，H.264，MPEG2等)。
 * 是最简单的FFmpeg视频解码方面的教程。
 * 通过学习本例子可以了解FFmpeg的解码流程。
 * This software is a simplest video player based on FFmpeg.
 * Suitable for beginner of FFmpeg.
 *
 */



#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL2/SDL.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif

#define PRINT_DBG( n ) fprintf( stderr, "PASS: %d\n", n ) 

//Output YUV420P data as a file 
#define OUTPUT_YUV420P 0

int main(int argc, char* argv[])
{
  AVFormatContext *pFormatCtx;
  int             videoindex;
  AVCodecContext  *pCodecCtx;
  AVCodec         *pCodec;
  AVFrame         *pFrame,*pFrameYUV;
  uint8_t         *out_buffer;
  AVPacket        packet;

  int y_size;
  int ret, got_picture;

  struct SwsContext *img_convert_ctx;

  char filepath[]="bigbuckbunny_480x272.h265";
  //SDL---------------------------
  int          screen_w=0,screen_h=0;
  SDL_Window   *screen; 
  SDL_Renderer *sdlRenderer;
  SDL_Texture  *sdlTexture;
  SDL_Rect     sdlRect;

  FILE *fp_yuv;
  
  // Used if OUTPUT_YUV420P is different to 0
  ( void ) y_size ;
  ( void ) fp_yuv ;

  av_register_all();

  // ### 0 - Protocol Data : file
  avformat_network_init();
  pFormatCtx = avformat_alloc_context();

  if(avformat_open_input(&pFormatCtx,filepath,NULL,NULL)!=0){
    printf("Couldn't open input stream.\n");
    return -1;
  }

  // ### 1 - Container Format Data : raw HEVC video
  // fprintf( stderr, "pFormatCtx->iformat->name: %s\n", pFormatCtx->iformat->name ) ;
  // fprintf( stderr, "pFormatCtx->iformat->long_name: %s\n", pFormatCtx->iformat->long_name ) ;
  // fprintf( stderr, "pFormatCtx->iformat->extensions: %s\n", pFormatCtx->iformat->extensions ) ;
  // fprintf( stderr, "pFormatCtx->iformat->mime_type: %s\n", pFormatCtx->iformat->mime_type ) ;
  
  if(avformat_find_stream_info(pFormatCtx,NULL)<0){
    printf("Couldn't find stream information.\n");
    return -1;
  }

  // ### 2 - Audio/Video btstream data : hevc
  videoindex=-1;
  for(uint i=0; i<pFormatCtx->nb_streams; i++) {
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
      videoindex=i;
      break;
    }
  }
  if(videoindex==-1){
    printf("Didn't find a video stream.\n");
    return -1;
  }

  pCodecCtx = pFormatCtx->streams[videoindex]->codec;
  pCodec    = avcodec_find_decoder(pCodecCtx->codec_id);
  if(pCodec==NULL){
    printf("Codec not found.\n");
    return -1;
  }

  // fprintf( stderr, "pCodec->name: %s\n", pCodec->name ) ;
  // fprintf( stderr, "pCodec->long_name: %s\n", pCodec->name ) ;

  if(avcodec_open2( pCodecCtx, pCodec, NULL ) < 0 ){
    printf("Could not open codec.\n");
    return -1;
  }

  // ### 3 - Audio/Video raw data : AV_PIX_FMT_YUV420P
  fprintf( stderr, "input  pixel format: pCodecCtx->pix_fmt: %d\n", pCodecCtx->pix_fmt ) ;
  fprintf( stderr, "output pixel format: AV_PIX_FMT_YUV420P: %d\n", AV_PIX_FMT_YUV420P ) ;

  // ### 4 - Allocate data to store a single picture
  AVPixelFormat output_format_choice = AV_PIX_FMT_YUV420P ;
  int           interpolation_choice = SWS_BICUBIC ;
  int           width_choice         = pCodecCtx->width * 2 ;
  int           height_choice        = pCodecCtx->height * 2 ;
  
  pFrame     = av_frame_alloc();
  pFrameYUV  = av_frame_alloc();
  out_buffer = ( uint8_t * )av_malloc( av_image_get_buffer_size( output_format_choice,
						    width_choice,
						    height_choice,
						    1 ) ) ;
  av_image_fill_arrays( pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		        output_format_choice, width_choice, height_choice,
			1 ) ;
       
  //Output Info-----------------------------
  printf("--------------- File Information ----------------\n");
  av_dump_format( pFormatCtx, 0, filepath, 0 ) ;
  printf("-------------------------------------------------\n");

  img_convert_ctx = sws_getContext( pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, 
				    width_choice, height_choice,
				    output_format_choice, interpolation_choice,
				    NULL, NULL, NULL ) ;

#if OUTPUT_YUV420P 
  fp_yuv=fopen("output.yuv","wb+");  
#endif  


  // ALLOCATE SDL stuff and initialisation
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
    printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
    return -1;
  } 

  screen_w = width_choice ;
  screen_h = height_choice ;
  //SDL 2.0 Support for multiple windows
  screen = SDL_CreateWindow("Simplest ffmpeg player's Window",
			    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			    screen_w, screen_h,
			    SDL_WINDOW_OPENGL);

  if(!screen) {  
    printf("SDL: could not create window - exiting:%s\n",SDL_GetError());  
    return -1;
  }

  sdlRenderer = SDL_CreateRenderer( screen, -1, 0 ) ;
  //IYUV: Y + U + V  (3 planes)
  //YV12: Y + V + U  (3 planes)
  sdlTexture = SDL_CreateTexture( sdlRenderer, SDL_PIXELFORMAT_IYUV,
				  SDL_TEXTUREACCESS_STREAMING,
				  width_choice, height_choice ) ;

  sdlRect.x = 0 ;
  sdlRect.y = 0 ;
  sdlRect.w = screen_w ;
  sdlRect.h = screen_h ;

  //SDL End----------------------
  while( av_read_frame( pFormatCtx, &packet ) >= 0 ) {

    if( packet.stream_index != videoindex ) {
      continue ;
    }

    if ( avcodec_decode_video2( pCodecCtx, pFrame, &got_picture, &packet ) < 0 ) {
      printf("Decode Error.\n");
      return -1;
    }

    if ( !got_picture ) {
      continue ;
    }
    
    sws_scale( img_convert_ctx,
	       pFrame->data, pFrame->linesize, 0, pCodecCtx->height, 
	       pFrameYUV->data, pFrameYUV->linesize ) ;

    // ref in pFrame is no longer needed
    av_frame_unref( pFrame ) ;
    
#if OUTPUT_YUV420P
    y_size = pCodecCtx->width * pCodecCtx->height ;  
    fwrite(pFrameYUV->data[0],1,y_size,fp_yuv);    //Y 
    fwrite(pFrameYUV->data[1],1,y_size/4,fp_yuv);  //U
    fwrite(pFrameYUV->data[2],1,y_size/4,fp_yuv);  //V
#endif
      
    //SDL---------------------------
#if 0
    SDL_UpdateTexture( sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] ) ;
#else
    SDL_UpdateYUVTexture( sdlTexture, &sdlRect,
			  pFrameYUV->data[0], pFrameYUV->linesize[0],
			  pFrameYUV->data[1], pFrameYUV->linesize[1],
			  pFrameYUV->data[2], pFrameYUV->linesize[2] ) ;
#endif
    SDL_RenderClear( sdlRenderer ) ;
    SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, &sdlRect ) ;
    SDL_RenderPresent( sdlRenderer ) ;
    //SDL End-----------------------
      
    //Delay 40ms
    SDL_Delay( 40 ) ;

    av_free_packet( &packet ) ;

  }  // END - while( av_read_frame( pFormatCtx, packet ) >= 0 ) {
  
  // flush decoder
  // FIX: Flush Frames remained in Codec
  for ( ; ; ) {

    if ( avcodec_decode_video2( pCodecCtx, pFrame, &got_picture, &packet ) < 0 ) {
      printf( "Decode Error.\n" ) ;
      return -1 ;
    }

    if ( !got_picture ) {
      break ;
    }
    
    sws_scale( img_convert_ctx,
	       pFrame->data, pFrame->linesize, 0, pCodecCtx->height, 
	       pFrameYUV->data, pFrameYUV->linesize ) ;

#if OUTPUT_YUV420P
    y_size = pCodecCtx->width * pCodecCtx->height ;  
    fwrite(pFrameYUV->data[0],1,y_size,fp_yuv);    //Y 
    fwrite(pFrameYUV->data[1],1,y_size/4,fp_yuv);  //U
    fwrite(pFrameYUV->data[2],1,y_size/4,fp_yuv);  //V
#endif

    //SDL---------------------------
#if 0
    SDL_UpdateTexture( sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );  
#else
    SDL_UpdateYUVTexture( sdlTexture, &sdlRect,
			  pFrameYUV->data[0], pFrameYUV->linesize[0],
			  pFrameYUV->data[1], pFrameYUV->linesize[1],
			  pFrameYUV->data[2], pFrameYUV->linesize[2] ) ;
#endif
    SDL_RenderClear( sdlRenderer ) ;
    SDL_RenderCopy( sdlRenderer, sdlTexture,  NULL, &sdlRect ) ;
    SDL_RenderPresent( sdlRenderer ) ;
    //SDL End-----------------------

    //Delay 40ms
    SDL_Delay( 40 ) ;
  }

  SDL_DestroyWindow( screen ) ;
  SDL_DestroyTexture( sdlTexture ) ;
  SDL_DestroyRenderer( sdlRenderer ) ;
  SDL_Quit( ) ;

  av_free_packet( &packet ) ;
  sws_freeContext( img_convert_ctx ) ;

#if OUTPUT_YUV420P 
  fclose(fp_yuv);
#endif 

  av_frame_free( &pFrameYUV ) ;
  av_frame_free( &pFrame ) ;

  av_free( out_buffer ) ;
  
  avcodec_close( pCodecCtx ) ;
  
  avformat_close_input( &pFormatCtx ) ;
  avformat_free_context( pFormatCtx ) ;
  
  return 0;
}

