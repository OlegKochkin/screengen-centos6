#include "qffmpeg.h"

// Open videofile, codecs init, get streams info
TFfmpeg::TFfmpeg(QString videoFile, int logLevel){
	error=0;
	av_log_set_level(logLevel);
	avcodec_register_all();
	av_register_all();

	pAVFormatContext = avformat_alloc_context();
	if (avformat_open_input (&pAVFormatContext, videoFile.toLocal8Bit(), NULL, NULL)!=0){
		QTextStream(stdout)<<"Error open file: "<<videoFile<<"\n";
		error=1;
		return;
		}
	else if (avformat_find_stream_info (pAVFormatContext, NULL) < 0){
		QTextStream(stdout)<<"Not found streams in "<<videoFile<<"\n";
		error=1;
		return;
		}

// Duration
	duration = QTime(0,0,0,0).addMSecs (pAVFormatContext->duration / 1000).toString("hh:mm:ss");
// Full bitrate for all streams
	fullBitrate = QString::number (pAVFormatContext->bit_rate / 1000);
// Streams count
	streamsCount = pAVFormatContext->nb_streams;
// Main parameters string
	generalInfo = duration+", "+QObject::tr("Streams:")+" "+QString::number (streamsCount)+", "+QObject::tr("Full bitrate:")+" "+fullBitrate+" "+QObject::tr("kb/s");

// Check video stream exists and get his number
	vStreamNumber=-1;
	for (uint i=0; i < pAVFormatContext->nb_streams;i++)
		if (pAVFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			vStreamNumber=i;
			break;
			}
	if (vStreamNumber == -1){
		QTextStream(stdout)<<"Not video stream in "<<videoFile<<"\n";
		error=1;
		return;
		}

// Check codec exists for video stream
	AVStream *vStream = pAVFormatContext->streams[vStreamNumber];
	vCodecContext = vStream->codec;
	vCodec = avcodec_find_decoder (vCodecContext->codec_id);
	if (avcodec_open2 (vCodecContext, vCodec, NULL)){
		QTextStream(stdout)<<"Not open video codec in "<<videoFile<<"\n";
		error=1;
		return;
		}
	if (logLevel != -8) QTextStream(stdout)<<"Used libAVCodec version: "<<LIBAVCODEC_VERSION_MAJOR<<"."<<LIBAVCODEC_VERSION_MINOR<<"."<<LIBAVCODEC_VERSION_MICRO<<"\n";

// FPS
	vFps = "";
	if (vStream->avg_frame_rate.den && vStream->avg_frame_rate.num) vFps = QString::number ((double) av_q2d (vStream->avg_frame_rate));
	else if (vStream->r_frame_rate.den && vStream->r_frame_rate.num) vFps = QString::number ((double) av_q2d (vStream->r_frame_rate));
// Video codec
	vCodecName = QString(vCodec->name).toUpper();
// Size source frame
	vWidth = vCodecContext->width;
	vHeight = vCodecContext->height;
// Size source frame in text
	vResolution = QString::number (vWidth) + "x" + QString::number (vHeight);
// Duration
	iDuration = pAVFormatContext->duration;
// Calculate DAR (Display Aspect Ratio), frame aspect
	dar = double (vWidth) / double (vHeight);
// Bitrate
		vBitrate = QString::number (get_bit_rate (vCodecContext)/1000);
	if (vStream->sample_aspect_ratio.num && av_cmp_q(vStream->sample_aspect_ratio, vStream->codec->sample_aspect_ratio)){
		AVRational display_aspect_ratio;
		av_reduce (&display_aspect_ratio.num, &display_aspect_ratio.den, vWidth * vStream->sample_aspect_ratio.num, vHeight * vStream->sample_aspect_ratio.den, 1024*1024);
		dar = double (display_aspect_ratio.num) / double (display_aspect_ratio.den);
		} else {
			if (vCodecContext->sample_aspect_ratio.num) {
			AVRational display_aspect_ratio;
			av_reduce (&display_aspect_ratio.num, &display_aspect_ratio.den, vWidth * vCodecContext->sample_aspect_ratio.num, vHeight * vCodecContext->sample_aspect_ratio.den, 1024 * 1024);
			dar = double (display_aspect_ratio.num) / double (display_aspect_ratio.den);
			}
		}
		
// Check audio stream exists and get his number
	aStreamNumber=-1;
	for (uint i=0; i < pAVFormatContext->nb_streams;i++)
		if (pAVFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			aStreamNumber=i;
			break;
			}
	if (aStreamNumber == -1) QTextStream(stdout)<<"Not audio stream in\n"<<videoFile<<endl;
	else{
		aCodecContext = pAVFormatContext->streams[aStreamNumber]->codec;
		aCodec = avcodec_find_decoder (aCodecContext->codec_id);
		if (avcodec_open2 (aCodecContext, aCodec, NULL)) QTextStream(stdout)<<"Not open audio codec in"<<videoFile<<"\n"<<endl;
		}

	if (vCodecContext->codec_tag > 0){
		char vCodecTagC[32];
		av_get_codec_tag_string (vCodecTagC, sizeof(vCodecTagC), vCodecContext->codec_tag);
		vCodecTag = QString (vCodecTagC);
		}
	char aChC[32];
	if (aStreamNumber != -1){
		av_get_channel_layout_string (aChC, sizeof(aChC), aCodecContext->channels, aCodecContext->channel_layout);
// Channels count
		aChannels = QString (aChC);
// Freq
		aSamples = QString::number ((double)aCodecContext->sample_rate / 1000);
// Bitrate
		aBitrate = QString::number (get_bit_rate (aCodecContext)/1000);
// Codec
		aCodecName = QString(aCodec->name).toUpper();
		}
	}

