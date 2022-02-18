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
#include "libavutil/audio_fifo.h"
// lib swresample

#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavdevice/avdevice.h"

}

class ScreenRecorder {
private:
    /* Input and output */
    AVFormatContext *inputVideoFormatContext;
    AVFormatContext *inputAudioFormatContext;
    AVFormatContext *outputFormatContext;

    /* Video */
    AVCodecContext *videoDecoderContext;
    AVCodecContext *videoEncoderContext;
    AVStream *inVideoStream;
    AVStream *outVideoStream;
    int inVideoStreamIndex;
    int outVideoStreamIndex;
    AVDictionary *videoOptions;

    /* Audio */
    AVCodecContext *audioDecoderContext;
    AVCodecContext *audioEncoderContext;
    AVStream *inAudioStream;
    AVStream *outAudioStream;
    int inAudioStreamIndex;
    int outAudioStreamIndex;
    AVAudioFifo *fifo;

    /* Converter */
    SwsContext *swsContext;
    SwrContext *resampleContext;

    /*Filter*/
    AVFilterContext *sourceContext;
    AVFilterContext *sinkContext;
    AVFilterGraph *filterGraph;

    /* Configure before starting the video */
    void Configure();
    void Capture();
    void CaptureAudio();

    /* Video functions */
    void inizializeOutput();
    void configureVideoInput();
    void configureVideoDecoder();
    void configureVideoEncoder();
    void configureOutVideoStream();
    void configureOutput();

    /* Audio functions */
    void configureAudioInput();
    void configureAudioDecoder();
    void configureAudioEncoder();
    void configureOutAudioStream();

    /* Filter functions */
    void configureFilters();

    /* Parameters */
    bool isAudioRecorded;
    bool isCropped;
    int width, height;
    std::pair<int, int> bottomLeft, topRight;
    std::string filename;
    int framerate;
    std::string audioDevice;

    /*Thread Management*/
    std::thread recorder;
    std::thread audioRecorder;
    bool capture;
    bool end;
    //write
    std::mutex n;
    //pause-stop
    std::mutex m;
    std::condition_variable cv;
public:
    ScreenRecorder();
    ~ScreenRecorder();

    /* MANAGE FLOW */
    void Start() {
        ScreenRecorder::Configure();

        recorder = std::thread(&ScreenRecorder::Capture, this);
        if(isAudioRecorded) audioRecorder = std::thread(&ScreenRecorder::CaptureAudio, this);
    }

    void Pause() {
        std::unique_lock ul(m);
        av_read_pause(inputVideoFormatContext);
        av_read_pause(inputAudioFormatContext);
        capture = false;
    }

    void Resume() {
        av_read_play(inputVideoFormatContext);
        av_read_play(inputAudioFormatContext);
        std::unique_lock ul(m);
        capture = true;
        cv.notify_all();
    }

    void Stop() {
        try {
            std::unique_lock ul(m);
            end = true;
            cv.notify_all();
            ul.unlock();
            if (isAudioRecorded && audioRecorder.joinable())
                audioRecorder.join();
            if (recorder.joinable())
                recorder.join();

            if (av_write_trailer(outputFormatContext) < 0) throw std::runtime_error("Error in writing av trailer.");
            avformat_free_context(outputFormatContext);
        } catch (std::exception &ex) {
            std::cout << ex.what() << std::endl;
        }
    }

    /* SETTERS */
    void recordAudio(bool audio) {
        this->isAudioRecorded = audio;
    }

    /* Set viewport with bottom left corner and top right corner */
    void setViewPortFromCorners1(std::pair<int, int> bottomLeft, std::pair<int, int> topRight) {
        this->bottomLeft = bottomLeft;
        this->topRight = topRight;
        this->isCropped = true;
    };

    /* Set viewport with top left corner and bottom right corner */
    void setViewPortFromCorners2(std::pair<int, int> topLeft, std::pair<int, int> bottomRight) {
        this->bottomLeft = std::make_pair(topLeft.first, bottomRight.second);
        this->topRight = std::make_pair(bottomRight.first, topLeft.second);
        this->isCropped = true;
    };

    /* Set viewport with bottom left corner dimensions */
    void setViewPort(std::pair<int, int> bottomLeft, int width, int height) {
        this->bottomLeft = bottomLeft;
        this->topRight = std::make_pair(bottomLeft.first + width, bottomLeft.second + height);
        this->isCropped = true;
    };

    void setOutputFile(std::string filename) {
        this->filename = filename;
    };
#if _WIN32
    void setAudioDevice(std::string name){
        audioDevice = "audio=" + name;
    }
#endif
    void setFrameRate(int framerate) {
#ifdef _WIN32
        if(framerate > 15) this->framerate = 15;
        else
#endif
            this->framerate = framerate;
    };
};

#endif