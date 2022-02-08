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

using namespace std;

#include "ScreenRecorder.h"
#include <chrono>
#include <thread>
/* driver function to run the application */
int main() {
    ScreenRecorder screenRecorder{};

    screenRecorder.setOutputFile("../Recordings/output.mp4");
    //screenRecorder.setResolution(ScreenRecorder::ORIGINAL);
    screenRecorder.setFrameRate(15);
    screenRecorder.recordAudio(true);
    //screenRecorder.setViewPortFromCorners1(std::pair<int, int>(100,100), std::pair<int, int>(900,900));

    screenRecorder.Start();
    std::cout<<"started"<<std::endl;
    std::this_thread::sleep_for(3s);

    screenRecorder.Pause();
    std::cout<<"paused"<<std::endl;
    std::this_thread::sleep_for(2s);

    screenRecorder.Resume();
    std::cout<<"resumed"<<std::endl;
    std::this_thread::sleep_for(3s);

    screenRecorder.Stop();
    std::cout<<"stopped"<<std::endl;

    return 0;
};