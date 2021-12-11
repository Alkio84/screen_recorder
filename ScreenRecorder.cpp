//
//  ScreenRecorder.cpp
//  ScreenRecorder
//
//  Created by Checco on 15/10/2021.
//  Copyright © 2021 Checco. All rights reserved.
//

#include "ScreenRecorder.h"

AVFormatContext *configureInput(AVDictionary *options, int width, int height, std::pair<int, int> bottomLeft) {
    AVFormatContext *formatContext = avformat_alloc_context();
    formatContext->probesize = 5 * pow(10, 7);

    /*std::string videoSize = std::to_string(width) + "x" + std::to_string(height);
    std::string offsetX = std::to_string(std::get<1>(bottomLeft));
    std::string offsetY = std::to_string(std::get<0>(bottomLeft));
    av_dict_set(&options,"video_size", videoSize.c_str(), 0);
    av_dict_set(&options, "offset_x", offsetX.c_str(), 0);
    av_dict_set(&options, "offset_y", offsetY.c_str(), 0);*/
    AVInputFormat *inputFormat = nullptr;
#ifdef _WIN32
#if USE_DSHOW
    inputFormat=av_find_input_format("dshow");
    if(avformat_open_input(&formatContext,"video=screen-capture-recorder",inputFormat,NULL) != 0)
        throw std::runtime_error("Error in opening input.");
#else
    inputFormat = av_find_input_format("gdigrab");
    if(avformat_open_input(&formatContext,"desktop",inputFormat,&options) != 0)
        throw std::runtime_error("Error in opening input.");
#endif
#elif defined linux
    inputFormat=av_find_input_format("x11grab");
    if(avformat_open_input(&formatContext,":0.0+10,20",inputFormat,&options) != 0)
        throw std::runtime_error("Error in opening input.");
#else
    inputFormat = av_find_input_format("avfoundation");
    if(avformat_open_input(&formatContext, "1:0", inputFormat, &options) != 0)
        throw std::runtime_error("Error in opening input.");
#endif

    /* Applying frame rate */

    AVDictionary *options2 = nullptr;
    av_dict_set(&options, "r", std::to_string(15).c_str(), 0);

    if (avformat_find_stream_info(formatContext, nullptr) < 0)
        throw std::runtime_error("Unable to find the stream information.");

    return formatContext;
}
AVFormatContext *configureOutput(std::string filename) {
    AVFormatContext *formatContext = nullptr;
    avformat_alloc_output_context2(&formatContext, NULL, NULL, filename.c_str());
    if (!formatContext)
        throw std::runtime_error("Error in allocating av format output context.");

    return formatContext;
}
AVCodecContext *configureDecoder(AVFormatContext *formatContext, int &streamIndex, AVMediaType mediaType) {
    AVCodecContext *decoderContext = nullptr;

    AVCodec *decoder = nullptr;
    streamIndex = av_find_best_stream(formatContext, mediaType, -1, -1, &decoder, 0);
    if (streamIndex == -1)
        throw std::runtime_error("Unable to find the video stream index.\nValue is -1.");

    decoder = avcodec_find_decoder(formatContext->streams[streamIndex]->codecpar->codec_id);
    decoderContext = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoderContext, formatContext->streams[streamIndex]->codecpar);
    if (avcodec_open2(decoderContext, decoder, NULL) < 0)
        throw std::runtime_error("Unable to open the av codec.");

    return decoderContext;
}
AVCodecContext *configureEncoder(AVStream *stream, AVCodecID id, int framerate) {
    AVCodecContext *encoderContext = nullptr;

    AVCodec *encoder = avcodec_find_encoder(id);
    if (!encoder)
        throw std::invalid_argument("Invalid coded id.");

    /* Alloc encoder context */
    encoderContext = avcodec_alloc_context3(encoder);
    if (!encoderContext)
        throw std::runtime_error("Error in allocating the codec context.");

    avcodec_parameters_to_context(encoderContext, stream->codecpar);

    /* Set property of the video file */
    if(id == AV_CODEC_ID_MPEG4) {
        encoderContext->pix_fmt = AV_PIX_FMT_YUV420P;
    } else {
        encoderContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
        encoderContext->sample_rate = 44100; // 44KHz
        encoderContext->channel_layout = AV_CH_LAYOUT_MONO;
    }
    encoderContext->time_base = {1, framerate};

    if (avcodec_open2(encoderContext, encoder, NULL) < 0)
        throw std::runtime_error("Error in opening the avcodec for " + id);

    return encoderContext;
}
void configureFilter(AVCodecContext *decoderContext,AVFilterContext *&sourceContext, AVFilterContext *&sinkContext, AVFilterGraph *&filterGraph, const std::string filtersDesc) {
    const AVFilter *bufferSource = avfilter_get_by_name("buffer");
    const AVFilter *bufferSink = avfilter_get_by_name("buffersink");
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterInOut *outputs = avfilter_inout_alloc();
    filterGraph = avfilter_graph_alloc();
    const std::string args = "video_size=" + std::to_string(decoderContext->width) +
            "x" + std::to_string(decoderContext->height) +
            ":pix_fmt=" + std::to_string(decoderContext->pix_fmt) +
            ":time_base=" + std::to_string(1) +
            "/" + std::to_string(15) +
            ":pixel_aspect=" + std::to_string(decoderContext->sample_aspect_ratio.num) +
            "/" + std::to_string(decoderContext->sample_aspect_ratio.den);


    if (!outputs || !inputs || !filterGraph) {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        throw std::runtime_error("Error in inizializing filter graph");
    }
    if(avfilter_graph_create_filter(&sourceContext, bufferSource, "in", args.c_str(), NULL, filterGraph) < 0){
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        throw std::runtime_error("Error in inizializing input filters");
    }

    if(avfilter_graph_create_filter(&sinkContext, bufferSink, "out", NULL, NULL, filterGraph) < 0){
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        throw std::runtime_error("Error in inizializing output filters");
    }

    /* Connecting bufferSink input to bufferSource output */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = sinkContext;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    /* Connecting bufferSource output to bufferSink input */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = sourceContext;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    if (avfilter_graph_parse_ptr(filterGraph, filtersDesc.c_str(), &inputs, &outputs, NULL) < 0){
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        throw std::runtime_error("Error in inizializing output filters");
    }

    if (avfilter_graph_config(filterGraph, NULL) < 0){
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        throw std::runtime_error("Error in inizializing output filters");
    }

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
}
AVStream *configureVideoStream(AVFormatContext *&formatContext, int width, int height, int framerate) {
    /* Alloc and set stream */
    AVStream *videoStream = avformat_new_stream(formatContext, NULL);
    if (!videoStream)
        throw std::runtime_error("Error in creating a av format new stream.");

    videoStream->codecpar->codec_id = AV_CODEC_ID_MPEG4;
    videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    videoStream->codecpar->bit_rate = 9000 * 1024 * 2; // 2500000
    /* Applying width, height and framerate */
    videoStream->codecpar->width = width;
    videoStream->codecpar->height = height;
    videoStream->time_base = {1, framerate};

    return videoStream;
}
AVStream *configureAudioStream(AVFormatContext *&formatContext, int framerate) {
    /* Alloc and set stream */
    AVStream *audioStream = avformat_new_stream(formatContext, NULL);
    if (!audioStream)
        throw std::runtime_error("Error in creating a av format new stream.");

    audioStream->codecpar->codec_id = AV_CODEC_ID_AAC;
    audioStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    audioStream->codecpar->bit_rate = 128 * 1000; // 128 Kb/s
    /* Applying framerate */
    audioStream->time_base = {1, framerate};

    return audioStream;
}
void configureOutput(AVFormatContext *&formatContext, AVCodecContext *&encoderContext, AVCodecContext *&enc, std::string filename) {
    /* Create empty video file */
    if (!(formatContext->flags & AVFMT_NOFILE))
        if (avio_open2(&formatContext->pb, filename.c_str(), AVIO_FLAG_WRITE, NULL, NULL) < 0)
            throw std::runtime_error("Error in creating video file.");

    /* Some container formats (like MP4) require global headers to be present
Mark the encoder so that it behaves accordingly. */
    if (formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!formatContext->nb_streams)
        throw std::runtime_error("Output file does not contain any stream video header.");

    /* MP4 files require header information */
    if (avformat_write_header(formatContext, nullptr) < 0)
        throw std::runtime_error("Error in writing video header.");
}

