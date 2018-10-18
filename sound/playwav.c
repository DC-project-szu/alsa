#include <alsa/asoundlib.h>
#include <stdio.h>
#include <unistd.h>
#include <alloca.h>
#include <error.h>
#include <string.h>



#define RADIO	0
#define PHONE 1

/* Samples per packet in SPEEX */
#define SPEEX_SAMPLES	480

/* 函数前置声明 */
#define INT16 short
int toneDetecting(INT16 input_array[]);

snd_pcm_t *rhandle, *whandle;
snd_pcm_hw_params_t *rparams, *wparams;


/* Create sound device */
//初始化声卡的capture和playback
int sound_init(void)
{
    int rc;
    int dir = 0;

    unsigned int val;
    snd_pcm_uframes_t frames;

    /* Init the capture handle */
    /* rc = snd_pcm_open(&rhandle, "plughw:1", SND_PCM_STREAM_CAPTURE, 0); */
    rc = snd_pcm_open(&rhandle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if(rc < 0)
    {
        printf("unable to open capture device : %s\n", snd_strerror(rc));
        return(1);
    }

    /* allocate an invalid snd_pcm_hw_params_t using standard alloca */
    snd_pcm_hw_params_alloca(&rparams);
    /* Fill params with a full configuration space for a PCM. */
    rc = snd_pcm_hw_params_any(rhandle, rparams);
    if(rc < 0)
    {
        printf("unable to fill capture parameters : %s\n", snd_strerror(rc));
        return(1);
    }
    /* NONINTERLEAVED mode, snd_pcm_readn/snd_pcm_writen access,
     * data read/write in seperated buffers
     */
    rc = snd_pcm_hw_params_set_access(rhandle, rparams, SND_PCM_ACCESS_RW_INTERLEAVED); //注意！！这是交错模式
    if(rc < 0)
    {
        printf("unable to set access mode : %s\n", snd_strerror(rc));
        return(1);
    }
    /* Signed 16 bit Little Endian */
    rc = snd_pcm_hw_params_set_format(rhandle, rparams, SND_PCM_FORMAT_S16_LE); //注意！！这是16bit
    if(rc < 0)
    {
        printf("unable to set capture format : %s\n", snd_strerror(rc));
        return(1);
    }

    /* 2 channels */
    rc = snd_pcm_hw_params_set_channels(rhandle, rparams, 2);
    if(rc < 0)
    {
        printf("unable to set capture channels : %s\n", snd_strerror(rc));
        return(1);
    }
    /* 8kHz */
    val = 8000;
    rc = snd_pcm_hw_params_set_rate_near(rhandle, rparams, &val, 0);
    if(rc < 0)
    {
        printf("unable to set capture sample rate : %s\n", snd_strerror(rc));
        return(1);
    }
    printf("Sample rate is %dHz\n", val);

    /* r/w 480 samples */
    frames = 40;
    rc = snd_pcm_hw_params_set_period_size_near(rhandle, rparams, &frames, &dir);
    if(rc < 0)
    {
        printf("snd_pcm_hw_params_set_period_size_near : %s\n", snd_strerror(rc));
        return(1);
    }
    printf("Period size = %d\n", (int)frames);

    rc = snd_pcm_hw_params(rhandle, rparams);
    if(rc < 0)
    {
        printf("unable to set capture parameters : %s\n", snd_strerror(rc));
        return(1);
    }

    /* Init the playback handle */
    /* rc = snd_pcm_open(&whandle, "plughw:1", SND_PCM_STREAM_PLAYBACK, 0); */
    rc = snd_pcm_open(&whandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if(rc < 0)
    {
        printf("unable to open playback device : %s\n", snd_strerror(rc));
        return(1);
    }

    /* allocate an invalid snd_pcm_hw_params_t using standard alloca */
    snd_pcm_hw_params_alloca(&wparams);

    /* Fill params with a full configuration space for a PCM. */
    rc = snd_pcm_hw_params_any(whandle, wparams);
    if(rc < 0)
    {
        printf("unable to set playback parameters : %s\n", snd_strerror(rc));
        return(1);
    }
    /* NONINTERLEAVED mode, snd_pcm_readn/snd_pcm_writen access,
     * data read/write in seperated buffers
     */
    rc = snd_pcm_hw_params_set_access(whandle, wparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(rc < 0)
    {
        printf("unable to set access mode : %s\n", snd_strerror(rc));
        return(1);
    }
    /* Signed 16 bit Little Endian */
    rc = snd_pcm_hw_params_set_format(whandle, wparams, SND_PCM_FORMAT_S16_LE);
    if(rc < 0)
    {
        printf("unable to set format : %s\n", snd_strerror(rc));
        return(1);
    }
    /* 2 channels */
    rc = snd_pcm_hw_params_set_channels(whandle, wparams, 2);
    if(rc < 0)
    {
        printf("unable to set channel : %s\n", snd_strerror(rc));
        return(1);
    }

    /* 8kHz */
    val = 8000;
    rc = snd_pcm_hw_params_set_rate_near(whandle, wparams, &val, 0);
    if(rc < 0)
    {
        printf("unable to set sample rate : %s\n", snd_strerror(rc));
        return(1);
    }
    /* r/w 480 samples */
    frames = 40;
    rc = snd_pcm_hw_params_set_period_size_near(whandle, wparams, &frames, &dir);
    if(rc < 0)
    {
        printf("snd_pcm_hw_params_set_period_size_near : %s\n", snd_strerror(rc));
        return(1);
    }

    rc = snd_pcm_hw_params(whandle, wparams);
    if(rc < 0)
    {
        printf("unable to set playback parameters : %s\n", snd_strerror(rc));
        return(1);
    }

    /* set_volume(); */

    return 0;
}

static int xrunRecovery(snd_pcm_t *rc, int err)
{
    printf("Recovering from underrun\n");

    if (err == -EPIPE) {
        if (snd_pcm_prepare(rc) < 0) {
            printf("Failed to recover from over or underrun on.\n");
            return -1;
        }
    }
    else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(rc)) == -EAGAIN) {
            sleep (1);  /* wait until the suspend flag is released */
        }

        if (snd_pcm_prepare(rc) < 0) {
            printf("Failed to recover from over or underrun.\n");
            return -1;
        }
    }
    else {
        return -1;
    }

    return 0;
}


