//
//  ScreenRecorder.cpp
//  ScreenRecorder
//
//  Created by Checco on 15/10/2021.
//  Copyright © 2021 Checco. All rights reserved.
//

#include "ScreenRecorder.h"

using namespace std;

//Show AVFoundation Device
void show_avfoundation_device(){
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    AVDictionary* options = NULL;
    av_dict_set(&options,"list_devices","true",0);
    AVInputFormat *iformat = av_find_input_format("avfoundation");
    printf("==AVFoundation Device Info===\n");
    avformat_open_input(&pFormatCtx,"",iformat,&options);
    printf("=============================\n");
}

int openInput(AVFormatContext *formatContext, AVDictionary *options) {
    AVInputFormat *inputFormat = nullptr;
    
    #ifdef _WIN32
    #if USE_DSHOW
        inputFormat=av_find_input_format("dshow");
        return avformat_open_input(&formatContext,"video=screen-capture-recorder",inputFormat,NULL)!=0);
    #else
        inputFormat = av_find_input_format("gdigrab");
        return avformat_open_input(&formatContext,"desktop",inputFormat,&options);
    #endif
    #elif defined linux
        inputFormat=av_find_input_format("x11grab");
        return avformat_open_input(&formatContext,":0.0+10,20",inputFormat,&options);
    #else
        show_avfoundation_device();
        inputFormat=av_find_input_format("avfoundation");
    return avformat_open_input(&formatContext, "1", inputFormat, &options);
    #endif
}

