//
// Created by ferre on 19/12/2021.
//
#include "ScreenRecorder.h"

const AVSampleFormat requireAudioFmt = AV_SAMPLE_FMT_FLTP;

void ScreenRecorder::configureAudioInput() {
    inputAudioFormatContext = avformat_alloc_context();

    AVInputFormat *inputFormat = nullptr;
    std::string sr = std::to_string(44100);
    //av_dict_set(&options, "sample_rate",sr.c_str(), 0);
#ifdef _WIN32
    inputFormat=av_find_input_format("dshow");
    if(avformat_open_input(&inputAudioFormatContext,"audio=Microfono (4- HyperX Cloud Flight Wireless)",inputFormat, nullptr) != 0)
        throw std::runtime_error("Error in opening audio input.");
#elif defined linux
    inputFormat=av_find_input_format("x11grab");
    if(avformat_open_input(&inputAudioFormatContext,":0.0+10,20",inputFormat,nullptr) != 0)
        throw std::runtime_error("Error in opening input.");
#elif __APPLE__
    inputFormat = av_find_input_format("avfoundation");
    if(avformat_open_input(&inputAudioFormatContext, ":0", inputFormat, nullptr) != 0)
        throw std::runtime_error("Error in opening input.");
#endif

    if (avformat_find_stream_info(inputAudioFormatContext, nullptr) < 0)
        throw std::runtime_error("Unable to find the stream information.");

}

void ScreenRecorder::configureAudioDecoder() {
    AVMediaType mediaType = AVMEDIA_TYPE_AUDIO;
    AVCodec *decoder = nullptr;
    inAudioStreamIndex = av_find_best_stream(inputAudioFormatContext, mediaType, -1, -1, &decoder, 0);
    if (inAudioStreamIndex < 0)
        throw std::runtime_error("Unable to find the stream index.\n");

    decoder = avcodec_find_decoder(inputAudioFormatContext->streams[inAudioStreamIndex]->codecpar->codec_id);
    audioDecoderContext = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(audioDecoderContext, inputAudioFormatContext->streams[inAudioStreamIndex]->codecpar);
    audioDecoderContext->frame_size = 1024;
    if (avcodec_open2(audioDecoderContext, decoder, NULL) < 0)
        throw std::runtime_error("Unable to open the av codec.");

}

void ScreenRecorder::configureAudioEncoder() {

    AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!encoder)
        throw std::invalid_argument("Invalid coded id.");

    /* Alloc encoder context */
    audioEncoderContext = avcodec_alloc_context3(encoder);
    if (!audioEncoderContext)
        throw std::runtime_error("Error in allocating the codec context.");

    avcodec_parameters_to_context(audioEncoderContext, outAudioStream->codecpar);

    /* Set property of the video file */

    audioEncoderContext->sample_fmt = requireAudioFmt;
    audioEncoderContext->sample_rate = 44100; // 44.1KHz
    audioEncoderContext->channels = inputAudioFormatContext->streams[inAudioStreamIndex]->codecpar->channels;
    audioEncoderContext->channel_layout = av_get_default_channel_layout(inputAudioFormatContext->streams[inAudioStreamIndex]->codecpar->channels);;
    audioEncoderContext->bit_rate = 32000;
    audioEncoderContext->time_base = {1, 44100};
    //audioEncoderContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    if (avcodec_open2(audioEncoderContext, encoder, nullptr) < 0)
        throw std::runtime_error("Error in opening the avcodec for " + std::to_string(AV_CODEC_ID_AAC));

}

void ScreenRecorder::configureOutAudioStream() {
    /* Alloc and set stream */
    outAudioStream = avformat_new_stream(outputFormatContext, NULL);
    if (!outAudioStream)
        throw std::runtime_error("Error in creating a av format new stream.");

    outAudioStream->codecpar->codec_id = AV_CODEC_ID_AAC;
    outAudioStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    outAudioStream->codecpar->bit_rate = 1 * 1000; // 128 Kb/s
    /* Applying framerate */
    outAudioStream->codecpar->sample_rate = 44100; //44.1kHz
    outAudioStream->codecpar->frame_size = 1024;
}

static void add_samples_to_fifo(AVAudioFifo *fifo, uint8_t **converted_input_samples, const int frame_size) {
    if (av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size) < 0)
        throw std::runtime_error("Could not reallocate FIFO.");

    if (av_audio_fifo_write(fifo, (void **)converted_input_samples, frame_size) < frame_size)
        throw std::runtime_error("Could not write data to FIFO.");
}