int sound_read(	snd_pcm_t * pcm, void * bufs, snd_pcm_uframes_t size )
{
    int numSamples, readSamples;
    void *bufPtr;

    //为什么要多设置这两个变量？是怕改变指针的值?
    readSamples = size;
    bufPtr = bufs;

    while (readSamples > 0) {
        numSamples = snd_pcm_readi(pcm, bufPtr, readSamples);

        if (numSamples == -EAGAIN)
            continue;

        if (numSamples < 0) {
            if (xrunRecovery(pcm, numSamples) < 0) {
                printf ("Failed to read from (%s)\n", strerror(numSamples));
                return -1;
            }
        }
        else {
            bufPtr += numSamples * 2 *  2;
            readSamples -= numSamples;
        }
    }

    return 0;
}



/* Split stereo sample into Left-Right channel */
int sound_split( void* InBuf, void* LeftBuf, void* RightBuf, int len )
{
    int ii;

    short *in = (short *)(InBuf),
          *left = (short *)(LeftBuf),
          *right = (short *)(RightBuf);

    for (ii = 0; ii < len; ii += 2){
        *(left + ii/2) = *(in + ii);
        *(right+ ii/2) = *(in + ii + 1);
    }

    return ii;
}

/* Combine the Left-Right channel to stereo */
int sound_combine( void* OutBuf, void* LeftBuf, void* RightBuf, int len )
{
    int ii;

    short *out = (short *)(OutBuf),
          *left = (short *)(LeftBuf),
          *right = (short *)(RightBuf);

    for (ii = 0; ii < len; ii += 2){
        *(out + ii) = *(left + ii/2);
        /* if (UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER) */
        if (0)
            ;
        /* *(out + ii + 1) = (*(right+ ii/2) /4*3); */
        else
            *(out + ii + 1) = *(right+ ii/2);
    }

    return ii;
}