// Calculate stream bitrate (only audio worked)
int TFfmpeg::get_bit_rate(AVCodecContext *ctx){
	int bit_rate = 0;
	int bits_per_sample;

	switch(ctx->codec_type) {
		case AVMEDIA_TYPE_VIDEO:
			bit_rate = ctx->bit_rate;
			break;
		case AVMEDIA_TYPE_DATA:
		case AVMEDIA_TYPE_SUBTITLE:
		case AVMEDIA_TYPE_ATTACHMENT:
			bit_rate = ctx->bit_rate;
			break;
		case AVMEDIA_TYPE_AUDIO:
			bits_per_sample = av_get_bits_per_sample(ctx->codec_id);
			if (bits_per_sample) bit_rate = ctx->sample_rate * ctx->channels * bits_per_sample;
				else bit_rate = ctx->bit_rate;
			break;
		default:
			break;
		}
	return bit_rate;
}

// Get string about video stream
QString TFfmpeg::getVInfo(){
	QString vct = ", ";
	if ((vCodecContext->codec_tag > 0) &&
		(vCodecTag != vCodecName) &&
		(vCodecTag.mid(0,1) != "[") &&
		(vCodecTag.mid (vCodecTag.length()-1,1) != "]"))
			vct = " ("+vCodecTag+"), ";
	QString sFps = ", " + vFps + " " + QObject::tr("fps");
	if (vFps == "") sFps = "";
	QString br = "";
	if (vBitrate != "0") br = ", " + vBitrate + " " + QObject::tr("kb/s") + " ";
	return vCodecName + vct + vResolution + br + sFps;
}

// Get string about audio stream
QString TFfmpeg::getAInfo(){
	QString ret="";
	if (aStreamNumber != -1){
		QString br = "";
		if (aBitrate != "0") br = ", " + aBitrate + " " + QObject::tr("kb/s");
		ret = aCodecName + ", " + aChannels.toUpper() + ", " + aSamples + " " + QObject::tr("kHz") + br;
		}
	else ret = QObject::tr("Not present");
	return ret;
	}

// Close videofile
TFfmpeg::~TFfmpeg(){
	avformat_close_input (&pAVFormatContext);
}

// Get picture QImage from video frame in position "ms" in miliseconds
QImage TFfmpeg::getImage(uint ms){
	QImage img = QImage(vCodecContext->width, vCodecContext->height, QImage::Format_RGB888);
	AVFrame *pFrame, *pFrameRGB;
	AVPacket packet;
	int numBytes;
	uint8_t *buffer;
	int frameFinished;

// Deprecated function "avcodec_alloc_frame" change to "av_frame_alloc" from libavcodec version 55.45.101
	#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 45, 101)
		pFrame = av_frame_alloc();
		pFrameRGB = av_frame_alloc();
	#else
		pFrame = avcodec_alloc_frame();
		pFrameRGB = avcodec_alloc_frame();
	#endif

	numBytes = avpicture_get_size (AV_PIX_FMT_RGB24, vCodecContext->width, vCodecContext->height);
	buffer = (uint8_t *) av_malloc (numBytes);
	avpicture_fill ((AVPicture *) pFrameRGB, buffer, AV_PIX_FMT_RGB24,	vCodecContext->width, vCodecContext->height);

	int frameNum = av_rescale (ms,pAVFormatContext->streams[vStreamNumber]->time_base.den, pAVFormatContext->streams[vStreamNumber]->time_base.num) / 1000;
	avformat_seek_file (pAVFormatContext, vStreamNumber, 0, frameNum, frameNum, AVSEEK_FLAG_FRAME);
	avcodec_flush_buffers (vCodecContext);

	while (av_read_frame (pAVFormatContext, &packet) >= 0){
		if (packet.stream_index == vStreamNumber){
			avcodec_decode_video2 (vCodecContext, pFrame, &frameFinished, &packet);
			if (frameFinished){
				struct SwsContext *context;
				context = sws_getCachedContext (0, vCodecContext->width, vCodecContext->height, vCodecContext->pix_fmt, vCodecContext->width,vCodecContext->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, 0, 0, 0);
				sws_scale (context, (uint8_t**) pFrame->data, pFrame->linesize, 0, vCodecContext->height, pFrameRGB->data, pFrameRGB->linesize);
				for (int j=0; j<vCodecContext->height; j++) memcpy (img.scanLine (j), pFrameRGB->data[0] + j * pFrameRGB->linesize[0], vCodecContext->width * 3);
				break;
				}
			}
		av_packet_unref  (&packet);
		}
	av_frame_free (&pFrame);
	return img;
}
