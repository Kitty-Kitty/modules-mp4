#ifdef _WIN32
#include <stddef.h>
typedef size_t ssize_t;
#else
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "mp4muxer.h"
#define ENABLE_AUDIO 1
#if ENABLE_AUDIO
#include <fdk-aac/aacenc_lib.h>
#include <fdk-aac/aacdecoder_lib.h>
//#define AUDIO_RATE 12000
#define AUDIO_RATE 44100
#endif

#define VIDEO_FPS 24

// H.264 NAL type
			/**
			* 表示h264的nalu类型
			* @author f
			* @version 1.0
			* @created 07-12月-2019 16:30:52
			*/
enum h264_nalu_type
{
	h264_nalu_nal = 0,					//
	h264_nalu_slice,					//非idr图像的编码条带(p 帧)
	h264_nalu_slice_dpa,				//编码条带数据分割块a
	h264_nalu_slice_dpb,				//编码条带数据分割块b
	h264_nalu_slice_dpc,				//编码条带数据分割块c
	h264_nalu_slice_idr,				//idr图像的编码条带(i 帧)
	h264_nalu_sei,						//补充增强信息单元
	h264_nalu_sps,						//序列参考集
	h264_nalu_pps,						//图象参考集
	h264_nalu_aud,						//分界符
	h264_nalu_eoseq,					//序列结束
	h264_nalu_eostream,					//码流结束
	h264_nalu_fill,						//填充
										//13..23	保留
										//24..31	未使用
};

//表示有标记的缓存结构体
typedef struct byte_buffer_s
{
	uint8_t *buffer;				//表示缓存内存空间
	int  length;					//表示当前有效数据的长度
} byte_buffer;

static uint8_t *preload(const char *path, ssize_t *data_size)
{
	FILE *file = fopen(path, "rb");
	uint8_t *data = NULL;
	*data_size = 0;
	if (!file)
		return 0;
	if (fseek(file, 0, SEEK_END))
		exit(1);
	*data_size = (ssize_t)ftell(file);
	if (*data_size < 0)
		exit(1);
	if (fseek(file, 0, SEEK_SET))
		exit(1);
	data = (unsigned char*)malloc(*data_size);
	if (!data) {
		exit(1);
	}

	ssize_t read_size = fread(data, 1, *data_size, file);
	if (read_size != *data_size) {
		exit(1);
	}
	fclose(file);
	return data;
}

static ssize_t get_nal_size(uint8_t *buf, ssize_t size)
{
	ssize_t pos = 3;
	while ((size - pos) > 3)
	{
		if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 1)
			return pos;
		if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 0 && buf[pos + 3] == 1)
			return pos;
		pos++;
	}
	return size;
}

