//
//  ScreenRecorder.hpp
//  ScreenRecorder
//
//  Created by Checco on 15/10/2021.
//  Copyright © 2021 Checco. All rights reserved.
//

#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <math.h>
#include <string.h>

#define __STDC_CONSTANT_MACROS

//FFMPEG LIBRARIES
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"

#include "libavdevice/avdevice.h"

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"

// libav resample

#include "libavutil/opt.h"
#include "libavutil/common.h"
#include "libavutil/channel_layout.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/file.h"

// lib swresample

#include "libswscale/swscale.h"

}

class ScreenRecorder
{
private:
    AVInputFormat *inputFormat;
    AVOutputFormat *outputFormat;

    AVCodecContext *decoderContext;
    AVCodecContext *encoderContext;

    AVFormatContext *inputFormatContext;
    AVFormatContext *outputFormatContext;

    AVCodec *decoder;
    AVCodec *encoder;

    AVDictionary *options;

    AVStream *video_st;

    int value;
    int VideoStreamIndx;

public:

    ScreenRecorder();
    ~ScreenRecorder();

    /* function to initiate communication with display library */
    int openCamera();
    int init_outputfile();
    int CaptureVideoFrames();

};

#endif
