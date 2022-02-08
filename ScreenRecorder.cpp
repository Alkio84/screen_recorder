//
//  ScreenRecorder.cpp
//  ScreenRecorder
//
//  Created by Checco on 15/10/2021.
//  Copyright © 2021 Checco. All rights reserved.
//

#include "ScreenRecorder.h"

void ScreenRecorder::configureVideoInput() {
    inputVideoFormatContext = avformat_alloc_context();
    inputVideoFormatContext->probesize = 5 * pow(10, 7);

    /*std::string videoSize = std::to_string(width) + "x" + std::to_string(height);
    std::string offsetX = std::to_string(std::get<1>(bottomLeft));
    std::string offsetY = std::to_string(std::get<0>(bottomLeft));
    av_dict_set(&options,"video_size", videoSize.c_str(), 0);
    av_dict_set(&options, "offset_x", offsetX.c_str(), 0);
    av_dict_set(&options, "offset_y", offsetY.c_str(), 0);*/
    AVInputFormat *inputFormat = nullptr;
#ifdef _WIN32
    inputFormat = av_find_input_format("gdigrab");
    if(avformat_open_input(&inputVideoFormatContext,"desktop",inputFormat,&videoOptions) != 0)
        throw std::runtime_error("Error in opening input.");
#elif __linux__
    inputFormat=av_find_input_format("x11grab");
    if(avformat_open_input(&inputVideoFormatContext,":0.0+10,20",inputFormat,nullptr) != 0)
        throw std::runtime_error("Error in opening input.");
#elif __APPLE__
    inputFormat = av_find_input_format("avfoundation");
    if(avformat_open_input(&inputVideoFormatContext, "1:", inputFormat, &options) != 0)
        throw std::runtime_error("Error in opening input.");
#endif

    /* Applying frame rate */
    av_dict_set(&videoOptions, "r", std::to_string(framerate).c_str(), 0);
    if (avformat_find_stream_info(inputVideoFormatContext, nullptr) < 0)
        throw std::runtime_error("Unable to find the stream information.");
}

void ScreenRecorder::inizializeOutput() {
    AVOutputFormat *outputFormat = nullptr;
    outputFormat = av_guess_format(nullptr, "output.mp4", nullptr);
    if(!outputFormat)
        throw std::runtime_error("Error in guessing format.");
    avformat_alloc_output_context2(&outputFormatContext, outputFormat, outputFormat->name, filename.c_str());
    if (!outputFormatContext)
        throw std::runtime_error("Error in allocating av format output context.");
}
void ScreenRecorder::configureVideoDecoder() {
    AVMediaType mediaType = AVMEDIA_TYPE_VIDEO;
    AVCodec *decoder = nullptr;
    inVideoStreamIndex = av_find_best_stream(inputVideoFormatContext, mediaType, -1, -1, &decoder, 0);
    if (inVideoStreamIndex < 0)
        throw std::runtime_error("Unable to find the stream index.\n");

    decoder = avcodec_find_decoder(inputVideoFormatContext->streams[inVideoStreamIndex]->codecpar->codec_id);
    videoDecoderContext = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(videoDecoderContext, inputVideoFormatContext->streams[inVideoStreamIndex]->codecpar);
    if (avcodec_open2(videoDecoderContext, decoder, NULL) < 0)
        throw std::runtime_error("Unable to open the av codec.");
}
void ScreenRecorder::configureVideoEncoder() {

    AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!encoder)
        throw std::invalid_argument("Invalid coded id.");

    /* Alloc encoder context */
    videoEncoderContext = avcodec_alloc_context3(encoder);
    if (!videoEncoderContext)
        throw std::runtime_error("Error in allocating the codec context.");

    avcodec_parameters_to_context(videoEncoderContext, outVideoStream->codecpar);

    /* Set property of the video file */
    videoEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;
    videoEncoderContext->time_base = {1, framerate};


    if (avcodec_open2(videoEncoderContext, encoder, nullptr) < 0)
        throw std::runtime_error("Error in opening the avcodec for " + std::to_string(AV_CODEC_ID_MPEG4));

}
void ScreenRecorder::configureFilters() {

    const AVFilter *bufferSource = avfilter_get_by_name("buffer");
    const AVFilter *bufferSink = avfilter_get_by_name("buffersink");
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterInOut *outputs = avfilter_inout_alloc();
    filterGraph = avfilter_graph_alloc();
    std::string filtersDesc = "crop=w=800:h=800:x=100:y=100";
    const std::string args = "video_size=" + std::to_string(videoDecoderContext->width) +
                             "x" + std::to_string(videoDecoderContext->height) +
                             ":pix_fmt=" + std::to_string(videoDecoderContext->pix_fmt) +
                             ":time_base=" + std::to_string(1) +
                             "/" + std::to_string(15) +
                             ":pixel_aspect=" + std::to_string(videoDecoderContext->sample_aspect_ratio.num) +
                             "/" + std::to_string(videoDecoderContext->sample_aspect_ratio.den);


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
void ScreenRecorder::configureOutVideoStream() {
    /* Alloc and set stream */
    outVideoStream = avformat_new_stream(outputFormatContext, NULL);
    if (!outVideoStream)
        throw std::runtime_error("Error in creating a av format new stream.");

    outVideoStream->codecpar->codec_id = AV_CODEC_ID_MPEG4;
    outVideoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    outVideoStream->codecpar->bit_rate = 9000 * 1024 * 2; // 2500000
    /* Applying width, height and framerate */
    outVideoStream->codecpar->width = width;
    outVideoStream->codecpar->height = height;
    outVideoStream->time_base = {1, framerate};
}

void ScreenRecorder::configureOutput() {
    /* Create empty video file */
    if (!(outputFormatContext->flags & AVFMT_NOFILE))
        if (avio_open2(&outputFormatContext->pb, filename.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr) < 0)
            throw std::runtime_error("Error in creating video file.");

    /* Some container formats (like MP4) require global headers to be present
Mark the encoder so that it behaves accordingly. */
    if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        videoEncoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!outputFormatContext->nb_streams)
        throw std::runtime_error("Output file does not contain any stream video header.");

    /* MP4 files require header information */
    if (avformat_write_header(outputFormatContext, nullptr) < 0)
        throw std::runtime_error("Error in writing video header.");

    outVideoStreamIndex = outputFormatContext->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? 0 : 1;
    outAudioStreamIndex = outputFormatContext->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ? 0 : 1;
}