void ScreenRecorder::CaptureAudio() {
    AVPacket *inputPacket = av_packet_alloc();
    AVPacket *outputPacket = av_packet_alloc();
    AVFrame *inputFrame = av_frame_alloc();
    /*AVFrame *outputFrame = av_frame_alloc();

    outputFrame->nb_samples = audioEncoderContext->frame_size;
    outputFrame->channel_layout = audioEncoderContext->channel_layout;
    outputFrame->format = audioEncoderContext->sample_fmt;
    outputFrame->sample_rate = audioEncoderContext->sample_rate;
    av_frame_get_buffer(outputFrame, 0);*/

    //resampler
    int64_t pts = 0;
    resampleContext = swr_alloc_set_opts(nullptr,
                                         av_get_default_channel_layout(audioDecoderContext->channels),
                                         requireAudioFmt,  // aac encoder only receive this format
                                         audioDecoderContext->sample_rate,
                                         av_get_default_channel_layout(audioDecoderContext->channels),
                                         (AVSampleFormat)inputAudioFormatContext->streams[inAudioStreamIndex]->codecpar->format,
                                         inputAudioFormatContext->streams[inAudioStreamIndex]->codecpar->sample_rate,
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

    if (!(fifo = av_audio_fifo_alloc(requireAudioFmt,
                                     audioDecoderContext->channels, audioDecoderContext->sample_rate*2))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        //return AVERROR(ENOMEM);
    }
    int frameCount=0;
    std::unique_lock write(n);
    write.unlock();
    std::unique_lock ul(m);
    int ret;
    while (!end) {
        // Mutual Exclusion, lock done at the end of the cycle
        //cv.wait(ul, [this]() { return this->capture; });

        ul.unlock();

        // Get packet
        if ((ret = av_read_frame(inputAudioFormatContext, inputPacket)) < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                ul.lock();
                continue;
            } else throw std::runtime_error("Error in reading packet from input context.");
        }
        //av_packet_rescale_ts(outputPacket, inputAudioFormatContext->streams[inAudioStreamIndex]->time_base, audioDecoderContext->time_base);
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
        /*if (outputFormatContext->streams[outAudioStreamIndex]->start_time <= 0) {
            outputFormatContext->streams[outAudioStreamIndex]->start_time = inputFrame->pts;
        }
        //init converted samples
        uint8_t **resampledData = nullptr;
        ret = av_samples_alloc_array_and_samples(&resampledData, NULL, audioEncoderContext->channels, inputFrame->nb_samples, requireAudioFmt, 0);
        if (ret < 0) {
            throw std::runtime_error("Fail to alloc samples by av_samples_alloc_array_and_samples.");
        }
        //convert samples
        swr_convert(resampleContext,
                    resampledData, inputFrame->nb_samples,
                    (const uint8_t**)inputFrame->extended_data, inputFrame->nb_samples);
        int x = av_audio_fifo_size(fifo);
        add_samples_to_fifo(fifo, resampledData, inputFrame->nb_samples);
        x = av_audio_fifo_size(fifo);

        outputFrame->nb_samples = audioEncoderContext->frame_size;
        outputFrame->channel_layout = audioEncoderContext->channel_layout;
        outputFrame->format = audioEncoderContext->sample_fmt;
        outputFrame->sample_rate = audioEncoderContext->sample_rate;
        av_frame_get_buffer(outputFrame, 0);
        */
        uint8_t **cSamples = nullptr;
        ret = av_samples_alloc_array_and_samples(&cSamples, NULL, audioEncoderContext->channels, inputFrame->nb_samples, requireAudioFmt, 0);
        if (ret < 0) {
            throw std::runtime_error("Fail to alloc samples by av_samples_alloc_array_and_samples.");
        }
        ret = swr_convert(resampleContext, cSamples, inputFrame->nb_samples, (const uint8_t**)inputFrame->extended_data, inputFrame->nb_samples);
        if (ret < 0) {
            throw std::runtime_error("Fail to swr_convert.");
        }
        if (av_audio_fifo_space(fifo) < inputFrame->nb_samples) throw std::runtime_error("audio buffer is too small.");

        ret = av_audio_fifo_write(fifo, (void**)cSamples, inputFrame->nb_samples);
        if (ret < 0) {
            throw std::runtime_error("Fail to write fifo");
        }

        av_freep(&cSamples[0]);

        av_frame_unref(inputFrame);
        av_packet_unref(inputPacket);

        while (av_audio_fifo_size(fifo) >= audioEncoderContext->frame_size){

            AVFrame* outputFrame = av_frame_alloc();

            outputFrame->nb_samples = audioEncoderContext->frame_size;
            outputFrame->channels = audioDecoderContext->channels;
            outputFrame->channel_layout = av_get_default_channel_layout(audioDecoderContext->channels);
            outputFrame->format = requireAudioFmt;
            outputFrame->sample_rate = audioEncoderContext->sample_rate;

            ret = av_frame_get_buffer(outputFrame, 0);
            if(ret < 0) throw std::runtime_error("Error in retrieving buffer.");
            ret = av_audio_fifo_read(fifo, (void**)outputFrame->data, audioEncoderContext->frame_size);
            if(ret < 0) throw std::runtime_error("Error in reading from fifo.");
            ul.lock();
            if(!this->capture) {
                ul.unlock();
                continue;
            }
            else ul.unlock();
            pts = outAudioStream->time_base.den  / audioEncoderContext->sample_rate * 1024 * frameCount;
            outputFrame->pts = pts;
            // Encoded inputFrame
            if ((avcodec_send_frame(audioEncoderContext, outputFrame)) < 0) {
                throw std::runtime_error("Error in encoding the frame."); //new
                if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
                    ul.lock();
                    continue;
                } else throw std::runtime_error("Error in encoding the frame.");
            }
            av_frame_free(&outputFrame);
            // Receive encoded packet
            if ((ret = avcodec_receive_packet(audioEncoderContext, outputPacket)) >= 0) {
                //outputPacket->stream_index = outAudioStreamIndex;

            } else if (ret == AVERROR(EAGAIN) /*|| ret == AVERROR_EOF*/)
                //break;

                continue;
            else
                throw std::runtime_error("Error in receiving the decoded audio packet.");
            //av_packet_rescale_ts(outputPacket, audioEncoderContext->time_base, outputFormatContext->streams[outAudioStreamIndex]->time_base);
            outputPacket->stream_index = outAudioStream->index;
            outputPacket->duration = outAudioStream->time_base.den * 1024 / audioEncoderContext->sample_rate;
            outputPacket->dts = pts;
            outputPacket->pts = pts;
            frameCount++;
            write.lock();
            if (av_interleaved_write_frame(outputFormatContext, outputPacket) < 0)
                throw std::runtime_error("Error in writing packet to output file.");
            write.unlock();


            av_packet_unref(outputPacket);

            ret = 0;
        }

        ul.lock();
    }

}