/* initialize the resources*/
ScreenRecorder::ScreenRecorder() {
    inputFormatContext = NULL;
    outputFormatContext = NULL;
    
    decoder = nullptr;

    avdevice_register_all();
    avformat_network_init();
    avdevice_register_all();

    inputFormatContext = avformat_alloc_context();
    
    if(openInput(inputFormatContext, nullptr) != 0 ) {
        // throw ex
        std::cout << "Error in opening input" << std::endl;
    }
    
    inputFormatContext->probesize = 50000000;
    
    AVDictionary *options = nullptr;
    if(avformat_find_stream_info(inputFormatContext, &options) < 0) {
        // throw ex
        std::cout<<"\nunable to find the stream information";
    }
    
    VideoStreamIndx = av_find_best_stream(inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if( VideoStreamIndx == -1) {
        // throw ex
        std::cout<<"\nunable to find the video stream index. (-1)";
    }
    
    decoder = avcodec_find_decoder(inputFormatContext->streams[VideoStreamIndx]->codecpar->codec_id);
    decoderContext = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoderContext, inputFormatContext->streams[VideoStreamIndx]->codecpar);
    if( avcodec_open2(decoderContext, decoder, NULL) < 0 ) {
        // throw ex
        std::cout<<"\nunable to open the av codec";
    }
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder() {
    avformat_close_input(&inputFormatContext);
    if( !inputFormatContext ) cout<<"\nfile closed successfully";
    else cout<<"\nunable to close the file";
    
    avformat_free_context(inputFormatContext);
    if( !inputFormatContext ) cout<<"\navformat free successfully";
    else cout<<"\nunable to free avformat context";
}

void ScreenRecorder::Start() {
    /* Init output file */
    avformat_alloc_output_context2(&outputFormatContext, NULL, NULL, filename.c_str());
    if (!outputFormatContext) {
        // throw ex
        cout<<"\nerror in allocating av format output context";
    }
    
    //create AVStream
    video_st = avformat_new_stream(outputFormatContext ,NULL);
    if( !video_st ) {
        // throw ex
        cout<<"\nerror in creating a av format new stream";
    }
    encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if( !encoder )
    {
        // throw ex
        cout<<"\nerror in finding the av codecs. try again with correct codec";
    }
    
    //alloc AVCodecContext
    encoderContext = avcodec_alloc_context3(encoder);
    if( !encoderContext ) {
        // throw ex
        cout<<"\nerror in allocating th codec contexts";
    }

    video_st->codecpar->codec_id = AV_CODEC_ID_MPEG4;
    video_st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    video_st->codecpar->bit_rate = 9000*1024*2; // 2500000
    video_st->codecpar->width = width;
    video_st->codecpar->height = height;
    video_st->time_base = {1, 15};

    avcodec_parameters_to_context(encoderContext, video_st->codecpar);
    
    /* set property of the video file */
    encoderContext->pix_fmt  = AV_PIX_FMT_YUV420P;
    encoderContext->time_base = {1, 15};

    /* Some container formats (like MP4) require global headers to be present
     Mark the encoder so that it behaves accordingly. */
    if ( outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    
    if( avcodec_open2(encoderContext, encoder, NULL) < 0) {
        cout<<"\nerror in opening the avcodec";
    }
    
    /* create empty video file */
    if ( !(outputFormatContext->flags & AVFMT_NOFILE) ) {
        if( avio_open2(&outputFormatContext->pb , filename.c_str() , AVIO_FLAG_WRITE ,NULL, NULL) < 0 ) {
            cout<<"\nerror in creating the video file";
        }
    }
    
    if(!outputFormatContext->nb_streams) cout<<"\noutput file dose not contain any stream";
    
    /* imp: mp4 container or some advanced container file required header information*/
    if(avformat_write_header(outputFormatContext , 0) < 0) {
        cout<<"\nerror in writing the header context";
    }
    
    CaptureVideoFrames();
}

//int ScreenRecorder::init_outputfile() {
//    return 0;
//}

/* function to capture and store data in frames by allocating required memory and auto deallocating the memory.   */
int ScreenRecorder::CaptureVideoFrames() {
    int value = 0;

    SwsContext* swsCtx_ ;
    swsCtx_ = sws_getContext(decoderContext->width,
                             decoderContext->height,
                             decoderContext->pix_fmt,
                             encoderContext->width,
                             encoderContext->height,
                             encoderContext->pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);


    int ii = 0;
    int no_frames = 100;
    cout<<"\nenter No. of frames to capture : ";
    cin>>no_frames;

    AVFrame *out_frame = nullptr;
    out_frame = av_frame_alloc();
    out_frame->format = encoderContext->pix_fmt;
    out_frame->width  = encoderContext->width;
    out_frame->height = encoderContext->height;
    av_frame_get_buffer(out_frame, 0);

    AVPacket *inputPacket = av_packet_alloc();

    AVPacket *out_packet = av_packet_alloc();
    int ret = av_read_frame( inputFormatContext , inputPacket );
    while(ii < no_frames) {
        if((ret = av_read_frame( inputFormatContext , inputPacket )) < 0) {
            if(ret == AVERROR(EAGAIN)) continue;
            else break;
        }

        //cout << "Row: " << ii << endl;
        int ret;
        if(inputPacket->stream_index == VideoStreamIndx) {
            ret = avcodec_send_packet(decoderContext, inputPacket);
            if(ret < 0) {
                if(ret == AVERROR(EAGAIN)) cout << "AVERROR(EAGAIN)" << endl;
                else if(ret == AVERROR(EINVAL)) cout << "AVERROR(EINVAL)" << endl;

                return -1;
            }

            AVFrame *in_frame = av_frame_alloc();
            ret = avcodec_receive_frame(decoderContext, in_frame);
            if(ret < 0) {
                if(ret == AVERROR(EAGAIN)) cout << "AVERROR(EAGAIN)" << endl;
                else if(ret == AVERROR(EINVAL)) cout << "AVERROR(EINVAL)" << endl;

                return -1;
            }

            if (ret < 0) {
                fprintf(stderr, "Could not allocate raw picture buffer\n");
                return -1;;
            }

            sws_scale(swsCtx_, in_frame->data, in_frame->linesize, 0, decoderContext->height, out_frame->data,out_frame->linesize);

            ret = avcodec_send_frame(encoderContext, out_frame);
            if(ret < 0) {
                if(ret == AVERROR(EAGAIN)) cout << "AVERROR(EAGAIN)" << endl;
                else if(ret == AVERROR(EINVAL)) cout << "AVERROR(EINVAL)" << endl;

                return -1;
            }

            while (true) {
                out_packet = av_packet_alloc();
                out_packet->data = nullptr;
                out_packet->size = 0;

                ret = avcodec_receive_packet(encoderContext, out_packet);

                out_packet->pts = av_rescale_q(out_packet->pts, encoderContext->time_base, video_st->time_base);

                out_packet->dts = av_rescale_q(out_packet->dts, encoderContext->time_base, video_st->time_base);


                if(ret >= 0) {
                    // invece di av_write_frame
                    ret = av_interleaved_write_frame(outputFormatContext, out_packet);
                    if(ret < 0) {
                        cout << "Error in writing packet";
                        return -3;
                    }
                    ii++;
                    av_packet_unref(out_packet);
                    break;
                } else if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret == AVERROR(EINVAL)) {
                    cout << "AVERROR(EINVAL)" << endl;
                    return -1;
                } else {
                    cout << "Undefined error" << endl;
                    return -2;
                }
            }
        }
    }
    value = av_write_trailer(outputFormatContext);
    if( value < 0) {
        cout<<"\nerror in writing av trailer";
        return -1;
    }

    avformat_free_context(outputFormatContext);

    return 0;
}
