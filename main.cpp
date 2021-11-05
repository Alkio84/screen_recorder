#include <stdio.h>
#include <iostream>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
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
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif

//Output YUV420P
#define OUTPUT_YUV420P 0
//'1' Use Dshow
//'0' Use GDIgrab
#define USE_DSHOW 0

//Refresh Event
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

using namespace std;

#include "ScreenRecorder.h"

/* driver function to run the application */
 int main() {
     ScreenRecorder screen_record;
 
     if(screen_record.openCamera() != 0) return -1;
     std::cout<<"opened camera"<<std::endl;
     if(screen_record.init_outputfile() != 0) return -1;
    std::cout<<"input file created"<<std::endl;
     if(screen_record.CaptureVideoFrames() != 0) return -1;
     cout<<"\nprogram executed successfully\n";
 
     return 0;
 }
