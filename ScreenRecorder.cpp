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

    inputFormat = av_find_input_format("gdigrab");
    if(avformat_open_input(&formatContext,"desktop",inputFormat,&options) != 0)
        throw std::runtime_error("Error in opening input.");

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
    av_dict_set(&options, "r", std::to_string(15).c_str(), 0);

    if (avformat_find_stream_info(formatContext, nullptr) < 0)
        throw std::runtime_error("Unable to find the stream information.");

    return formatContext;
}
AVFormatContext *configureAudioInput(AVDictionary *options) {
    AVFormatContext *formatContext = avformat_alloc_context();

    AVInputFormat *inputAudioFormat = nullptr;
    std::string sr = std::to_string(44100);
    //av_dict_set(&options, "sample_rate",sr.c_str(), 0);
#ifdef _WIN32

    inputAudioFormat=av_find_input_format("dshow");
    if(avformat_open_input(&formatContext,"audio=Microfono (4- HyperX Cloud Flight Wireless)",inputAudioFormat,&options) != 0)
        throw std::runtime_error("Error in opening audio input.");

#elif defined linux
    inputAudioFormat=av_find_input_format("x11grab");
    if(avformat_open_input(&formatContext,":0.0+10,20",inputFormat,&options) != 0)
        throw std::runtime_error("Error in opening input.");
#else
    inputAudioFormat = av_find_input_format("avfoundation");
    if(avformat_open_input(&formatContext, "1:0", inputFormat, &options) != 0)
        throw std::runtime_error("Error in opening input.");
#endif

    /* Applying frame rate */

    AVDictionary *options2 = nullptr;

    if (avformat_find_stream_info(formatContext, nullptr) < 0)
        throw std::runtime_error("Unable to find the stream information.");

    return formatContext;
}
AVFormatContext *configureOutput(std::string filename) {
    AVFormatContext *formatContext = nullptr;
    AVOutputFormat *outputFormat = nullptr;
    outputFormat = av_guess_format(nullptr, "output.mp4", nullptr);
    if(!outputFormat)
        throw std::runtime_error("Error in guessing format.");
    avformat_alloc_output_context2(&formatContext, outputFormat, outputFormat->name, filename.c_str());
    if (!formatContext)
        throw std::runtime_error("Error in allocating av format output context.");

    return formatContext;
}
AVCodecContext *configureDecoder(AVFormatContext *formatContext, int &streamIndex, AVMediaType mediaType) {
    AVCodecContext *decoderContext = nullptr;

    AVCodec *decoder = nullptr;
    streamIndex = av_find_best_stream(formatContext, mediaType, -1, -1, &decoder, 0);
    if (streamIndex < 0)
        throw std::runtime_error("Unable to find the stream index.\n");

    decoder = avcodec_find_decoder(formatContext->streams[streamIndex]->codecpar->codec_id);
    decoderContext = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoderContext, formatContext->streams[streamIndex]->codecpar);
    //decoderContext->frame_size = 1000;
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
        //encoderContext->frame_size = 1000;
        encoderContext->time_base = {1, framerate};
    } else {
        encoderContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
        encoderContext->sample_rate = 44100; // 44.1KHz
        encoderContext->channel_layout = AV_CH_LAYOUT_MONO;
        //encoderContext->frame_size = 1000;
        encoderContext->bit_rate = 96000;
        encoderContext->time_base = {1, 41000};
        encoderContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    }


    if (avcodec_open2(encoderContext, encoder, nullptr) < 0)
        throw std::runtime_error("Error in opening the avcodec for " + std::to_string(id));
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
    audioStream->codecpar->sample_rate = 44100; //44.1kHz
    audioStream->codecpar->frame_size = 1000;
    return audioStream;
}
void configureOutput(AVFormatContext *&formatContext, AVCodecContext *&encoderContext, AVCodecContext *&encoderAudioContext, int &videoIndex, int &audioIndex, std::string filename) {
    /* Create empty video file */
    if (!(formatContext->flags & AVFMT_NOFILE))
        if (avio_open2(&formatContext->pb, filename.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr) < 0)
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

    videoIndex = formatContext->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? 0 : 1;
    audioIndex = formatContext->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ? 0 : 1;
}

