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

/* initialize the resources*/
ScreenRecorder::ScreenRecorder() {
    //av_register_all();
    //avcodec_register_all();
    avdevice_register_all();
    cout<<"\nall required functions are registered successfully";
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder()
{
    avformat_close_input(&pAVFormatContext);
    if( !pAVFormatContext ) cout<<"\nfile closed sucessfully";
    else cout<<"\nunable to close the file";
    
    avformat_free_context(pAVFormatContext);
    if( !pAVFormatContext ) cout<<"\navformat free successfully";
    else cout<<"\nunable to free avformat context";
}

/* function to capture and store data in frames by allocating required memory and auto deallocating the memory.   */
int ScreenRecorder::CaptureVideoFrames() {
    value = 0;
    
    inputPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(inputPacket);
    
    pAVFrame = av_frame_alloc();
    if( !pAVFrame )
    {
        cout<<"\nunable to release the avframe resources";
        return 1;
    }
    
    SwsContext* swsCtx_ ;
    swsCtx_ = sws_getContext(pAVCodecContext->width,
                             pAVCodecContext->height,
                             pAVCodecContext->pix_fmt,
                             outAVCodecContext->width,
                             outAVCodecContext->height,
                             outAVCodecContext->pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);
    
    
    int ii = 0;
    int no_frames = 100;
    cout<<"\nenter No. of frames to capture : ";
    cin>>no_frames;
    
    AVFrame *out_frame = nullptr;
    out_frame = av_frame_alloc();
    out_frame->format = outAVCodecContext->pix_fmt;
    out_frame->width  = outAVCodecContext->width;
    out_frame->height = outAVCodecContext->height;
    av_frame_get_buffer(out_frame, 0);
    
    AVPacket *out_packet = av_packet_alloc();
    while(true) {
        int ret;
        if((ret = av_read_frame( pAVFormatContext , inputPacket )) < 0) break;
        if( ii++ > no_frames ) {
            av_packet_unref(out_packet);
            break;
        }

        //cout << "Row: " << ii << endl;
        
        if(inputPacket->stream_index == VideoStreamIndx) {
            ret = avcodec_send_packet(pAVCodecContext, inputPacket);
            if(ret < 0) {
                if(ret == AVERROR(EAGAIN)) cout << "AVERROR(EAGAIN)" << endl;
                else if(ret == AVERROR(EINVAL)) cout << "AVERROR(EINVAL)" << endl;
                
                return -1;
            }
            
            AVFrame *in_frame = av_frame_alloc();
            ret = avcodec_receive_frame(pAVCodecContext, in_frame);
            if(ret < 0) {
                if(ret == AVERROR(EAGAIN)) cout << "AVERROR(EAGAIN)" << endl;
                else if(ret == AVERROR(EINVAL)) cout << "AVERROR(EINVAL)" << endl;
                
                return -1;
            }
            
            if (ret < 0) {
                fprintf(stderr, "Could not allocate raw picture buffer\n");
                return -1;;
            }
            
            sws_scale(swsCtx_, in_frame->data, in_frame->linesize, 0, pAVCodecContext->height, out_frame->data,out_frame->linesize);
            
            ret = avcodec_send_frame(outAVCodecContext, out_frame);
            if(ret < 0) {
                if(ret == AVERROR(EAGAIN)) cout << "AVERROR(EAGAIN)" << endl;
                else if(ret == AVERROR(EINVAL)) cout << "AVERROR(EINVAL)" << endl;
                
                return -1;
            }
            
            while (true) {
                av_init_packet(out_packet);
                out_packet->data = nullptr;
                out_packet->size = 0;
                
                ret = avcodec_receive_packet(outAVCodecContext, out_packet);
                
                out_packet->pts = av_rescale_q(out_packet->pts, video_st->codec->time_base, video_st->time_base);

                out_packet->dts = av_rescale_q(out_packet->dts, video_st->codec->time_base, video_st->time_base);

                
                if(ret >= 0) {
                    // invece di av_write_frame
                    ret = av_interleaved_write_frame(outputFormatContext, out_packet);
                    if(ret < 0) {
                        cout << "Error in writing packet";
                        return -3;
                    }
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

/* establishing the connection between camera or screen through its respective folder */
int ScreenRecorder::openCamera() {
    options = NULL;
    pAVFormatContext = NULL;
    
    //av_register_all();
    avformat_network_init();
    pAVFormatContext = avformat_alloc_context();
    
    //Register Device
    avdevice_register_all();
    //Windows
#ifdef _WIN32
#if USE_DSHOW
    pAVInputFormat=av_find_input_format("dshow");
    if(avformat_open_input(&pAVFormatContext,"video=screen-capture-recorder",ifmt,NULL)!=0){
        printf("Couldn't open input stream.\n");
        return -1;
    }
#else
    AVDictionary* options = NULL;
    pAVInputFormat=av_find_input_format("gdigrab");
    if(avformat_open_input(&pAVFormatContext,"desktop",ifmt,&options)!=0){
        printf("Couldn't open input stream.\n");
        return -1;
    }
    
#endif
#elif defined linux
    AVDictionary* options = NULL;
    pAVInputFormat=av_find_input_format("x11grab");
    if(avformat_open_input(&pAVFormatContext,":0.0+10,20",ifmt,&options)!=0){
        printf("Couldn't open input stream.\n");
        return -1;
    }
#else
    show_avfoundation_device();
    pAVInputFormat=av_find_input_format("avfoundation");
    if(avformat_open_input(&pAVFormatContext,"1",pAVInputFormat,NULL)!=0){
        printf("Couldn't open input stream.\n");
        return -1;
    }
#endif
    
    if(av_dict_set( &options,"framerate","30",0 ) < 0) {
        cout<<"\nerror in setting dictionary value";
        return 1;
    }
    
    if(av_dict_set( &options, "preset", "medium", 0 ) < 0) {
        cout<<"\nerror in setting preset values";
        return 1;
    }
    
    AVDictionary *d = NULL;
    av_dict_set(&d, "probesize", "10M", 0); // add an entry
    if(avformat_find_stream_info(pAVFormatContext, &d) < 0) {
        cout<<"\nunable to find the stream information";
        return 1;
    }
    
    
    VideoStreamIndx = -1;
    
    /* find the first video stream index . Also there is an API available to do the below operations */
    for(int i = 0; i < pAVFormatContext->nb_streams; i++ ) {
        if( pAVFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ) {
            VideoStreamIndx = i;
            break;
        }
    }
    
    if( VideoStreamIndx == -1) {
        cout<<"\nunable to find the video stream index. (-1)";
        return 1;
    }
    
    // assign pAVFormatContext to VideoStreamIndx
    pAVCodecContext = pAVFormatContext->streams[VideoStreamIndx]->codec;
    
    decoder = avcodec_find_decoder(pAVCodecContext->codec_id);
    if( decoder == NULL ) {
        cout<<"\nunable to find the decoder";
        return 1;
    }
    
    value = avcodec_open2(pAVCodecContext , decoder , NULL);
    if( value < 0 ) {
        cout<<"\nunable to open the av codec";
        return 1;
    }
    
    return 0;
}

/* initialize the video output file and its properties  */
int ScreenRecorder::init_outputfile() {
    outputFormatContext = NULL;
    int value = 0;
    output_file = "/Users/checco/Downloads/output.mp4";
    
    avformat_alloc_output_context2(&outputFormatContext, NULL, NULL, output_file);
    if (!outputFormatContext) {
        cout<<"\nerror in allocating av format output context";
        return 1;
    }
    
    video_st = avformat_new_stream(outputFormatContext ,NULL);
    if( !video_st ) {
        cout<<"\nerror in creating a av format new stream";
        return 1;
    }
    
    outAVCodecContext = avcodec_alloc_context3(encoder);
    if( !outAVCodecContext )
    {
        cout<<"\nerror in allocating the codec contexts";
        return 1;
    }
    
    
    /* set property of the video file */
    outAVCodecContext = video_st->codec;
    outAVCodecContext->codec_id = AV_CODEC_ID_MPEG4;// AV_CODEC_ID_MPEG4; // AV_CODEC_ID_H264 // AV_CODEC_ID_MPEG1VIDEO
    outAVCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    outAVCodecContext->pix_fmt  = AV_PIX_FMT_YUV420P;
    outAVCodecContext->bit_rate = 400000; // 2500000
    outAVCodecContext->width = 2560;
    outAVCodecContext->height = 1600;
    outAVCodecContext->gop_size = 3;
    outAVCodecContext->max_b_frames = 1;
    outAVCodecContext->time_base = {1, 30};
    
    if (codec_id == AV_CODEC_ID_H264) {
        av_opt_set(outAVCodecContext->priv_data, "preset", "slow", 0);
    }
    
    encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if( !encoder )
    {
        cout<<"\nerror in finding the av codecs. try again with correct codec";
        return 1;
    }
    
    /* Some container formats (like MP4) require global headers to be present
     Mark the encoder so that it behaves accordingly. */
    if ( outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
    {
        outAVCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    value = avcodec_open2(outAVCodecContext, encoder, NULL);
    if( value < 0)
    {
        cout<<"\nerror in opening the avcodec";
        return 1;
    }
    
    /* create empty video file */
    if ( !(outputFormatContext->flags & AVFMT_NOFILE) )
    {
        if( avio_open2(&outputFormatContext->pb , output_file , AVIO_FLAG_WRITE ,NULL, NULL) < 0 )
        {
            cout<<"\nerror in creating the video file";
            return 1;
        }
    }
    
    if(!outputFormatContext->nb_streams) {
        cout<<"\noutput file dose not contain any stream";
        return 1;
    }
    
    /* imp: mp4 container or some advanced container file required header information*/
    value = avformat_write_header(outputFormatContext , 0);
    if(value < 0) {
        cout<<"\nerror in writing the header context";
        return 1;
    }
    
    return 0;
}