/* Initialize the resources*/
ScreenRecorder::ScreenRecorder() : filename("./output.mp4"), framerate(30), audio(true) {
    inputFormatContext = nullptr;
    outputFormatContext = nullptr;
    decoderContext = nullptr;
    encoderContext = nullptr;
}

/* Uninitialize the resources */
ScreenRecorder::~ScreenRecorder() {
    /* Trust me, bro */
    if (recorder.joinable()) recorder.join();

    avformat_close_input(&inputFormatContext);
    if (!inputFormatContext) std::cout << "\nfile closed successfully";
    else std::cout << "\nunable to close the file";

    avformat_free_context(inputFormatContext);
    if (!inputFormatContext) std::cout << "\navformat free successfully";
    else std::cout << "\nunable to free avformat context";
}

void ScreenRecorder::Configure() {
    avformat_network_init();
    avdevice_register_all();

    if (crop) {
        /* Viewport is default, fullscreen */
        ScreenSize::getScreenResolution(fullWidth, fullHeight);
    } else {
        ScreenSize::getScreenResolution(width, height);
    }

    /* Create and configure input format */
    inputFormatContext = configureInput(nullptr, this->fullWidth, this->fullHeight, this->bottomLeft);
    /* Create and configure video and audio decoder */
    decoderContext = configureDecoder(inputFormatContext, videoStreamIndex, AVMEDIA_TYPE_VIDEO);
    //audioDecoderContext = configureDecoder(inputFormatContext, audioStreamIndex, AVMEDIA_TYPE_AUDIO);
    /* Create and configure output format */
    outputFormatContext = configureOutput(filename);
    /* Create and configure video stream */
    videoStream = configureVideoStream(outputFormatContext, width, height, framerate);
    //outputFormatContext->oformat->write_header
    //audioStream = configureAudioStream(outputFormatContext, framerate);
    /* Create and configure encoder */
    encoderContext = configureEncoder(videoStream, AV_CODEC_ID_MPEG4, framerate);
    //audioEncoderContext = configureEncoder(audioStream, AV_CODEC_ID_AAC, framerate);
    /* Configure output setting and write file header */
    configureOutput(outputFormatContext, encoderContext, audioEncoderContext, filename);

    swsContext = sws_getContext(decoderContext->width, decoderContext->height, decoderContext->pix_fmt,
                                encoderContext->width, encoderContext->height, encoderContext->pix_fmt,
                                SWS_BICUBIC, NULL, NULL, NULL);
    /* Configure filter crop */

    configureFilter(decoderContext, sourceContext, sinkContext, filterGraph, "crop=w=800:h=800:x=100:y=100");

    /* Clean avformat and pause it */
    av_read_pause(inputFormatContext);
    avformat_flush(inputFormatContext);


}