int main(int argc, char **argv)
{
	int					tmp_number = 0;
	long				tmp_usec = 0;
#ifndef _WIN32
	struct timeval		tmp_begin_tv = { 0, };
	struct timeval		tmp_end_tv = { 0, };
#endif


	if (argc < 3)
	{
		printf("usage: minimp4 in.h264 in.pcm out.mp4\n");
		return 0;
	}
	ssize_t h264_size;
	uint8_t *alloc_buf;
	uint8_t *buf_h264 = alloc_buf = preload(argv[1], &h264_size);
	if (!buf_h264)
	{
		printf("error: can't open h264 file\n");
		return 0;
	}
	else {
		printf("open h264[%s] file succeed!\n", argv[1]);
	}

	//uint8_t sps[] = { 0x67, 0x64, 0x00, 0x28, 0xAC, 0xD9, 0x00, 0x78, 0x06, 0x5A, 0x6A, 0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x18, 0x47, 0x8C, 0x18, 0xCB };
	//uint8_t pps[] = { 0x68, 0xE9, 0x2B, 0xCB };
	//uint8_t sps[] = { 0x67, 0x42, 0x00, 0x2a, 0x96, 0x35, 0xc0, 0xf0, 0x04, 0x4f, 0xcb, 0x37, 0x01, 0x01, 0x01, 0x02 };
	//uint8_t pps[] = { 0x68, 0xce, 0x3c, 0x80, 0x00, 0x00, 0x00, 0x01, 0x06, 0xe5, 0x01, 0xa1, 0x80 };
	
	//uint8_t sps[] = { 0x67, 0x42, 0xc0, 0x15, 0xda, 0x07, 0x82, 0x9a, 0x10, 0x00, 0x00, 0x03, 0x00, 0x10, 0x00, 0x00, 0x03, 0x01, 0xe8, 0xf1, 0x62, 0xea };
	//uint8_t pps[] = { 0x68, 0xce, 0x1f, 0x20 };
	byte_buffer sps = { 0, };
	byte_buffer pps = { 0, };
	uint8_t	nalu_type = 0;
	
	//void *mp4 = mp4muxer_init(argv[3], 2710266, 480, 320, 15, 50, 1, 44100, 16, 1024, NULL);
	//void *mp4 = mp4muxer_init(argv[3], 300000, 480, 320, 15, 50, 1, 44100, 16, 1024, NULL);
	void *mp4 = mp4muxer_init(argv[3], 1800000, 1920, 1080, 30, 50, 1, 44100, 16, 1024, NULL);
	if (mp4) {
		printf("create mp4[%s] file succeed!\n", argv[3]);
	}
	//void *mp4 = mp4muxer_init(argv[2], 300000, 1920, 1080, 30, 50, 1, 44100, 16, 1024, NULL);
	//void *mp4 = mp4muxer_init(argv[2], 1800000, 1920, 1080, 30, 50, 1, 44100, 16, 1024, NULL);
	//mp4muxer_spspps(mp4, sps, sizeof(sps), pps, sizeof(pps));	

#if ENABLE_AUDIO
	ssize_t pcm_size;
	int16_t *alloc_pcm;
	//int16_t *buf_pcm = alloc_pcm = (int16_t *)preload("stream.pcm", &pcm_size);
	int16_t *buf_pcm = alloc_pcm = (int16_t *)preload(argv[2], &pcm_size);
	if (!buf_pcm)
	{
		printf("error: can't open pcm file\n");
		return 0;
	}
	else {
		printf("open pcm[%s] file succeed!\n", argv[2]);
	}

	uint32_t sample = 0, total_samples = pcm_size / 2;
	uint64_t ts = 0, ats = 0;
	HANDLE_AACENCODER aacenc;
	AACENC_InfoStruct info;
	aacEncOpen(&aacenc, 0, 0);
	aacEncoder_SetParam(aacenc, AACENC_TRANSMUX, 0);
	aacEncoder_SetParam(aacenc, AACENC_AFTERBURNER, 1);
	aacEncoder_SetParam(aacenc, AACENC_BITRATE, 64000);
	aacEncoder_SetParam(aacenc, AACENC_SAMPLERATE, AUDIO_RATE);
	aacEncoder_SetParam(aacenc, AACENC_CHANNELMODE, 1);
	aacEncEncode(aacenc, NULL, NULL, NULL, NULL);
	aacEncInfo(aacenc, &info);

	mp4muxer_aacdecspecinfo(mp4, info.confBuf);
#endif
	while (h264_size > 0)
	{
		ssize_t nal_size = get_nal_size(buf_h264, h264_size);
		//printf("nal size=%ld, rest=%ld\n", nal_size, h264_size);
		if (!nal_size)
		{
			buf_h264 += 1;
			h264_size -= 1;
			continue;
		}

#ifndef _WIN32
		gettimeofday(&tmp_begin_tv, NULL);
#endif

		nalu_type = buf_h264[4] & 0x1f;
		switch (nalu_type)
		{
		case h264_nalu_sps:
		{
			sps.buffer = buf_h264;
			sps.length = nal_size;
		}
		break;
		case h264_nalu_pps:
		{
			pps.buffer = buf_h264;
			pps.length = nal_size;
			mp4muxer_spspps(mp4, sps.buffer, sps.length, pps.buffer, pps.length);
		}
		break;
		default:
		{
			mp4muxer_video(mp4, buf_h264, nal_size, 90000 / VIDEO_FPS);
		}
		break;
		}

		//mp4muxer_video(mp4, buf_h264, nal_size, 90000 / VIDEO_FPS);

#ifndef _WIN32
		gettimeofday(&tmp_end_tv, NULL);

		tmp_usec = (tmp_end_tv.tv_sec - tmp_begin_tv.tv_sec) * 1000000LL + (tmp_end_tv.tv_usec - tmp_begin_tv.tv_usec);
		printf("write nal[ %d ] size: %d current_time: %d s %d us used_time: %ld \r\n"
			, tmp_number++
			, nal_size
			, tmp_end_tv.tv_sec
			, tmp_end_tv.tv_usec
			, tmp_usec);
#endif
		buf_h264 += nal_size;
		h264_size -= nal_size;

#if ENABLE_AUDIO
		ts += 90000 / VIDEO_FPS;
		while (ats < ts)
		{
			AACENC_BufDesc in_buf, out_buf;
			AACENC_InArgs  in_args;
			AACENC_OutArgs out_args;
			uint8_t buf[2048];
			if (total_samples < 1024)
			{
				buf_pcm = alloc_pcm;
				total_samples = pcm_size / 2;
			}
			in_args.numInSamples = 1024;
			void *in_ptr = buf_pcm, *out_ptr = buf;
			int in_size = 2 * in_args.numInSamples;
			int in_element_size = 2;
			int in_identifier = IN_AUDIO_DATA;
			int out_size = sizeof(buf);
			int out_identifier = OUT_BITSTREAM_DATA;
			int out_element_size = 1;

			in_buf.numBufs = 1;
			in_buf.bufs = &in_ptr;
			in_buf.bufferIdentifiers = &in_identifier;
			in_buf.bufSizes = &in_size;
			in_buf.bufElSizes = &in_element_size;
			out_buf.numBufs = 1;
			out_buf.bufs = &out_ptr;
			out_buf.bufferIdentifiers = &out_identifier;
			out_buf.bufSizes = &out_size;
			out_buf.bufElSizes = &out_element_size;

			if (AACENC_OK != aacEncEncode(aacenc, &in_buf, &out_buf, &in_args, &out_args))
			{
				printf("error: aac encode fail\n");
				exit(1);
			}
			sample += in_args.numInSamples;
			buf_pcm += in_args.numInSamples;
			total_samples -= in_args.numInSamples;
			ats = (uint64_t)sample * 90000 / AUDIO_RATE;

			mp4muxer_audio(mp4, buf, out_args.numOutBytes, ats);

			//MP4E__put_sample(mux, audio_track_id, buf, out_args.numOutBytes, 1024 * 90000 / AUDIO_RATE, MP4E_SAMPLE_RANDOM_ACCESS);
		}
#endif
	}
	if (alloc_buf)
		free(alloc_buf);

#if ENABLE_AUDIO
	if (alloc_pcm)
		free(alloc_pcm);
#endif

#ifndef _WIN32
	gettimeofday(&tmp_begin_tv, NULL);
#endif
	mp4muxer_exit(mp4);

#ifndef _WIN32
	gettimeofday(&tmp_end_tv, NULL);

	tmp_usec = (tmp_end_tv.tv_sec - tmp_begin_tv.tv_sec) * 1000000LL + (tmp_end_tv.tv_usec - tmp_begin_tv.tv_usec);
	printf("current_time: %d s %d us stop process: %ld \r\n"
		, tmp_end_tv.tv_sec
		, tmp_end_tv.tv_usec
		, tmp_usec);
#endif
}
