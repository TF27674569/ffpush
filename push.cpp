#include <iostream>
using namespace std;

//����ͷ�ļ�
extern "C"
{
#include "libavformat/avformat.h"
	//����ʱ��
#include "libavutil/time.h"
}

//�����
#pragma comment(lib,"avformat.lib")
//���߿⣬������ȡ������Ϣ��
#pragma comment(lib,"avutil.lib")
//�����Ŀ�
#pragma comment(lib,"avcodec.lib")



static double r2d(AVRational r)
{
	return r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den;
}



int main(){

	//���д���ִ��֮ǰҪ����av_register_all��avformat_network_init
	//��ʼ�����еķ�װ�ͽ��װ flv mp4 mp3 mov������������ͽ���
	av_register_all();

	//��ʼ�������
	avformat_network_init();

	//ʹ�õ����·����.exeĿ¼�·ŵ�.exeĿ¼�¼���
	const char *inUrl = "E:\\360MoveData\\Users\\ubt\\Desktop\\input.mp4";
	//����ĵ�ַ
	const char *outUrl = "rtmp://localhost:1935/live/tianfeng";

	AVFormatContext *ictx = NULL;

	//���ļ�������ļ�ͷ
	int ret = avformat_open_input(&ictx, inUrl, 0, NULL);
	if (ret < 0) {
		cout << "���ļ�ʧ�� ����ֵ" << ret << endl;
		goto end;
	}

	//��ȡ��Ƶ��Ƶ����Ϣ .h264 flv û��ͷ��Ϣ
	ret = avformat_find_stream_info(ictx, 0);
	if (ret != 0) {
		cout << "��ȡ��Ƶ����Ϣʧ�� ����ֵ" << ret << endl;
		goto end;
	}

	//��ӡ��Ƶ��Ƶ��Ϣ
	//0��ӡ����  inUrl ��ӡʱ����ʾ��
	av_dump_format(ictx, 0, inUrl, 0);

	AVFormatContext * octx = NULL;
	//����������ļ� flv���Բ��������Դ��ļ����жϡ������������봫
	//�������������
	ret = avformat_alloc_output_context2(&octx, NULL, "flv", outUrl);
	if (ret < 0) {
		cout << "�������������ʧ�� ����ֵ" << ret << endl;
		goto end;
	}

	AVOutputFormat *ofmt = octx->oformat;
	for (int i = 0; i < ictx->nb_streams; i++) {
		//��ȡ������Ƶ��
		AVStream *in_stream = ictx->streams[i];
		//Ϊ����������������Ƶ������ʼ��һ������Ƶ��������
		AVStream *out_stream = avformat_new_stream(octx, in_stream->codec->codec);
		if (!out_stream) {
			printf("δ�ܳɹ��������Ƶ��\n");
			ret = AVERROR_UNKNOWN;
		}

		//��������������������Ϣ copy ������������������
		//ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
		//ret = avcodec_parameters_from_context(out_stream->codecpar, in_stream->codec);
		//ret = avcodec_parameters_to_context(out_stream->codec, in_stream->codecpar);
		if (ret < 0) {
			printf("copy �������������ʧ��\n");
		}
		out_stream->codecpar->codec_tag = 0;

		out_stream->codec->codec_tag = 0;
		if (octx->oformat->flags & AVFMT_GLOBALHEADER) {
			out_stream->codec->flags = out_stream->codec->flags | CODEC_FLAG_GLOBAL_HEADER;
		}
	}

	int videoindex = -1;
	//���������ݵ�����ѭ��
	for (int i = 0; i < ictx->nb_streams; i++) {
		if (ictx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}
	}

	
	av_dump_format(octx, 0, outUrl, 1);


	//��IO
	ret = avio_open(&octx->pb, outUrl, AVIO_FLAG_WRITE);
	if (ret < 0) {
		cout << "��IOʧ��" << endl;
		goto end;
	}

	//д��ͷ����Ϣ
	ret = avformat_write_header(octx, 0);
	if (ret < 0) {
		cout << "д��ͷ����Ϣʧ�� ����ֵ" << ret << endl;
		goto end;
	}

	//����ÿһ֡����
	//int64_t pts  [ pts*(num/den)  �ڼ�����ʾ]
	//int64_t dts  ����ʱ�� [P֡(�������һ֡�ı仯) I֡(�ؼ�֡������������) B֡(��һ֡����һ֡�ı仯)]  ����B֡ѹ���ʸ��ߡ�
	//uint8_t *data    
	//int size
	//int stream_index
	//int flag
	AVPacket pkt;
	//��ȡ��ǰ��ʱ���  ΢��
	long long start_time = av_gettime();
	long long frame_index = 0;
	while (true)
	{
		//���������Ƶ��
		AVStream *in_stream, *out_stream;
		//��ȡ����ǰ����
		ret = av_read_frame(ictx, &pkt);
		
		if (ret < 0) {
			break;
		}

		/*
		PTS��Presentation Time Stamp����ʾ����ʱ��
		DTS��Decoding Time Stamp������ʱ��
		*/
		//û����ʾʱ�䣨����δ����� H.264 ��
		if (pkt.pts == AV_NOPTS_VALUE) {
			//AVRational time_base��ʱ����ͨ����ֵ���԰�PTS��DTSת��Ϊ������ʱ�䡣
			AVRational time_base1 = ictx->streams[videoindex]->time_base;

			//������֮֡���ʱ��
			/*
			r_frame_rate ����֡����  ������̫����
			av_q2d ת��Ϊdouble����
			*/
			int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(ictx->streams[videoindex]->r_frame_rate);

			//���ò���
			pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
			pkt.dts = pkt.pts;
			pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
		}

		//��ʱ
		if (pkt.stream_index == videoindex) {
			AVRational time_base = ictx->streams[videoindex]->time_base;
			AVRational time_base_q = { 1, AV_TIME_BASE };
			//������Ƶ����ʱ��
			int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
			//����ʵ����Ƶ�Ĳ���ʱ��
			int64_t now_time = av_gettime() - start_time;

			AVRational avr = ictx->streams[videoindex]->time_base;
			cout << avr.num << " " << avr.den << "  " << pkt.dts << "  " << pkt.pts << "   " << pts_time << endl;
			if (pts_time > now_time) {
				//˯��һ��ʱ�䣨Ŀ�����õ�ǰ��Ƶ��¼�Ĳ���ʱ����ʵ��ʱ��ͬ����
				av_usleep((unsigned int)(pts_time - now_time));
			}
		}

		in_stream = ictx->streams[pkt.stream_index];
		out_stream = octx->streams[pkt.stream_index];

		//������ʱ������ָ��ʱ���
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = (int)av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		//�ֽ�����λ�ã�-1 ��ʾ��֪���ֽ���λ��
		pkt.pos = -1;

		if (pkt.stream_index == videoindex) {
			printf("Send %8d video frames to output URL\n", frame_index);
			frame_index++;
		}

		//����������ķ��ͣ����ַ���ͣ�
		ret = av_interleaved_write_frame(octx, &pkt);

		if (ret < 0) {
			printf("�������ݰ�����\n");
			break;
		}

		//�ͷ�
		av_free_packet(&pkt);

	}

	end:

	system("pause");
	return 0;
}