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

#include <map>

#include "ScreenSize.h"

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

class ScreenRecorder {
private:
    AVInputFormat *inputFormat;
    AVOutputFormat *outputFormat;
    
    AVCodecContext *decoderContext;
    AVCodecContext *encoderContext;
    
    AVFormatContext *inputFormatContext;
    AVFormatContext *outputFormatContext;
    
    AVCodec *decoder;
    AVCodec *encoder;
        
    AVStream *video_st;
    
    int VideoStreamIndx;
        
    /* Output file resolution */
    unsigned int width, height;
    /* Viewport corners (x, y)*/
    std::pair<int, int> bottomLeft, topRight;
    /* Output file name */
    std::string filename;
    /* Framerate */
    int framerate;
public:
    ScreenRecorder();
    ~ScreenRecorder();
    
    int CaptureVideoFrames();
        
    /* MANAGE FLOW */
    void Start();
    void Pause();
    void Resume();
    void Stop();
    
    /* ENUMS */
    enum Resolution {
        MAXIMUM
    };
    
    enum ViewPort {
        FULLSCREEN,
        LEFT_HALF,
        RIGHT_HALF,
        TOP_HALF,
        BOTTOM_HALF
    };
    
    /* SETTERS */
    /* Set output file resolution */
    void setResolution(unsigned int width, unsigned int height) {
        this->width = width;
        this->height = height;
    }
    
    void setResolution(Resolution resolution) {
        switch (resolution) {
            case MAXIMUM:
                ScreenSize::getScreenResolution(this->width, this->height);
                break;
                
            default:
                break;
        }
    }
    
    /* Set viewport with bottom left corner and top right corner */
    void setViewPortFromCorners1(std::pair<int, int> bottomLeft, std::pair<int, int> topRight) {
        this->bottomLeft = bottomLeft;
        this->topRight = topRight;
    };
    
    /* Set viewport with top left corner and bottom right corner */
    void setViewPortFromCorners2(std::pair<int, int> topLeft, std::pair<int, int> bottomRight) {
        this->bottomLeft = std::make_pair(topLeft.first, bottomRight.second);
        this->topRight = std::make_pair(bottomRight.first, topLeft.second);
    };
    
    /* Set viewport with bottom left corner dimensions */
    void setViewPort(std::pair<int, int> bottomLeft, int width, int height) {
        this->bottomLeft = bottomLeft;
        this->topRight = std::make_pair(bottomLeft.first + width, bottomLeft.second + height);
    };
    
    /* Set viewport from keyword */
    void setViewPort(ViewPort viewport) {
        
    };
    
    void setOutputFile(std::string filename) {
        this->filename = filename;
    };
    
    void setFrameRate(int framerate) {
        this->framerate = framerate;
    };
};

#endif
