#include <iostream>
#include <thread>
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}
using namespace std;
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"swresample.lib")
static double r2d(AVRational r)
{
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}
void msSleep(int ms)
{
	chrono::milliseconds du(ms);
	this_thread::sleep_for(du);
}
int main(int argc, char *argv[])
{
	cout << "Test Demux FFmpeg.club" << endl;
	const char *path = "music_mv.mp4";

	//��������
	AVDictionary *opts = NULL;
	//����rtsp����tcpЭ���
	av_dict_set(&opts, "rtsp_transport", "tcp", 0);

	//������ʱʱ��
	av_dict_set(&opts, "max_delay", "500", 0);


	//���װ������
	AVFormatContext *ic = NULL;
	int re = avformat_open_input(
		&ic,
		path,
		0,  // 0��ʾ�Զ�ѡ������
		&opts //�������ã�����rtsp����ʱʱ��
		);
	if (re != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(re, buf, sizeof(buf) - 1);
		cout << "open " << path << " failed! :" << buf << endl;
		getchar();
		return -1;
	}
	cout << "open " << path << " success! " << endl;

	//��ȡ����Ϣ 
	re = avformat_find_stream_info(ic, 0);

	//��ʱ�� ����
	int totalMs = ic->duration / (AV_TIME_BASE / 1000);
	cout << "totalMs = " << totalMs << endl;

	//��ӡ��Ƶ����ϸ��Ϣ
	av_dump_format(ic, 0, path, 0);

	//����Ƶ��������ȡʱ��������Ƶ
	int videoStream = 0;
	int audioStream = 1;

	//��ȡ����Ƶ����Ϣ ��������������ȡ��
	for (int i = 0; i < ic->nb_streams; i++)
	{
		AVStream *as = ic->streams[i];
		cout << "codec_id = " << as->codecpar->codec_id << endl;
		cout << "format = " << as->codecpar->format << endl;

		//��Ƶ AVMEDIA_TYPE_AUDIO
		if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioStream = i;
			cout << i << "��Ƶ��Ϣ" << endl;
			cout << "sample_rate = " << as->codecpar->sample_rate << endl;
			//AVSampleFormat;
			cout << "channels = " << as->codecpar->channels << endl;
			//һ֡���ݣ��� ��ͨ�������� 
			cout << "frame_size = " << as->codecpar->frame_size << endl;
			//1024 * 2 * 2 = 4096  fps = sample_rate/frame_size

		}
		//��Ƶ AVMEDIA_TYPE_VIDEO
		else if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream = i;
			cout << i << "��Ƶ��Ϣ" << endl;
			cout << "width=" << as->codecpar->width << endl;
			cout << "height=" << as->codecpar->height << endl;
			//֡�� fps ����ת��
			cout << "video fps = " << r2d(as->avg_frame_rate) << endl;
		}
	}

	//��ȡ��Ƶ��
	videoStream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

	//////////////////////////////////////////////////////////
	///��Ƶ��������
	///�ҵ���Ƶ������
	AVCodec *vcodec = avcodec_find_decoder(ic->streams[videoStream]->codecpar->codec_id);
	if (!vcodec)
	{
		cout << "can't find the codec id " << ic->streams[videoStream]->codecpar->codec_id;
		getchar();
		return -1;
	}
	cout << "find the AVCodec " << ic->streams[videoStream]->codecpar->codec_id << endl;

	AVCodecContext *vc = avcodec_alloc_context3(vcodec);

	///���ý����������Ĳ���
	avcodec_parameters_to_context(vc, ic->streams[videoStream]->codecpar);
	//���߳̽���
	vc->thread_count = 8;

	///�򿪽�����������
	re = avcodec_open2(vc, 0, 0);
	if (re != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(re, buf, sizeof(buf) - 1);
		cout << "avcodec_open2  failed! :" << buf << endl;
		getchar();
		return -1;
	}
	cout << "video avcodec_open2 success!" << endl;


	//////////////////////////////////////////////////////////
	///��Ƶ��������
	AVCodec *acodec = avcodec_find_decoder(ic->streams[audioStream]->codecpar->codec_id);
	if (!acodec)
	{
		cout << "can't find the codec id " << ic->streams[audioStream]->codecpar->codec_id;
		getchar();
		return -1;
	}
	cout << "find the AVCodec " << ic->streams[audioStream]->codecpar->codec_id << endl;
	///������������������
	AVCodecContext *ac = avcodec_alloc_context3(acodec);

	///���ý����������Ĳ���
	avcodec_parameters_to_context(ac, ic->streams[audioStream]->codecpar);
	//���߳̽���
	ac->thread_count = 8;

	///�򿪽�����������
	re = avcodec_open2(ac, 0, 0);
	if (re != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(re, buf, sizeof(buf) - 1);
		cout << "avcodec_open2  failed! :" << buf << endl;
		getchar();
		return -1;
	}
	cout << "audio avcodec_open2 success!" << endl;

	///ic->streams[videoStream]
	//malloc AVPacket����ʼ��
	AVPacket *pkt = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();

	//���ظ�ʽ�ͳߴ�ת��������
	SwsContext *vctx = NULL;
	unsigned char *rgb = NULL;

	//��Ƶ�ز��� �����ĳ�ʼ��
	SwrContext *actx = swr_alloc();
	actx = swr_alloc_set_opts(actx,
		av_get_default_channel_layout(2),	//�����ʽ
		AV_SAMPLE_FMT_S16,					//���������ʽ
		ac->sample_rate,					//���������
		av_get_default_channel_layout(ac->channels),//�����ʽ
		ac->sample_fmt,
		ac->sample_rate,
		0, 0
		);
	re = swr_init(actx);
	if (re != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(re, buf, sizeof(buf) - 1);
		cout << "swr_init  failed! :" << buf << endl;
		getchar();
		return -1;
	}
	unsigned char *pcm = NULL;


	for (;;)
	{
		int re = av_read_frame(ic, pkt);
		if (re != 0)
		{
			//ѭ������
			cout << "==============================end==============================" << endl;
			int ms = 3000; //����λ�� ����ʱ�������������ת��
			long long pos = (double)ms / (double)1000 * r2d(ic->streams[pkt->stream_index]->time_base);
			av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
			continue;
		}
		cout << "pkt->size = " << pkt->size << endl;
		//��ʾ��ʱ��
		cout << "pkt->pts = " << pkt->pts << endl;

		//ת��Ϊ���룬������ͬ��
		cout << "pkt->pts ms = " << pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000) << endl;



		//����ʱ��
		cout << "pkt->dts = " << pkt->dts << endl;

		AVCodecContext *cc = 0;
		if (pkt->stream_index == videoStream)
		{
			cout << "ͼ��" << endl;
			cc = vc;


		}
		if (pkt->stream_index == audioStream)
		{
			cout << "��Ƶ" << endl;
			cc = ac;
		}

		///������Ƶ
		//����packet�������߳�  send��NULL����ö��receiveȡ�����л���֡
		re = avcodec_send_packet(cc, pkt);
		//�ͷţ����ü���-1 Ϊ0�ͷſռ�
		av_packet_unref(pkt);

		if (re != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(re, buf, sizeof(buf) - 1);
			cout << "avcodec_send_packet  failed! :" << buf << endl;
			continue;
		}

		for (;;)
		{
			//���߳��л�ȡ����ӿ�,һ��send���ܶ�Ӧ���receive
			re = avcodec_receive_frame(cc, frame);
			if (re != 0) break;
			cout << "recv frame " << frame->format << " " << frame->linesize[0] << endl;

			//��Ƶ
			if (cc == vc)
			{

				vctx = sws_getCachedContext(
					vctx,	//��NULL���´���
					frame->width, frame->height,		//����Ŀ��
					(AVPixelFormat)frame->format,	//�����ʽ YUV420p
					frame->width, frame->height,	//����Ŀ��
					AV_PIX_FMT_RGBA,				//�����ʽRGBA
					SWS_BILINEAR,					//�ߴ�仯���㷨
					0, 0, 0);
				//if(vctx)
				//cout << "���ظ�ʽ�ߴ�ת�������Ĵ������߻�ȡ�ɹ���" << endl;
				//else
				//	cout << "���ظ�ʽ�ߴ�ת�������Ĵ������߻�ȡʧ�ܣ�" << endl;
				if (vctx)
				{
					if (!rgb) rgb = new unsigned char[frame->width*frame->height * 4];
					uint8_t *data[2] = { 0 };
					data[0] = rgb;
					int lines[2] = { 0 };
					lines[0] = frame->width * 4;
					re = sws_scale(vctx,
						frame->data,		//��������
						frame->linesize,	//�����д�С
						0,
						frame->height,		//����߶�
						data,				//������ݺʹ�С
						lines
						);
					cout << "sws_scale = " << re << endl;
				}

			}
			else //��Ƶ
			{
				uint8_t *data[2] = { 0 };
				if (!pcm) pcm = new uint8_t[frame->nb_samples * 2 * 2];
				data[0] = pcm;
				re = swr_convert(actx,
					data, frame->nb_samples,		//���
					(const uint8_t**)frame->data, frame->nb_samples	//����
					);
				cout << "swr_convert = " << re << endl;
			}

		}
		msSleep(500);
	}
	av_frame_free(&frame);
	av_packet_free(&pkt);



	if (ic)
	{
		//�ͷŷ�װ�����ģ����Ұ�ic��0
		avformat_close_input(&ic);
	}

	getchar();
	return 0;
}