/* Initialize the resources*/
ScreenRecorder::ScreenRecorder() : filename("./output.mp4"), framerate(30), audio(false), crop(false) {
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
        setResolution(std::get<1>(topRight) - std::get<1>(bottomLeft), std::get<0>(topRight) - std::get<0>(bottomLeft));
    } else {
        ScreenRecorder::setResolution(Resolution::ORIGINAL);
    }

    /* Create and configure input format */
    inputFormatContext = configureInput(nullptr, this->fullWidth, this->fullHeight, this->bottomLeft);
    inputAudioFormatContext = configureAudioInput(nullptr);
    /* Create and configure video and audio decoder */
    decoderContext = configureDecoder(inputFormatContext, inVideoStreamIndex, AVMEDIA_TYPE_VIDEO);
    audioDecoderContext = configureDecoder(inputAudioFormatContext, inAudioStreamIndex, AVMEDIA_TYPE_AUDIO);
    /* Create and configure output format */
    outputFormatContext = configureOutput(filename);
    /* Create and configure video stream */
    videoStream = configureVideoStream(outputFormatContext, width, height, framerate);
    audioStream = configureAudioStream(outputFormatContext, framerate);
    /* Create and configure encoder */
    encoderContext = configureEncoder(videoStream, AV_CODEC_ID_MPEG4, framerate);
    audioEncoderContext = configureEncoder(audioStream, AV_CODEC_ID_AAC, framerate);
    /* Configure output setting and write file header */
    configureOutput(outputFormatContext, encoderContext, audioEncoderContext, outVideoStreamIndex, outAudioStreamIndex, filename);

    swsContext = sws_getContext(decoderContext->width, decoderContext->height, decoderContext->pix_fmt,
                                encoderContext->width, encoderContext->height, encoderContext->pix_fmt,
                                SWS_BICUBIC, NULL, NULL, NULL);

    /* Configure filter crop */
#ifdef macos
    configureFilter(decoderContext, sourceContext, sinkContext, filterGraph, "crop=w=800:h=800:x=100:y=100");
