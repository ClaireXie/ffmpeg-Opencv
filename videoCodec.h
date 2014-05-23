#define __STDC_CONSTANT_MACROS

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
	#include <libavutil/opt.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/common.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/samplefmt.h>
}

#include <math.h>
#include <stdio.h>
#include "timer.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"


#define inline __inline
#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

#define CVW 1024
#define CVH 540

#define AVW 1024
#define AVH 540

class VideoCodec
{
public:

	AVCodec *codec;
    AVCodecContext *c;
	SwsContext *swsCtx;
    AVFrame *frame, *frameRescaled;
    AVPacket pkt;
	AVCodecID  codec_id;
	bool encoder;

	cv::Mat cvFrame;
	int cvWidth, cvHeight;
	int avWidth, avHeight;

	AVPixelFormat codecFormat, orgFormat;

	VideoCodec() {};
	~VideoCodec() {};
	bool initBase(bool isEncoder);
	void destory();
};

class VideoEncode : public VideoCodec{

public:
	VideoEncode() {};
	VideoEncode(AVCodecID id);
	bool loadcvFrame(cv::Mat input);
	bool init(int w, int h);
	int encode(cv::Mat input, int index, unsigned char cdata[]);
	~VideoEncode() {destory();}
	bool init();
};

class VideoDecode : public VideoCodec{

public:
	VideoDecode() {};
	VideoDecode(AVCodecID id);
	~VideoDecode(){destory();}
	bool init(int w, int h);
	int decode(cv::Mat &output, const unsigned char cdata[], int pkgSize);
};

