#include "videoCodec.h"


void VideoCodec::destory()
{
	avcodec_close(c);
    av_free(c);
	av_free(frame);
    av_freep(&frameRescaled->data[0]);
    avcodec_free_frame(&frameRescaled);
}

bool VideoCodec::initBase(bool isEncoder)
{
	
	encoder = isEncoder;

	orgFormat = PIX_FMT_RGB24;

	frame = avcodec_alloc_frame();
	
	frame->format = orgFormat;
	frameRescaled = avcodec_alloc_frame();

	avWidth = AVW;  
	avHeight = AVH; 

	if (codec_id == AV_CODEC_ID_MJPEG) {
		codecFormat = AV_PIX_FMT_YUVJ420P;
	}
	else if (codec_id == AV_CODEC_ID_H264) {
		codecFormat = AV_PIX_FMT_YUV420P;
	}
	
	if(encoder) {
		codec = avcodec_find_encoder(codec_id);
	}
	else {
		codec = avcodec_find_decoder(codec_id);
	}

	if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return false;
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return false;
    }

	return true;
}

VideoEncode::VideoEncode(AVCodecID id)
{
	encoder = 1;
	codec_id =  id;
	c= NULL;
}

bool VideoEncode::init(int w, int h)
{
	avcodec_register_all();

	if (!initBase(encoder)) return false;

    c->bit_rate = 6500000;
	c->width = avWidth;
    c->height = avHeight;
    /* frames per second */
	AVRational fps;
	fps.num=1;
	fps.den=10;
    c->time_base = fps;
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 5;
	c->pix_fmt = codecFormat;
	 
	if (codec_id == AV_CODEC_ID_H264) {
        av_opt_set(c->priv_data, "preset", "medium", 0);
		av_opt_set(c->priv_data, "tune", "zerolatency", 0); 
		c->max_b_frames = 1;
	}

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return false;
    }

	cvWidth = w;
	cvHeight = h;

	int ret = av_image_alloc(frameRescaled->data, frameRescaled->linesize, c->width, c->height,
                        c->pix_fmt, 16);

	if (ret<0) {
		fprintf(stderr, "Could not allocate image\n");
        return false;
	}

	swsCtx = sws_getContext(cvWidth, cvHeight, orgFormat, avWidth, 
		avHeight,  AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	return true;
}

bool VideoEncode::loadcvFrame(cv::Mat input)
{
	if (input.cols != avWidth || input.rows != avHeight) {
		cv::resize(input, cvFrame, cv::Size(avWidth, avHeight));
	}
	else cvFrame = input;

	frame->width = cvFrame.cols;
    frame->height = cvFrame.rows;
	
	if (input.channels() != 3)
		cvtColor(cvFrame, cvFrame, CV_GRAY2BGR);

	avpicture_fill((AVPicture *)frame, cvFrame.data, orgFormat, cvFrame.cols, cvFrame.rows);

	// rescale to outStream format
    int h = sws_scale(swsCtx, ((AVPicture*)frame)->data, ((AVPicture*)frame)->linesize, 
		0, c->height, ((AVPicture*)frameRescaled)->data, ((AVPicture*)frameRescaled) ->linesize);

	if (h ==0) {
		fprintf(stderr, "Fail to rescale\n");
        return false;
	}

	return true;
}

int VideoEncode::encode(cv::Mat input, int index, unsigned char cdata[])
{
	
	int size;

	if (!loadcvFrame(input)) 
		return -1;

	frameRescaled->pts = index;
	frameRescaled->format = codecFormat;

	av_init_packet(&pkt);
    pkt.data = NULL;    // packet data will be allocated by the encoder
    pkt.size = 0;

    /* encode the image */
	int got_output;
    int ret = avcodec_encode_video2(c, &pkt, frameRescaled, &got_output);
    if (ret < 0) {
        fprintf(stderr, "Error encoding frame\n");
        return -1;
    }
	if (got_output) {
		size = pkt.size;
		memcpy ( cdata, pkt.data, pkt.size*sizeof(unsigned char));
		//fprintf(stderr, "Encoding %d bytes\n", size);
	}

	av_free_packet(&pkt);

	return size;
}

VideoDecode::VideoDecode(AVCodecID id)
{
	encoder = 0;
	codec_id =  id;
	c= NULL;
}

bool VideoDecode::init(int w, int h)
{
	avcodec_register_all();

	if (!initBase(encoder)) return false;

	c->pix_fmt = codecFormat;
    
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return false;
    }

	cvWidth = w;
	cvHeight = h;

	int ret = av_image_alloc(frameRescaled->data, frameRescaled->linesize, cvWidth, cvHeight,
                        orgFormat, 16);

	if (ret<0) {
		fprintf(stderr, "Could not allocate image\n");
        return false;
	}

	swsCtx = sws_getContext(avWidth, avHeight, AV_PIX_FMT_YUV420P, cvWidth, cvHeight, 
		orgFormat, SWS_BICUBIC, NULL, NULL, NULL);

	return true;
}

int VideoDecode::decode(cv::Mat &output, const unsigned char cdata[], int pkgSize)
{
	
	frameRescaled->format = orgFormat;

	av_init_packet(&pkt);
    pkt.data = (uint8_t*) cdata;    // packet data will be allocated by the encoder
    pkt.size = pkgSize;

    /* encode the image */
	int got_frame;
    int len = avcodec_decode_video2(c, frame, &got_frame, &pkt);
    if (len < 0) {
        fprintf(stderr, "Error decoding frame\n");
        return -1;
    }
	if (got_frame) {
		int h = sws_scale(swsCtx, frame->data, frame->linesize, 0, c->height, 
					frameRescaled->data, frameRescaled ->linesize);

		output = cv::Mat(cvHeight, cvWidth, CV_8UC3, frameRescaled->data[0]); 
	}

	av_free_packet(&pkt);

	return len;
}