/* Initialize the resources*/
ScreenRecorder::ScreenRecorder() : filename("./output.mp4"), framerate(15), audio(false), crop(false), capture(true), end(false) {
    inputVideoFormatContext = nullptr;
    inputAudioFormatContext = nullptr;
    outputFormatContext = nullptr;
    videoDecoderContext = nullptr;
    videoEncoderContext = nullptr;
    inVideoStream = nullptr;
    outVideoStream = nullptr;
    inVideoStreamIndex = -1;
    outVideoStreamIndex = -1;
    videoOptions = nullptr;

    audioDecoderContext = nullptr;
    audioEncoderContext = nullptr;
    inAudioStream = nullptr;
    outAudioStream = nullptr;
    inAudioStreamIndex = -1;
    outAudioStreamIndex = -1;
    fifo = nullptr;
    swsContext = nullptr;
    resampleContext = nullptr;

    sourceContext = nullptr;
    sinkContext = nullptr;
    filterGraph = nullptr;
}

/* Uninitialize the resources */
ScreenRecorder::~ScreenRecorder() { //DA RIVEDERE COMPLETAMENTE
    /* Trust me, bro */
    if (recorder.joinable()) recorder.join();

    /*avformat_close_input(&inputFormatContext);
    if (!inputFormatContext) std::cout << "\nfile closed successfully";
    else std::cout << "\nunable to close the file";

    avformat_free_context(inputFormatContext);
    if (!inputFormatContext) std::cout << "\navformat free successfully";
    else std::cout << "\nunable to free avformat context";*/
}

void ScreenRecorder::Configure() {
    avformat_network_init();
    avdevice_register_all();

    if (crop) {
        /* Viewport is default, fullscreen */
        setResolution(std::get<1>(topRight) - std::get<1>(bottomLeft), std::get<0>(topRight) - std::get<0>(bottomLeft));
    } else {
        ScreenRecorder::setResolution(Resolution::ORIGINAL);
    }

    /* Create and configure input format */
    ScreenRecorder::configureVideoInput();
    ScreenRecorder::configureAudioInput();
    /* Create and configure video and audio decoder */
    ScreenRecorder::configureVideoDecoder();
    ScreenRecorder::configureAudioDecoder();
    /* Create and configure output format */
    ScreenRecorder::inizializeOutput();
    /* Create and configure video stream */
    ScreenRecorder::configureOutVideoStream();
    ScreenRecorder::configureOutAudioStream();
    /* Create and configure encoder */
    ScreenRecorder::configureVideoEncoder();
    ScreenRecorder::configureAudioEncoder();
    /* Configure output setting and write file header */
    ScreenRecorder::configureOutput();

    swsContext = sws_getContext(videoDecoderContext->width, videoDecoderContext->height, videoDecoderContext->pix_fmt,
                                videoEncoderContext->width, videoEncoderContext->height, videoEncoderContext->pix_fmt,
                                SWS_BICUBIC, NULL, NULL, NULL);

    /* Configure filter crop */
#ifdef __APPLE__
    configureFilter(decoderContext, sourceContext, sinkContext, filterGraph, "crop=w=800:h=800:x=100:y=100");
#endif
    /* Clean avformat and pause it */
    av_read_pause(inputVideoFormatContext);
    avformat_flush(inputVideoFormatContext);
    av_read_pause(inputAudioFormatContext);
    avformat_flush(inputAudioFormatContext);


}




