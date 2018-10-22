/*play*/

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

int error_deal(snd_pcm_t *handle);

int main()
{
    int rc;
    snd_pcm_t *handle;//PCM句柄
    snd_pcm_hw_params_t *params;//PCM配置空间
    snd_pcm_uframes_t frames;//设置frame定义
    char *buffer;
    int dir;
    int val;
    int size;

/*------------------------------初始化PCM设备---------------------------------*/
    // 打开PCM设备
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK , 0);
    if(rc<0)
    {
        printf("can not open a PCM device,can not receive a handle of PCM device");
        return (-1);
    }

    //为设备分配配置空间
    snd_pcm_hw_params_alloca(&params);
    //初始化配置空间
    snd_pcm_hw_params_any(handle,params);
    //设置交错模式录音
    rc = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		error_deal(handle);
	}
    //设置字符在内存中的存放顺序
    rc = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		error_deal(handle);
	}
    //设置声道
    rc = snd_pcm_hw_params_set_channels(handle,params,2);
    if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		error_deal(handle);
	}
    
    //设置采样率，一般来说采样率设为44100hz，但是由于不同设备支持的采样率不同，所以还需要PCM设备本身适应，
    //所以这里我们加入了与采样率有关的函数
    val=44100;
    rc = snd_pcm_hw_params_set_rate_near(handle,params,&val,&dir);//Question:在这里后面两个参数的使用取地址符，应该是调用这个函数的时候，自动给他们赋值
    if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		error_deal(handle);
	}
    printf("这里的采样率是%d",val);
    
    //设置一个period里面有多少帧
    frames = 32;//Question:这里的32是根据什么来设置的？
    rc = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		error_deal(handle);
	}
    printf("这里的frame是%ld",frames);

    //激活设备
    rc = snd_pcm_hw_params(handle,params);
    if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		error_deal(handle);
	}
    //设置frames的大小
    rc = snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		error_deal(handle);
	}
    size = frames*4;
    buffer = (char *)malloc(size);//buffer ,make sure that it is big enough to hold one period

    //推测，这里是为了根据音频数据得出peiriod的个数
    rc = snd_pcm_hw_params_get_period_time(params, &val, &dir);//Question
    if (rc < 0)
	{
		printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
		error_deal(handle);
	}
/*----------------------------播放-------------------------------*/

    while(1)
    {
        rc=read(0,buffer,frames);
        if(rc<0)
        {
            printf("wrong reading");
            return(-1);
        }
        rc=snd_pcm_writei(handle,buffer,frames);
        if (rc == -EPIPE) {
            /* EPIPE means underrun */
            fprintf(stderr, "underrun occured\n");
            snd_pcm_prepare(handle);
        } 
        else if (rc < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        } 
        else if (rc != (int)frames) {
            fprintf(stderr, "short read, read %d frames\n", rc);
        }
        //这里需要判断什么时候数据读取完了吗?
    }
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return 0;
}

int error_deal(snd_pcm_t *handle)
{
    snd_pcm_close(handle);
	return -1;
}