#endif
    /* Clean avformat and pause it */
    av_read_pause(inputFormatContext);
    avformat_flush(inputFormatContext);


}
/*
static int add_samples_to_fifo(AVAudioFifo *fifo,
                               uint8_t **converted_input_samples,
                               const int frame_size){
    int error;

    // Make the FIFO as large as it needs to be to hold both,
     // the old and the new samples.
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
        fprintf(stderr, "Could not reallocate FIFO\n");
        return error;
    }
    // Store the new samples in the FIFO buffer.
    if (av_audio_fifo_write(fifo, (void **)converted_input_samples,
                            frame_size) < frame_size) {
        fprintf(stderr, "Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }
    return 0;
}

void ScreenRecorder::CaptureAudio() {
    AVPacket *inputPacket = av_packet_alloc();
    AVPacket *outputPacket = av_packet_alloc();
    AVFrame *inputFrame = av_frame_alloc();
    AVFrame *outputFrame = av_frame_alloc();

    outputFrame->nb_samples = audioEncoderContext->frame_size;
    outputFrame->channel_layout = audioEncoderContext->channel_layout;
    outputFrame->format = audioEncoderContext->sample_fmt;
    outputFrame->sample_rate = audioEncoderContext->sample_rate;
    av_frame_get_buffer(outputFrame, 0);

    //resampler
    int64_t pts = 0;
    uint8_t*** resampledData;
    SwrContext *resampleContext = nullptr;
    resampleContext = swr_alloc_set_opts(resampleContext,
                                           av_get_default_channel_layout(audioDecoderContext->channels),
                                          audioDecoderContext->sample_fmt,
                                          audioDecoderContext->sample_rate,
                                           av_get_default_channel_layout(audioEncoderContext->channels),
                                          audioEncoderContext->sample_fmt,
                                          audioEncoderContext->sample_rate,
                                           0, nullptr);
    if (!resampleContext) {
        fprintf(stderr, "Could not allocate resample context\n");
        //return AVERROR(ENOMEM);
    }

    if (swr_init(resampleContext) < 0) {
        fprintf(stderr, "Could not open resample context\n");
        swr_free(&resampleContext);
        //return error;
    }

    //use a fifo buffer for the audio samples to be encoded
    AVAudioFifo *fifo = nullptr;

    if (!(fifo = av_audio_fifo_alloc(audioDecoderContext->sample_fmt,
                                      audioDecoderContext->channels, 1))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        //return AVERROR(ENOMEM);
    }

    std::unique_lock ul(m);

    int ret;
    while (!end) {
        // Mutual Exclusion, lock done at the end of the cycle
        cv.wait(ul, [this]() { return this->capture; });

        ul.unlock();

        // Get packet
        if ((ret = av_read_frame(inputAudioFormatContext, inputPacket)) < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                ul.lock();
                continue;
            } else throw std::runtime_error("Error in reading packet from input context.");
        }
        av_packet_rescale_ts(outputPacket, inputAudioFormatContext->streams[inAudioStreamIndex]->time_base, audioDecoderContext->time_base);
            // Decode packet
            if ((ret = avcodec_send_packet(audioDecoderContext, inputPacket)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in decoding the packet.");
            }

            // Get decoded inputFrame

            if ((ret = avcodec_receive_frame(audioDecoderContext, inputFrame)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in receiving the decoded frame.");
            }
        if (outputFormatContext->streams[outAudioStreamIndex]->start_time <= 0) {
            outputFormatContext->streams[outAudioStreamIndex]->start_time = inputFrame->pts;
        }
            //init converted samples
        if (!(*resampledData = (uint8_t**)calloc(audioEncoderContext->channels, sizeof(**resampledData)))) {
            fprintf(stderr, "Could not allocate converted input sample pointers\n");
        }
        if (av_samples_alloc(*resampledData, NULL,
                                      audioEncoderContext->channels,
                                      inputFrame->nb_samples,
                                      audioEncoderContext->sample_fmt, 0) < 0) {
            fprintf(stderr,
                    "Could not allocate converted input samples\n");
            av_freep(&(*resampledData)[0]);
            free(*resampledData);
        }
    //convert samples
        swr_convert(resampleContext,
                    *resampledData, inputFrame->nb_samples,
                    (const uint8_t**)inputFrame->extended_data, inputFrame->nb_samples);
        int x = av_audio_fifo_size(fifo);
        add_samples_to_fifo(fifo, *resampledData, inputFrame->nb_samples);
        x = av_audio_fifo_size(fifo);

        outputFrame->nb_samples = audioEncoderContext->frame_size;
        outputFrame->channel_layout = audioEncoderContext->channel_layout;
        outputFrame->format = audioEncoderContext->sample_fmt;
        outputFrame->sample_rate = audioEncoderContext->sample_rate;
        av_frame_get_buffer(outputFrame, 0);

        while (av_audio_fifo_size(fifo) >= audioEncoderContext->frame_size){
            ret = av_audio_fifo_read(fifo, (void**)(outputFrame->data), audioEncoderContext->frame_size);
            x = av_audio_fifo_size(fifo);
            outputFrame->pts = pts;
            pts += outputFrame->nb_samples;
            // Encoded inputFrame
            if ((avcodec_send_frame(audioEncoderContext, outputFrame)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in encoding the frame.");
            }
            // Receive encoded packet
            while (ret >= 0) {
            if ((ret = avcodec_receive_packet(audioEncoderContext, outputPacket)) >= 0) {
                outputPacket->stream_index = outAudioStreamIndex;

                if (av_interleaved_write_frame(outputFormatContext, outputPacket) < 0)
                    throw std::runtime_error("Error in writing packet to output file.");

                av_packet_unref(outputPacket);
            } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else
                throw std::runtime_error("Error in receiving the decoded audio packet.");
            av_packet_rescale_ts(outputPacket, audioEncoderContext->time_base, outputFormatContext->streams[outAudioStreamIndex]->time_base);
            outputPacket->stream_index = outAudioStreamIndex;
            if (av_interleaved_write_frame(outputFormatContext, outputPacket) < 0)
                throw std::runtime_error("Error in writing packet to output file.");
            av_packet_unref(outputPacket);
            }
            ret = 0;
        }
        ul.lock();
    }

    //DA RIMUOVERE
    if (av_write_trailer(outputFormatContext) < 0) throw std::runtime_error("Error in writing av trailer.");
    avformat_free_context(outputFormatContext);

}*/
int init_fifo(AVAudioFifo *fifo, AVCodecContext* audioEncoderContext)
{
    /* Create the FIFO buffer based on the specified output sample format. */
    if (!(fifo = av_audio_fifo_alloc(audioEncoderContext->sample_fmt,
                                     audioEncoderContext->channels, 1))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

int add_samples_to_fifo(AVAudioFifo *fifo, uint8_t** converted_input_samples, const int frame_size) {
    int error;
    /* Make the FIFO as large as it needs to be to hold both,
     * the old and the new samples. */
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
        fprintf(stderr, "Could not reallocate FIFO\n");
        return error;
    }
    /* Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write(fifo, (void**)converted_input_samples, frame_size) < frame_size) {
        fprintf(stderr, "Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }
    return 0;
}

int initConvertedSamples(uint8_t*** converted_input_samples,
                                         AVCodecContext* output_codec_context,
                                         int frame_size) {
    int error;
    /* Allocate as many pointers as there are audio channels.
     * Each pointer will later point to the audio samples of the corresponding
     * channels (although it may be NULL for interleaved formats).
     */
    if (!(*converted_input_samples = (uint8_t**)calloc(output_codec_context->channels,
                                                       sizeof(**converted_input_samples)))) {
        fprintf(stderr, "Could not allocate converted input sample pointers\n");
        return AVERROR(ENOMEM);
    }
    /* Allocate memory for the samples of all channels in one consecutive
     * block for convenience. */
    if (av_samples_alloc(*converted_input_samples, nullptr,
                         output_codec_context->channels,
                         frame_size,
                         output_codec_context->sample_fmt, 0) < 0) {

        exit(1);
    }
    return 0;
}
using namespace  std;
static int64_t pts = 0;
void ScreenRecorder::CaptureAudio() {
    int ret;
    unique_lock write(n);
    write.unlock();
    AVAudioFifo *fifo = nullptr;
    AVPacket* inPacket, * outPacket;
    AVFrame* rawFrame, * scaledFrame;
    uint8_t** resampledData;
    unique_lock ul(m);
    fifo = av_audio_fifo_alloc(audioDecoderContext->sample_fmt,
                        audioDecoderContext->channels, 1);

    //allocate space for a packet
    inPacket = av_packet_alloc();
    if (!inPacket) {
        cerr << "Cannot allocate an AVPacket for encoded video" << endl;
        exit(1);
    }

    //allocate space for a packet
    rawFrame = av_frame_alloc();
    if (!rawFrame) {
        cerr << "Cannot allocate an AVPacket for encoded video" << endl;
        exit(1);
    }

    scaledFrame = av_frame_alloc();
    if (!scaledFrame) {
        cerr << "Cannot allocate an AVPacket for encoded video" << endl;
        exit(1);
    }

    outPacket = av_packet_alloc();
    if (!outPacket) {
        cerr << "Cannot allocate an AVPacket for encoded video" << endl;
        exit(1);
    }

    //init the resampler
    SwrContext* resampleContext = nullptr;
    resampleContext = swr_alloc_set_opts(resampleContext,
                                         av_get_default_channel_layout(audioEncoderContext->channels),
                                         audioEncoderContext->sample_fmt,
                                         audioEncoderContext->sample_rate,
                                         av_get_default_channel_layout(audioDecoderContext->channels),
                                         audioDecoderContext->sample_fmt,
                                         audioDecoderContext->sample_rate,
                                         0,
                                         nullptr);
    if (!resampleContext) {
        cerr << "Cannot allocate the resample context" << endl;
        exit(1);
    }
    if ((swr_init(resampleContext)) < 0) {
        fprintf(stderr, "Could not open resample context\n");
        swr_free(&resampleContext);
        exit(1);
    }

    while (true) {
        if (!capture) {
            cout << "Pause audio" << endl;
        }
        cv.wait(ul, [this]() { return capture; });

        if (end) {
            break;
        }

        ul.unlock();

        if (av_read_frame(inputAudioFormatContext, inPacket) >= 0 && inPacket->stream_index == inAudioStreamIndex) {
            //decode audio routing
            av_packet_rescale_ts(outPacket, inputAudioFormatContext->streams[inAudioStreamIndex]->time_base, audioDecoderContext->time_base);
            if ((ret = avcodec_send_packet(audioDecoderContext, inPacket)) < 0) {
                cout << "Cannot decode current audio packet " << ret << endl;
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(audioDecoderContext, rawFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else if (ret < 0) {
                    cerr << "Error during decoding" << endl;
                    exit(1);
                }
                if (outputFormatContext->streams[outAudioStreamIndex]->start_time <= 0) {
                    outputFormatContext->streams[outAudioStreamIndex]->start_time = rawFrame->pts;
                }
                initConvertedSamples(&resampledData, audioEncoderContext, rawFrame->nb_samples);

                swr_convert(resampleContext,
                            resampledData, rawFrame->nb_samples,
                            (const uint8_t**)rawFrame->extended_data, rawFrame->nb_samples);

                add_samples_to_fifo(fifo, resampledData, rawFrame->nb_samples);

                //raw frame ready
                outPacket->data = nullptr;
                outPacket->size = 0;

                const int frame_size = FFMAX(av_audio_fifo_size(fifo), audioEncoderContext->frame_size);

                scaledFrame = av_frame_alloc();
                if (!scaledFrame) {
                    cerr << "Cannot allocate an AVPacket for encoded video" << endl;
                    exit(1);
                }

                scaledFrame->nb_samples = audioEncoderContext->frame_size;
                scaledFrame->channel_layout = audioEncoderContext->channel_layout;
                scaledFrame->format = audioEncoderContext->sample_fmt;
                scaledFrame->sample_rate = audioEncoderContext->sample_rate;
                av_frame_get_buffer(scaledFrame, 0);

                while (av_audio_fifo_size(fifo) >= audioEncoderContext->frame_size) {

                    ret = av_audio_fifo_read(fifo, (void**)(scaledFrame->data), audioEncoderContext->frame_size);
                    scaledFrame->pts = pts;
                    pts += scaledFrame->nb_samples;
                    if (avcodec_send_frame(audioEncoderContext, scaledFrame) < 0) {
                        cout << "Cannot encode current audio packet " << endl;
                        exit(1);
                    }
                    while (ret >= 0) {
                        ret = avcodec_receive_packet(audioEncoderContext, outPacket);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            break;
                        else if (ret < 0) {
                            cerr << "Error during encoding" << endl;
                            exit(1);
                        }
                        av_packet_rescale_ts(outPacket, audioEncoderContext->time_base, outputFormatContext->streams[outAudioStreamIndex]->time_base);

                        outPacket->stream_index = outAudioStreamIndex;

                        write.lock();

                        if (av_interleaved_write_frame(outputFormatContext, outPacket) != 0)
                        {
                            cerr << "Error in writing audio frame" << endl;
                        }
                        write.unlock();
                        av_packet_unref(outPacket);
                    }
                    ret = 0;
                }
                av_frame_free(&scaledFrame);
                av_packet_unref(outPacket);
            }
        }
    }
    int x = 7;
}
int ScreenRecorder::Capture() {
    int64_t pts = 0;
    int flag;
    int frameFinished = 0;
    bool endPause = false;
    int numPause = 0;
    unique_lock ul(m);
    ofstream outFile{ "..\\media\\log.txt", ios::out };
    unique_lock write(n);
    write.unlock();
    int frameIndex = 0;
    int value = 0;
    AVPacket* pAVPacket;
    pAVPacket = (AVPacket*)av_malloc(sizeof(AVPacket));
    if (pAVPacket == nullptr) {
        cerr << "Error in allocating AVPacket" << endl;
        exit(-1);
    }

    AVFrame* pAVFrame = av_frame_alloc();
    if (pAVFrame == nullptr) {
        cerr << "Error: unable to alloc the AVFrame resources" << endl;
        exit(-1);
    }

    AVFrame* outFrame = av_frame_alloc();
    if (outFrame == nullptr) {
        cerr << "Error: unable to alloc the AVFrame resources for out frame" << endl;
        exit(-1);
    }


    int videoOutBuffSize;
    int nBytes = av_image_get_buffer_size(encoderContext->pix_fmt, encoderContext->width, encoderContext->height, 32);
    uint8_t* videoOutBuff = (uint8_t*)av_malloc(nBytes);

    if (videoOutBuff == nullptr) {
        cerr << "Error: unable to allocate memory" << endl;
        exit(-1);
    }

    value = av_image_fill_arrays(outFrame->data, outFrame->linesize, videoOutBuff, AV_PIX_FMT_YUV420P, encoderContext->width, encoderContext->height, 1);
    if (value < 0) {
        cerr << "Error in filling image array" << endl;
    }

    SwsContext* swsCtx_;
    swsCtx_ = sws_getContext(decoderContext->width, decoderContext->height, decoderContext->pix_fmt,
                             encoderContext->width, encoderContext->height, encoderContext->pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);

    AVPacket outPacket;
    int gotPicture;

    time_t startTime;
    time(&startTime);

    while (true) {

        if (!capture) {
            cout << "Pause" << endl;
            outFile << "///////////////////   Pause  ///////////////////" << endl;
            cout << "encoderContext->time_base: " << encoderContext->time_base.num << ", " << encoderContext->time_base.den << endl;
        }
        cv.wait(ul, [this]() { return capture; });   //pause capture (not busy waiting)
        if (endPause) {
            endPause = false;
        }

        if (end)  //check if the capture has to stop
            break;
        ul.unlock();

        if (av_read_frame(inputFormatContext, pAVPacket) >= 0 && pAVPacket->stream_index == inVideoStreamIndex) {
            av_packet_rescale_ts(pAVPacket, inputFormatContext->streams[inVideoStreamIndex]->time_base, decoderContext->time_base);
            value = avcodec_decode_video2(decoderContext, pAVFrame, &frameFinished, pAVPacket);
            if (value < 0) {
                cout << "Unable to decode video" << endl;
            }

            if (frameFinished) { //frame successfully decoded
                //sws_scale(swsCtx_, pAVFrame->data, pAVFrame->linesize, 0, pAVCodecContext->height, outFrame->data, outFrame->linesize);
                av_init_packet(&outPacket);
                outPacket.data = nullptr;
                outPacket.size = 0;

                if (outAVFormatContext->streams[outVideoStreamIndex]->start_time <= 0) {
                    outAVFormatContext->streams[outVideoStreamIndex]->start_time = pAVFrame->pts;
                }

                //disable warning on the console
                outFrame->width = encoderContext->width;
                outFrame->height = encoderContext->height;
                outFrame->format = encoderContext->pix_fmt;

                sws_scale(swsCtx_, pAVFrame->data, pAVFrame->linesize, 0, encoderContext->height, outFrame->data, outFrame->linesize);

                avcodec_encode_video2(encoderContext, &outPacket, outFrame, &gotPicture);

                if (gotPicture) {
                    if (outPacket.pts != AV_NOPTS_VALUE) {
                        outPacket.pts = av_rescale_q(outPacket.pts, videoStream->codec->time_base, videoStream->time_base);
                    }
                    if (outPacket.dts != AV_NOPTS_VALUE) {
                        outPacket.dts = av_rescale_q(outPacket.dts, videoStream->codec->time_base, videoStream->time_base);
                    }

                    //cout << "Write frame " << j++ << " (size = " << outPacket.size / 1000 << ")" << endl;
                    //cout << "(size = " << outPacket.size << ")" << endl;

                    //av_packet_rescale_ts(&outPacket, encoderContext->time_base, outAVFormatContext->streams[outVideoStreamIndex]->time_base);
                    //outPacket.stream_index = outVideoStreamIndex;

                    outFile << "outPacket->duration: " << outPacket.duration << ", " << "pAVPacket->duration: " << pAVPacket->duration << endl;
                    outFile << "outPacket->pts: " << outPacket.pts << ", " << "pAVPacket->pts: " << pAVPacket->pts << endl;
                    outFile << "outPacket.dts: " << outPacket.dts << ", " << "pAVPacket->dts: " << pAVPacket->dts << endl;

                    time_t timer;
                    double seconds;

                    ul.lock();

                    write.lock();
                    if (av_write_frame(outAVFormatContext, &outPacket) != 0) {
                        cerr << "Error in writing video frame" << endl;
                    }
                    write.unlock();
                    av_packet_unref(&outPacket);
                }

                av_packet_unref(&outPacket);
                av_free_packet(pAVPacket);  //avoid memory saturation
            }
        }
    }

    outFile.close();

    av_free(videoOutBuff);

    return 0;
}
/*void ScreenRecorder::Capture() {
    AVPacket *inputPacket = av_packet_alloc();
    AVPacket *outputPacket = av_packet_alloc();
    AVFrame *inputFrame = av_frame_alloc();
    AVFrame *outputFrame = av_frame_alloc();
    outputFrame->format = encoderContext->pix_fmt;
    outputFrame->width = encoderContext->width;
    outputFrame->height = encoderContext->height;
    av_frame_get_buffer(outputFrame, 0);

    unique_lock write(n);
    write.unlock();
#ifdef macos
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
        if ((ret = av_read_frame(inputFormatContext, inputPacket)) < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                ul.lock();
                continue;
            } else throw std::runtime_error("Error in reading packet from input context.");
        }
            // Decode packet
            if ((ret = avcodec_send_packet(decoderContext, inputPacket)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in decoding the packet.");
            }
            // Get decoded inputFrame

            if ((ret = avcodec_receive_frame(decoderContext, inputFrame)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in receiving the decoded frame.");
            }
#ifdef macos
        if (crop) {
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
            if ((ret = avcodec_send_frame(encoderContext, outputFrame)) < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in encoding the frame.");
            }

            // Receive encoded packet
            if ((avcodec_receive_packet(encoderContext, outputPacket)) >= 0) {
                outputPacket->pts = av_rescale_q(outputPacket->pts, encoderContext->time_base, videoStream->time_base);
                outputPacket->dts = av_rescale_q(outputPacket->dts, encoderContext->time_base, videoStream->time_base);
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
    if(audio && audioRecorder.joinable())
        audioRecorder.join();
    if (av_write_trailer(outputFormatContext) < 0) throw std::runtime_error("Error in writing av trailer.");
    avformat_free_context(outputFormatContext);
}*/

void ScreenRecorder::Start() {
    ScreenRecorder::Configure();

    recorder = std::thread(&ScreenRecorder::Capture, this);
    audioRecorder = std::thread(&ScreenRecorder::CaptureAudio, this);
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
    cv.notify_all();
}

void ScreenRecorder::Stop() {
    std::unique_lock ul(m);
    end = true;
    cv.notify_all();

}