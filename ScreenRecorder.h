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
#include <mutex>
#include <thread>
#include <condition_variable>

#include "ScreenSize.h"

#define __STDC_CONSTANT_MACROS

//FFMPEG LIBRARIES
extern "C" {
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
    /* Input and output */
    AVFormatContext *inputFormatContext;
    AVFormatContext *inputAudioFormatContext;
    AVFormatContext *outputFormatContext;

    /* Video */
    AVCodecContext *decoderContext;
    AVCodecContext *encoderContext;
    AVStream *videoStream;
    int videoStreamIndex;

    /* Audio */
    AVCodecContext *audioDecoderContext;
    AVCodecContext *audioEncoderContext;
    AVStream *audioStream;
    int audioStreamIndex;

    /* Converter */
    SwsContext *swsContext;

    /*Filter*/
    AVFilterContext *sourceContext;
    AVFilterContext *sinkContext;
    AVFilterGraph *filterGraph;

    /* Configure before starting the video */
    void Configure();
    void Capture();
    bool audio;
    int fullWidth, fullHeight;
    int width, height;
    bool crop = false;
    std::pair<int, int> bottomLeft, topRight;
    std::string filename;
    int framerate;

    /*Thread Management*/
    std::thread recorder;
    bool capture = true;
    bool end = false;
    std::mutex m;
    std::condition_variable cv;
public:
    ScreenRecorder();
    ~ScreenRecorder();

    /* MANAGE FLOW */
    void Start();
    void Pause();
    void Resume();
    void Stop();

    /* ENUMS */
    enum Resolution {
        ORIGINAL,
        HALF
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
    void setResolution(int width, int height) {
        this->width = width;
        this->height = height;
        ScreenSize::getScreenResolution(fullWidth, fullHeight);
    }

    void setResolution(Resolution resolution) {
        switch (resolution) {
            case ORIGINAL:
                ScreenSize::getScreenResolution(width, height);
                ScreenSize::getScreenResolution(fullWidth, fullHeight);
                break;
            case HALF:

                break;
            default:
                break;
        }
    }

    void recordAudio(bool audio) {
        this->audio = audio;
    }

    /* Set viewport with bottom left corner and top right corner */
    void setViewPortFromCorners1(std::pair<int, int> bottomLeft, std::pair<int, int> topRight) {
        this->bottomLeft = bottomLeft;
        this->topRight = topRight;
        this->crop = true;
    };

    /* Set viewport with top left corner and bottom right corner */
    void setViewPortFromCorners2(std::pair<int, int> topLeft, std::pair<int, int> bottomRight) {
        this->bottomLeft = std::make_pair(topLeft.first, bottomRight.second);
        this->topRight = std::make_pair(bottomRight.first, topLeft.second);
        this->crop = true;
    };

    /* Set viewport with bottom left corner dimensions */
    void setViewPort(std::pair<int, int> bottomLeft, int width, int height) {
        this->bottomLeft = bottomLeft;
        this->topRight = std::make_pair(bottomLeft.first + width, bottomLeft.second + height);
        this->crop = true;
    };

    /* Set viewport from keyword */
    void setViewPort(ViewPort viewport) {

    };

    void setOutputFile(std::string filename) {
        this->filename = filename;
    };

    void setFrameRate(int framerate) {
#ifdef _WIN32
        if(framerate > 15) this->framerate = 15;
        else
#endif
            this->framerate = framerate;
    };
};

#endif