/* Delete sound device */
void sound_delete()
{
    snd_pcm_drain(rhandle);
    snd_pcm_close(rhandle);

    snd_pcm_drain(whandle);
    snd_pcm_close(whandle);

    return;
}


int sound_write(snd_pcm_t *pcm, void *bufs, snd_pcm_uframes_t size )
{
    int numSamples, writeSamples;
    void *bufPtr;

    writeSamples = size;
    bufPtr = bufs;

    while (writeSamples > 0) {

        /* start by doing a blocking wait for free space. */
        /* snd_pcm_wait (pcm, 40); */

        numSamples = snd_pcm_writei(pcm, bufPtr, writeSamples);

        if (numSamples == -EAGAIN)
            continue;

        if (numSamples < 0) {
            if (xrunRecovery(pcm,numSamples) < 0) {
                printf ("Failed to write to (%s)\n", strerror(numSamples));
                return -1;
            }
        }
        else {
            bufPtr += numSamples * 2 *  2;
            writeSamples -= numSamples;
        }
    }

    return 0;
}


void usage(char *argv)
{
    printf("%s filename filename", argv);
}

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        usage(argv[0]);
        exit(0);
    }

    void *InputBuf[2], // Input buffer, 0 - L, 1 - R
         *OutputBuf[2];
    void *InBuf, *OutBuf;

    InBuf = alloca(SPEEX_SAMPLES * 2 * 2); //alloca:在堆栈上分配内存,详见P122
    OutBuf = alloca(SPEEX_SAMPLES * 2 * 2);
    InputBuf[RADIO] = alloca(SPEEX_SAMPLES * 2);
    InputBuf[PHONE] = alloca(SPEEX_SAMPLES * 2);

    OutputBuf[RADIO] = alloca(SPEEX_SAMPLES * 2);
    OutputBuf[PHONE] = alloca(SPEEX_SAMPLES * 2);

    int frames;

    sound_init();

    int fd1 = open(argv[1], O_RDONLY);
    if(fd1 == -1)
        perror("open error\n");

    int fd2 = open(argv[2], O_RDONLY);
    if(fd2 == -1)
        perror("open error\n");

    ssize_t numRead1, numRead2;
    while((numRead1 = read(fd1, OutputBuf[RADIO], SPEEX_SAMPLES*2)) > 0 && (numRead2 = read(fd2, OutputBuf[PHONE], SPEEX_SAMPLES*2)) > 0)
    {
        if(numRead1 == -1 && numRead2 == -1)
        {
            perror("read error\n");
            break;
        }


        sound_combine(OutBuf, OutputBuf[RADIO], OutputBuf[PHONE], SPEEX_SAMPLES * 2);
        /* frames = sound_write(whandle, OutBuf, SPEEX_SAMPLES); */
        frames = sound_write(whandle, OutputBuf[RADIO], SPEEX_SAMPLES);
        if (frames < 0)
        {
            printf("Failed to write speech buffer.\n");
            usleep(1);
        }


        /* Read samples from the Sound device */
        /* frames = sound_read(rhandle, InBuf, SPEEX_SAMPLES) ; */
        /* if (frames < 0) */
        /* { */
        /*     printf("Failed to read speech buffer\n"); */
        /* } */

        /* sound_split(InBuf, InputBuf[RADIO], InputBuf[PHONE] , SPEEX_SAMPLES * 2); */

        /* busy tone detected */
        /* if (toneDetecting((short*)InputBuf[PHONE]) > 7) */
        /* int ret = toneDetecting((short*)InputBuf[PHONE]); */
        /* int ret = toneDetecting((short*)InputBuf[RADIO]); */
        /* int ret = toneDetecting((short*)OutputBuf[RADIO]); */
        /* printf("ret = %d\n", ret); */
        /* if (ret > 7) */
        /* { */
            /* if (toneDetecting((short*)InBuf) > 7){ */
            /* printf("Busy tone detected.\n"); */
        /* } */

    }
    sound_delete();
    close(fd1);
    close(fd2);

    return 0;
}
