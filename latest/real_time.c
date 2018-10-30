/* combine capture.c with play.c*/
#define ALSA_PCM_NEW_HW_PARAMS_API
#define NUM 20 //20个参数为上限
#include <alsa/asoundlib.h>

int error_deal(snd_pcm_t *handle);//错误处理
int snd_pcm_init(char fc,snd_pcm_t *handle);//写一个初始化设备的函数，专门初始化
int capture();//录音函数
int play();//播放函数

int rc;
snd_pcm_t *handle1,*handle2;           //PCM句柄
snd_pcm_hw_params_t *params; //PCM配置空间
snd_pcm_uframes_t frames;    //设置frame定义
char *buffer;
int dir;
int val;
int size;


int main(int argc, char *argv[]) //argv[1]为文件名，*(argv+2)为录音时打开文件的mode，
{

    int i;
    while(1)
    {
        capture();
        play();
    }

    snd_pcm_drain(handle1);
    snd_pcm_close(handle1);

    snd_pcm_drain(handle2);
    snd_pcm_close(handle2);
    free(buffer);
    return 0;
}

int error_deal(snd_pcm_t *handle)
{
    snd_pcm_close(handle);
    return -1;
}

int snd_pcm_init(char fc,snd_pcm_t *handle)
{
    /*------------------------------初始化PCM设备---------------------------------*/
    // 打开PCM设备

    switch (fc)
    {
    case 'p': //播放
    {
        rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (rc < 0)
        {
            printf("can not open a PCM device,can not receive a handle of PCM device\n");
            return -1;
        }
        break;
    }
    case 'c': //录音
    {
        rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
        if (rc < 0)
        {
            printf("can not open a PCM device,can not receive a handle of PCM device\n");
            return -1;
        }
        break;
    }
    }

    //为设备分配配置空间
    snd_pcm_hw_params_alloca(&params);
    //初始化配置空间
    snd_pcm_hw_params_any(handle, params);
    //设置交错模式录音
    rc = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0)
    {
        printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
        error_deal(handle);
        return -1;
    }
    //设置字符在内存中的存放顺序
    rc = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    if (rc < 0)
    {
        printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
        error_deal(handle);
        return -1;
    }
    //设置声道
    rc = snd_pcm_hw_params_set_channels(handle, params, 2);
    if (rc < 0)
    {
        printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
        error_deal(handle);
        return -1;
    }

    //设置采样率，一般来说采样率设为44100hz，但是由于不同设备支持的采样率不同，所以还需要PCM设备本身适应，
    //所以这里我们加入了与采样率有关的函数
    val = 8000;
    rc = snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir); //Question:在这里后面两个参数的使用取地址符，应该是调用这个函数的时候，自动给他们赋值
    if (rc < 0)
    {
        printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
        error_deal(handle);
        return -1;
    }
    printf("这里的采样率是%d\n", val);

    //设置一个period里面有多少帧
    frames = 400; //Question:这里的32是根据什么来设置的？
    rc = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    if (rc < 0)
    {
        printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
        error_deal(handle);
        return -1;
    }
    printf("这里的frame是%ld\n", frames);

    //激活设备
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0)
    {
        printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
        error_deal(handle);
        return -1;
    }
    //设置frames的大小
    rc = snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    if (rc < 0)
    {
        printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
        error_deal(handle);
        return -1;
    }
    size = frames * 4;
    buffer = (unsigned short *)malloc(size); //buffer ,make sure that it is big enough to hold one period

    //推测，这里是为了根据音频数据得出peiriod的个数
    if(fc=='p')
    {
        rc = snd_pcm_hw_params_get_period_time(params, &val, &dir); //Question
        if (rc < 0)
        {
            printf("Uable to Interleaved mode : %s\n", snd_strerror(rc));
            error_deal(handle);
            return -1;
        }
    }
    return 0;
}

//录音函数
int capture()
{
    rc = snd_pcm_init('c',handle1); //声卡初始化
    if (rc == -1)
    {
        printf("something wrong in sound_card init process\n");
        return -1;
    }

    rc = snd_pcm_readi(handle1, buffer, frames); //从声卡读取数据到内存空间
    if (rc == -EPIPE)
    {
        /* EPIPE means overrun */ //Question:这里不是underrun吗？文档里面好像没有overrun
        fprintf(stderr, "overrun occured\n");
        snd_pcm_prepare(handle1);
    }
    else if (rc < 0)
    {
        fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
    }
    else if (rc != (int)frames)
    {
        fprintf(stderr, "short read, read %d frames\n", rc);
    }

    return 0;
}

int play()
{
    
    rc = snd_pcm_init('p',handle2); //声卡初始化
    if (rc == -1)
    {
        printf("something wrong in sound_card init process\n");
        return -1;
    }
    
    rc = snd_pcm_writei(handle2, buffer, frames); //underrun如何处理和检查？
    if (rc == -EPIPE)
    {
        /* EPIPE means overrun */ //Question:这里不是underrun吗？文档里面好像没有overrun
        fprintf(stderr, "overrun occured\n");
        snd_pcm_prepare(handle2);
    }
    else if (rc < 0)
    {
        fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
    }
    else if (rc != (int)frames)
    {
        fprintf(stderr, "short read, read %d frames\n", rc);
    }

    //这里需要判断什么时候数据读取完了吗?

    return 0;
}
