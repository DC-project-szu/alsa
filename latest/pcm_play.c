#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define	u32 	unsigned int
#define	u8 		unsigned char
#define u16		unsigned short

#define ERROR -1

typedef struct {
	u32		dwSize;
	u16		wFormatTag;
	u16		wChannels;
	u32		dwSamplesPerSec;
	u32		dwAvgBytesPerSec;
	u16		wBlockAlign;
	u16		wBitsPerSample;
} WAVEFORMAT;

typedef struct {
	u8		RiffID [4];
	u32     RiffSize;
	u8    	WaveID [4];
	u8    	FmtID [4];
	u32     FmtSize;
	u16   	wFormatTag;
	u16   	nChannels;
	u32 	nSamplesPerSec;  /*����Ƶ��*/
	u32 	nAvgBytesPerSec; /*ÿ�������ֽ���*/
	u16		nBlockAlign;     /*���ݿ���뵥λ,ÿ��������Ҫ���ֽ���*/
	u16		wBitsPerSample;  /*ÿ��������Ҫ��bit��*/
	u8		DataID[4];
	u32 	nDataBytes;
} WAVE_HEADER; //存储wav文件的各种参数


WAVE_HEADER g_wave_header;
FILE * open_and_print_file_params(char *file_name)
{
	FILE * fp = fopen(file_name, "r");
	if(fp == NULL)
	{
		printf("Can't open wave file.\n");
		return NULL;
	}
	memset(&g_wave_header, 0, sizeof(g_wave_header));
	fread(&g_wave_header, 1, sizeof(g_wave_header), fp); //fread(&g_wave_header, sizeof(g_wave_header),1, fp);
	printf("----- Wav Header -----\n");
	printf("RiffID:%c%c%c%c\n", g_wave_header.RiffID[0], g_wave_header.RiffID[1], g_wave_header.RiffID[2], g_wave_header.RiffID[3]);
	printf("RiffSize:%d bits\n", g_wave_header.RiffSize);
	printf("WaveID:%c%c%c%c\n", g_wave_header.WaveID[0], g_wave_header.WaveID[1], g_wave_header.WaveID[2], g_wave_header.WaveID[3]);
	printf("FmtID:%c%c%c%c\n", g_wave_header.FmtID[0], g_wave_header.FmtID[1], g_wave_header.FmtID[2], g_wave_header.FmtID[3]);
	printf("FmtSize:%d bit\n", g_wave_header.FmtSize);
	printf("wFormatTag:%d\n", g_wave_header.wFormatTag);
	printf("nChannels:%d\n", g_wave_header.nChannels);
	printf("nSamplesPerSec:%d Hz\n", g_wave_header.nSamplesPerSec);
	printf("nAvgBytesPerSec:%d bytes\n", g_wave_header.nAvgBytesPerSec);
	printf("nBlockAlign:%d\n", g_wave_header.nBlockAlign);
	printf("wBitsPerSample:%d bits\n", g_wave_header.wBitsPerSample);
	printf("DataID:%c%c%c%c\n", g_wave_header.DataID[0], g_wave_header.DataID[1], g_wave_header.DataID[2], g_wave_header.DataID[3]);
	printf("nDataBytes:%d\n", g_wave_header.nDataBytes);
	printf("---------------------\n");
	
	return fp;
}

snd_pcm_t 				*gp_handle;
snd_pcm_hw_params_t 	*gp_params;
snd_pcm_uframes_t		g_frames;
u32						g_bufsize;
char 					*gp_buffer;

int set_hardware_params(void)
{
	int rc;
	
	/* Init the handle, SND_PCM_STREAM_PLAYBACK or SND_PCM_STREAM_CAPTURE */ 
	rc = snd_pcm_open(&gp_handle, "plughw:1", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0)
	{
		printf("Unable to open playback device : %s\n", snd_strerror(rc));
		return ERROR;
	}
	/* ����һ��Ӳ�������������Ĭ��ֵ */
	snd_pcm_hw_params_alloca(&gp_params);
	rc = snd_pcm_hw_params_any(gp_handle, gp_params);
	if (rc < 0)
	{
		printf("Unable to fill it with default value : %s\n", snd_strerror(rc));
		goto err1;
	}
	/* ���ý���ģʽ */
	rc = snd_pcm_hw_params_set_access(gp_handle, gp_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		goto err1;
	}
	/* ���ø�ʽ������wav�ļ�ͷ�а�������Ϣ */
	snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;  // teacher's code use, singned 16 bits Little Endian 
	rc = snd_pcm_hw_params_set_format(gp_handle, gp_params, format);
	if (rc < 0)
	{
		printf("Unable to set formatrc : %s\n", snd_strerror(rc));
		goto err1;
	}
	/* ���������� */
	rc = snd_pcm_hw_params_set_channels(gp_handle, gp_params, g_wave_header.nChannels);
	if (rc < 0)
	{
		printf("Unable to set channels : %s\n", snd_strerror(rc));
		goto err1;
	}
	/* ���ò����� */
	u32 dir, rate = g_wave_header.nSamplesPerSec;
	rc = snd_pcm_hw_params_set_rate_near(gp_handle, gp_params, &rate, 0);
	if (rc < 0)
	{
		printf("Unable to set sample rate : %s\n", snd_strerror(rc));
		goto err1;
	}
	/* ������д�������豸 */
	rc = snd_pcm_hw_params(gp_handle, gp_params);
	if (rc < 0)
	{
		printf("Unable to set parameters : %s\n", snd_strerror(rc));
		goto err1;
	}
	else	printf("[ok] Set parameters successfully.\n");
	/* ��ȡÿ��������Ҫ�Ĵ�С */ 
	// printf("%d\n", g_frames); 
	rc = snd_pcm_hw_params_get_period_size(gp_params, &g_frames, 0);
	// printf("%d\n", g_frames);
	if (rc < 0)
	{
		printf("Unable to set period size : %s\n", snd_strerror(rc));
		goto err1;
	}
	/* ����ռ� */ 
	g_bufsize = g_frames * 4;
	gp_buffer = (u8 *)malloc(g_bufsize);
	if (gp_buffer == NULL)
	{
		printf("Malloc failed\n");
		goto err1;
	}
	printf("[ok] Mallco successfully, size : %d.\n", g_bufsize);
	printf("---------------------\n");
	return 0;

err1:
	snd_pcm_close(gp_handle);
	return ERROR;
}


int main(int argc, char *argv[])
{
	FILE * fp = open_and_print_file_params("./JH+Female.wav");
	int ret = set_hardware_params();
	
	size_t rc;
	while(1)
	{
		rc = fread(gp_buffer, g_bufsize, 1, fp);
		if(rc < 1)
		{
			break;
		}
		ret = snd_pcm_writei(gp_handle, gp_buffer, g_frames);
		if (ret == -EPIPE)
		{
			printf("Underrun occured.\n");
			break;
		}
		else if (ret < 0)
		{
			printf("Error from writei : %s.\n", snd_strerror(ret));
			break;
		}
	}
	
	snd_pcm_drain(gp_handle);
	snd_pcm_close(gp_handle);
	free(gp_buffer);
	fclose(fp);
	return 0;
}

//  完成一个wav文件的读取，写入PCM设备（先定义对要打开的PCM设备进行初始化，再分配一个空间来读取文件中的数据，然后再把空间中的数据写入PCM）

/* ---------------------------------完成=录音----------------------------------------*/




