void ScreenRecorder::Capture() {
    AVPacket *inputPacket = av_packet_alloc();
    AVPacket *outputPacket = av_packet_alloc();
    AVFrame *inputFrame = av_frame_alloc();
    AVFrame *outputFrame = av_frame_alloc();
    outputFrame->format = videoEncoderContext->pix_fmt;
    outputFrame->width = videoEncoderContext->width;
    outputFrame->height = videoEncoderContext->height;
    av_frame_get_buffer(outputFrame, 0);

    std::unique_lock write(n);
    write.unlock();
#ifdef __APPLE__
    //Filters
    AVFrame *filteredFrame = av_frame_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterInOut *outputs = avfilter_inout_alloc();
#endif
    std::unique_lock ul(m);

    int ret;
    while (!end) {
        // Mutual Exclusion, lock done at the end of the cycle
        cv.wait(ul, [this]() { return this->capture; });

        ul.unlock();

        // Get packet
        if ((ret = av_read_frame(inputVideoFormatContext, inputPacket)) < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                ul.lock();
                continue;
            } else throw std::runtime_error("Error in reading packet from input context.");
        }
        // Decode packet
        if ((ret = avcodec_send_packet(videoDecoderContext, inputPacket)) < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                ul.lock();
                continue;
            } else throw std::runtime_error("Error in decoding the packet.");
        }
        // Get decoded inputFrame

        if ((ret = avcodec_receive_frame(videoDecoderContext, inputFrame)) < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                ul.lock();
                continue;
            } else throw std::runtime_error("Error in receiving the decoded frame.");
        }
#ifdef __APPLE__
        if (crop && false) {
            // Push the decoded frame into the filtergraph
            if (av_buffersrc_add_frame_flags(sourceContext, inputFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
                throw std::runtime_error("Error in filtering.");
            // Pull the filtered frame from the buffersink
            if (av_buffersink_get_frame(sinkContext, filteredFrame) < 0)
                throw std::runtime_error("Error in filtering.");

            //sws_scale(swsContext, inputFrame->data, inputFrame->linesize, 0, decoderContext->height, outputFrame->data, outputFrame->linesize);

            //filteredFrame->pkt_dts = outputFrame->pkt_dts;
        }
            // only if we need to crop the image
        else {
            // Resize the inputFrame
            sws_scale(swsContext, inputFrame->data, inputFrame->linesize, 0, inputFrame->height,
                      outputFrame->data, outputFrame->linesize);
        }
#else
        sws_scale(swsContext, inputFrame->data, inputFrame->linesize, 0, inputFrame->height,
                  outputFrame->data, outputFrame->linesize);
#endif

        // Encoded inputFrame
        if ((ret = avcodec_send_frame(videoEncoderContext, outputFrame)) < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                ul.lock();
                continue;
            } else throw std::runtime_error("Error in encoding the frame.");
        }

        // Receive encoded packet
        if ((avcodec_receive_packet(videoEncoderContext, outputPacket)) >= 0) {
            outputPacket->pts = av_rescale_q(outputPacket->pts, videoEncoderContext->time_base, outVideoStream->time_base);
            outputPacket->dts = av_rescale_q(outputPacket->dts, videoEncoderContext->time_base, outVideoStream->time_base);
            write.lock();
            if (av_interleaved_write_frame(outputFormatContext, outputPacket) < 0)
                throw std::runtime_error("Error in writing packet to output file.");
            write.unlock();
            av_packet_unref(outputPacket);
        } else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            throw std::runtime_error("Error in receiving the decoded packet.");

        ul.lock();
    }
    //wait for audio to end before closing the file
    /*if(audio && audioRecorder.joinable())
        audioRecorder.join();*/

}