void ScreenRecorder::Capture() {
    AVPacket *inputPacket = av_packet_alloc();
    AVPacket *outputPacket = av_packet_alloc();
    AVFrame *inputFrame = av_frame_alloc();
    AVFrame *filteredFrame = av_frame_alloc();

    AVFrame *outputFrame = av_frame_alloc();
    outputFrame->format = encoderContext->pix_fmt;
    outputFrame->width = encoderContext->width;
    outputFrame->height = encoderContext->height;
    av_frame_get_buffer(outputFrame, 0);

    /*Filters*/
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterInOut *outputs = avfilter_inout_alloc();

    std::unique_lock ul(m);

    AVCodecContext *decCtx = nullptr;
    AVCodecContext *encCtx = nullptr;

    int ret;
    while (!end) {
        /* Mutual Exclusion, lock done at the end of the cycle */
        cv.wait(ul, [this]() { return this->capture; });

        ul.unlock();

        /* Get packet */
        if ((ret = av_read_frame(inputFormatContext, inputPacket)) < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                ul.lock();
                continue;
            } else throw std::runtime_error("Error in reading packet from input context.");
        }

        bool isVideo = inputPacket->stream_index == videoStreamIndex;
        bool isAudio = inputPacket->stream_index == audioStreamIndex;
        if (isVideo || isAudio) {
            decCtx = isVideo ? decoderContext : audioDecoderContext;
            encCtx = isVideo ? encoderContext : audioEncoderContext;
            /* Decode packet */
            if ((ret = avcodec_send_packet(decCtx, inputPacket)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in decoding the packet.");
            }

            /* Get decoded inputFrame  */

            if ((ret = avcodec_receive_frame(decCtx, inputFrame)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in receiving the decoded frame.");
            }
            //inputFrame->crop_top = 360;
            //av_frame_apply_cropping(inputFrame, encoderContext->flags & AV_CODEC_FLAG_UNALIGNED ? AV_FRAME_CROP_UNALIGNED : 0);
            /* only if we need to crop the image */
            if(crop){
                /* Push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame_flags(sourceContext, inputFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                        ul.lock();
                        continue;
                    } else throw std::runtime_error("Error in filtering.");
                }
                if ((ret = av_buffersink_get_frame(sinkContext, filteredFrame)) < 0) {
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                        ul.lock();
                        continue;
                    } else throw std::runtime_error("Error in filtering.");
                }
            }
            else
                /* Resize the inputFrame */
                if(isVideo) sws_scale(swsContext, inputFrame->data, inputFrame->linesize, 0, inputFrame->height, outputFrame->data, outputFrame->linesize);
            /* Encoded inputFrame */

            if ((ret = avcodec_send_frame(encCtx, filteredFrame)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in encoding the frame.");
            }

            /* Receive encoded packet */
            if ((avcodec_receive_packet(encCtx, outputPacket)) >= 0) {
                outputPacket->pts = av_rescale_q(outputPacket->pts, encoderContext->time_base, videoStream->time_base);
                outputPacket->dts = av_rescale_q(outputPacket->dts, encoderContext->time_base, videoStream->time_base);

                if (av_interleaved_write_frame(outputFormatContext, outputPacket) < 0)
                    throw std::runtime_error("Error in writing packet to output file.");

                av_packet_unref(outputPacket);
            } else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
                throw std::runtime_error("Error in receiving the decoded packet.");
        }

        ul.lock();
    }
    //outputFormatContext->streams->
    if (av_write_trailer(outputFormatContext) < 0) throw std::runtime_error("Error in writing av trailer.");

    avformat_free_context(outputFormatContext);
}

void ScreenRecorder::Start() {
    ScreenRecorder::Configure();

    recorder = std::thread(&ScreenRecorder::Capture, this);
}

void ScreenRecorder::Pause() {
    std::unique_lock ul(m);
    av_read_pause(inputFormatContext);
    capture = false;
}

void ScreenRecorder::Resume() {
    av_read_play(inputFormatContext);
    std::unique_lock ul(m);
    capture = true;
    cv.notify_one();
}

void ScreenRecorder::Stop() {
    std::unique_lock ul(m);
    end = true;
    cv.notify_one();
}
