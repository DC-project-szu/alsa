/*
 * sound.c
 * Capture and Play sound samples on audio device
 */
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wiringPi.h>

#include "main.h"
#include "wave.h"
#include "DtmfDetector.h"

#include "gpio_table.h"

#define PICKUP_PHONE() 	digitalWrite(GPIO_HOOK, HIGH)
#define OFFHOOK_PHONE() digitalWrite(GPIO_HOOK, LOW)
#define ENABLE_PTT()		digitalWrite(GPIO_PTTA, LOW)
#define DISABLE_PTT()		digitalWrite(GPIO_PTTA, HIGH)
#define RADIO_RX()			digitalRead(GPIO_RX)

#define RADIO	0
#define PHONE 1

/* Samples per packet in SPEEX */
#define SPEEX_SAMPLES	480

/* definition of line state */
#define LINE_IDLE 		0
#define LINE_INCOMING 	1
#define LINE_OUTGOING	2
#define LINE_RETRY		3
#define LINE_ENDING		4
#define LINE_TALKING	5
#define LINE_DAILING	6

static int status = 0;

int idle_count = 20;

#ifdef MARK
FILE* fp;
#endif

void clear_line(void)
{
    status = 0;
    OFFHOOK_PHONE();
    DISABLE_PTT();
    dtmfDetecting_init();
    printf("clear line, off hook!\n");
    idle_count = 0;

#ifdef MARK
    writeWave_close(fp);
    system("sync"); //同步缓存和内存
#endif
}

//设置音量
void set_volume()
{
    long int a, b;
    long alsa_min_vol, alsa_max_vol;

    snd_mixer_t * mixer;
    snd_mixer_elem_t *pcm_element;

    snd_mixer_open(&mixer, 0);
    snd_mixer_attach(mixer, "hw:1");
    snd_mixer_selem_register(mixer, NULL, NULL);
    snd_mixer_load(mixer);

    // Only 1 element
    pcm_element = snd_mixer_first_elem(mixer);
    printf("1st elem name = [%s]\n", snd_mixer_selem_get_name(pcm_element));

    //snd_mixer_selem_has_common_volume
    printf("snd_mixer_selem_has_common_volume = %d\n", snd_mixer_selem_has_common_volume(pcm_element));
    //snd_mixer_selem_has_playback_volume
    printf("snd_mixer_selem_has_playback_volume = %d\n", snd_mixer_selem_has_playback_volume(pcm_element));
    //snd_mixer_selem_has_capture_volume
    printf("snd_mixer_selem_has_capture_volume = %d\n", snd_mixer_selem_has_capture_volume(pcm_element));

    snd_mixer_selem_get_playback_volume_range(pcm_element,
            &alsa_min_vol,
            &alsa_max_vol);
    printf("playback volume range = %d ~ %d\n", (int)alsa_min_vol, (int)alsa_max_vol);

    snd_mixer_selem_get_capture_volume_range(pcm_element,
            &alsa_min_vol,
            &alsa_max_vol);
    printf("capture volume range = %d ~ %d\n", alsa_min_vol, alsa_max_vol);

    snd_mixer_selem_set_playback_volume_all (pcm_element, alsa_max_vol);
    snd_mixer_selem_set_capture_volume_all (pcm_element, alsa_max_vol);
    snd_mixer_selem_get_playback_volume(pcm_element, SND_MIXER_SCHN_FRONT_LEFT, &a);
    printf("Get playback front left volume = %d\n", a);
    snd_mixer_selem_get_playback_volume(pcm_element, SND_MIXER_SCHN_FRONT_RIGHT, &b);
    printf("Get playback front right volume = %d\n", b);

    snd_mixer_selem_get_capture_volume(pcm_element, SND_MIXER_SCHN_FRONT_LEFT, &a);
    printf("Get capture front left volume = %d\n", a);
    snd_mixer_selem_get_capture_volume(pcm_element, SND_MIXER_SCHN_FRONT_RIGHT, &b);
    printf("Get capture front right volume = %d\n", b);

    snd_mixer_close(mixer);
    return;
}

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
    rc = snd_pcm_open(&rhandle, "plughw:1", SND_PCM_STREAM_CAPTURE, 0);
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
    rc = snd_pcm_hw_params_set_access(rhandle, rparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(rc < 0)
    {
        printf("unable to set access mode : %s\n", snd_strerror(rc));
        return(1);
    }
    /* Signed 16 bit Little Endian */
    rc = snd_pcm_hw_params_set_format(rhandle, rparams, SND_PCM_FORMAT_S16_LE);
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
    rc = snd_pcm_open(&whandle, "plughw:1", SND_PCM_STREAM_PLAYBACK, 0);
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

    set_volume();

    return 0;
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

/* play a wave file */
FILE* sound_file_playing = NULL;
int sound_file_stopping = 0;

FILE* sound_play_wave(char * filename)
{
    if (sound_file_playing){
        sound_file_stopping = 1;
        sleep(1);
    }

    sound_file_playing = playWave_init(filename); //应该在wave.h

    printf("Playing file %s.\n", filename);

    return sound_file_playing;
}

void sound_play_stop()
{
    if (sound_file_playing)
        sound_file_stopping = 1;
}

/* return the speech/mute mode */
static int ThresholdCounter[4];
static int AudioState[4];
// 判断现在是讲话还是静音模式
int sound_speech( void* Buf, int channel ) //Buf是音频数据
{
    int ii;
    int Volumn = 0;
    int Average = 0;

    int len = SPEEX_SAMPLES;
    short *buf = (short *)Buf;

    int threshold = UserConfig.TelephoneCfg.Threshold * 100;

    for(ii = 0; ii < len; ii++)
        Volumn += *(buf + ii); //Buf里的值求和

    Average = Volumn / len;
    Volumn = 0;
    for(ii = 0; ii < len; ii++){
        Volumn += abs(*(buf + ii) - Average); //abs求绝对值
        /* Calculate every 120 samples */
        if ( ((ii+1) % 120)==0){
            //printf("Volumn = %d, channel = %d, threshold = %d\n", Volumn, channel, threshold);
            /* Speech state now */
            if (AudioState[channel]){
                if (Volumn < threshold)
                    ThresholdCounter[channel]++;
                else
                    ThresholdCounter[channel] = 0;
                if (ThresholdCounter[channel] > UserConfig.TelephoneCfg.MuteSamples)
                {
                    ThresholdCounter[channel] = 0;
                    AudioState[channel] = 0;
                    printf("Mute channel %d_______________\n", channel);
                }
            }
            /* Mute state now */
            else{
                if (Volumn > threshold)
                    ThresholdCounter[channel]++;
                else
                    ThresholdCounter[channel] = 0;
                if (ThresholdCounter[channel] > UserConfig.TelephoneCfg.SpeechSamples)
                {
                    ThresholdCounter[channel] = 0;
                    AudioState[channel] = 1;
                    printf("Speech channel %d...............\n", channel);
                }
            }
            Volumn = 0;
        }
    } /* for */

    return AudioState[channel];
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
        //右声道还多了一个功能
        if (UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER)
            *(out + ii + 1) = (*(right+ ii/2) /4*3);
        else
            *(out + ii + 1) = *(right+ ii/2);
    }

    return ii;
}

/******************************************************************************
 * xrunRecovery
 * ***************************************************************************/
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

int sound_read(	snd_pcm_t * 	pcm, void * 	bufs, snd_pcm_uframes_t 	size )
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

int sound_write(	snd_pcm_t * 	pcm, void * 	bufs, snd_pcm_uframes_t 	size )
{
    int numSamples, writeSamples;
    void *bufPtr;

    writeSamples = size;
    bufPtr = bufs;

    while (writeSamples > 0) {

        /* start by doing a blocking wait for free space. */
        snd_pcm_wait (pcm, 40);

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

/* 一个线程函数 */
void * sound_thread( void* args )
{
    void 		*InputBuf[2], 		// Input buffer, 0 - L, 1 - R
                *OutputBuf[2];

    void		*InBuf, *OutBuf;

    int frames;
    int rc;
    int retry = 0;

    char * password;
    int ii, i;
    time_t audio_start = 0, audio_radio = 0, audio_telephone = 0;
    time_t time_status = 0; 	/* time entering each status */
    struct audioframe AudioFrame;

    sound_init();

    InBuf									= alloca(SPEEX_SAMPLES * 2 * 2);
    OutBuf								= alloca(SPEEX_SAMPLES * 2 * 2);
    InputBuf[RADIO] 	    = alloca(SPEEX_SAMPLES * 2);
    InputBuf[PHONE] 	  	= alloca(SPEEX_SAMPLES * 2);

    OutputBuf[RADIO]  		= alloca(SPEEX_SAMPLES * 2);
    OutputBuf[PHONE] 			= alloca(SPEEX_SAMPLES * 2);

    dtmfDetecting_init();
    DISABLE_PTT();
    OFFHOOK_PHONE();
    memset(&ThresholdCounter, 0, sizeof(ThresholdCounter));
    memset(&AudioState, 0, sizeof(AudioState));

    while(!(ShutDown&0x01)) 
    {
        /* Read samples from the Sound device */
        // 从声卡里读取数据
        frames = sound_read(rhandle, InBuf, SPEEX_SAMPLES) ;
        if (frames < 0)
        {
            printf("Failed to read speech buffer\n");
            usleep(1);
            break;
        }

        // 将从声卡读取的数据分为两个声道
        sound_split(InBuf, InputBuf[RADIO], InputBuf[PHONE] , SPEEX_SAMPLES * 2);

        /* busy tone detected */
        // 首先检查手机端传来的声音是否是忙音
        if (toneDetecting((short*)InputBuf[PHONE]) > 7){
            printf("Busy tone detected.\n");
            clear_line();
            continue;
        }

        /* status timeout */
        if (time_status)
            if ( time_after(time_sec(), time_status+60)){
                DP(("Status blocked for 60 seconds.\n")); //display??
                clear_line();
                time_status = 0;
            }

        if ((idle_count ++) < 20)
            continue;

        switch(status){
            // 空闲状态
            case LINE_IDLE:
                /* Check for incoming call */
                // 检查关联电话的gpio的电平，为低电平就是有电话打进来了
                if (digitalRead(GPIO_RING) == 0){
                    printf("Entering LINE_INCOMING...\n");

                    if (UserConfig.TelephoneCfg.PasswordEnabled){
                        status = LINE_INCOMING;
                    }
                    else{
                        if (UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER)// 什么模式？
                            ENABLE_PTT();
                        status = LINE_TALKING;
                    }
                    /* record the time */
                    time_status = time(NULL);
                    /* pick up the phone */
                    // 接电话
                    PICKUP_PHONE();
                    retry = 0;
                    /* play greeting sound */
                    // 播放欢迎音频
                    sound_play_wave("file1.wav");
                    /* restart dtmf-detect */
                    dtmfDetecting_init();// 有什么用？
                }

                /* radio with COR */
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_RADIO){
                    if (RADIO_RX()){
                        /* dtmf-detect on the radio channel */
                        password = dtmfDetecting((short*)InputBuf[RADIO]);
                        /* waiting for password */
                        if (password != NULL){
                            printf("password = %s\n", password);
                            /* valid password received */
                            if (*password != 0){
                                for (ii = 0; ii < TELEPHONE_PASS_LENGTH; ii++)
                                    if ( UserConfig.TelephoneCfg.Password[ii] != *(password + ii) )
                                        break;

                                /* End of password */
                                if ( (UserConfig.TelephoneCfg.Password[ii] == '\0') &&
                                        (*(password + ii) == '#') ){ //密码以#结尾,正确就往下执行
                                    printf("Entering LINE_OUTGOING...\n");
#ifdef	MARK
                                    fp = writeWave_init("radio.wav");
#endif
                                    /* wait for radio RX ending */
                                    while (RADIO_RX())
                                        usleep(1);
                                    /* pick up the phone */
                                    PICKUP_PHONE();
                                    /* record the time */
                                    time_status = time(NULL);
                                    /* Enable PTT (0 for enable!) */
                                    ENABLE_PTT();
                                    status = LINE_OUTGOING;
                                    /* remember the time */
                                    audio_start = time(NULL);
                                    break;
                                }
                            }
                            /* restart dtmf-detect */
                            dtmfDetecting_init();
                        }
                    }
                    else
                        dtmfDetecting_init();
                }
                /* RoIP Gateway */
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_GATEWAY){
                    /* Get audio from IP network */
                    if (GetAudioFrame(&AudioFrame)){
                        short SPXBuffer[SPEEX_SAMPLES];

                        /* decode */
                        for(i = 0; i<3; i++)
                            speex_dec(&AudioFrame.data[20*i], 20, &SPXBuffer[160*i]);

                        /* start conversation */
                        if (audio_start == 0)
                        {
                            /* restart dtmf-detect */
                            dtmfDetecting_init();
                            audio_start = time(NULL);
                        }

                        /* dtmf-detect */
                        password = dtmfDetecting(SPXBuffer);
                        /* waiting for password */
                        if (password != NULL){
                            /* valid password received */
                            if (*password != 0){
                                for (ii = 0; ii < TELEPHONE_PASS_LENGTH; ii++)
                                    if ( UserConfig.TelephoneCfg.Password[ii] != *(password + ii) )
                                        break;

                                /* End of password */
                                if ( (UserConfig.TelephoneCfg.Password[ii] == '\0') &&
                                        (*(password + ii) == '#') ){
                                    printf("Entering LINE_OUTGOING...\n");
                                    /* wait for radio RX ending */
                                    sleep(1);
                                    /* pick up the phone */
                                    PICKUP_PHONE();
                                    status = LINE_OUTGOING;
                                    /* record the time */
                                    time_status = time(NULL);
                                    audio_start = time(NULL);
                                    break;
                                }
                            }
                            else
                                /* restart dtmf-detect */
                                dtmfDetecting_init();
                        }
                    }
                    /* no data received */
                    else if (audio_start){
                        /* no data received in 10 second */
                        if ( time_after(time_sec(), audio_start+10)){
                            DP(("Get audio frame timeout,  decoder_hd was set to 0.\n"));
                            rtp_recv_init(0);
                            audio_start = 0;
                        }
                    }
                }
                /* Trbo-Care */
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_TRBO){
                }
                /* Repeater */
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER){
                    if (RADIO_RX()){ //GPIO_RX有数据可读
                        /* dtmf-detect on the radio channel */
                        password = dtmfDetecting((short*)InputBuf[RADIO]);
                        /* waiting for password */
                        if (password != NULL){
                            printf("password = %s\n", password);
                            /* valid password received */
                            if (*password != 0){
                                for (ii = 0; ii < TELEPHONE_PASS_LENGTH; ii++)
                                    if ( UserConfig.TelephoneCfg.Password[ii] != *(password + ii) )
                                        break;

                                /* End of password */
                                if ( (UserConfig.TelephoneCfg.Password[ii] == '\0') &&
                                        (*(password + ii) == '#') ){
                                    printf("Entering LINE_TALKING...\n");
                                    /* pick up the phone */
                                    PICKUP_PHONE();
                                    /* Enable PTT (0 for enable!) */
                                    ENABLE_PTT();
                                    status = LINE_TALKING;
                                    /* record the time */
                                    time_status = time(NULL);
                                    dtmfDetecting_init();

#ifdef	MARK
                                    fp = writeWave_init("repeater.wav");
#endif
                                    break;
                                }
                            }
                            /* restart dtmf-detect */
                            dtmfDetecting_init();
                        }
                    }
                    else
                        dtmfDetecting_init();
                }
                /* Radio without COR */
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_NOCOR){
                    if (sound_speech(InputBuf[RADIO], 0)){ //返回1说明是speech模式, 0是mute模式
                        digitalWrite(GPIO_LED2, HIGH);
                        /* dtmf-detect on the radio channel */
                        password = dtmfDetecting((short*)InputBuf[RADIO]);
                        /* waiting for password */
                        if (password != NULL){
                            printf("password = %s\n", password);
                            /* valid password received */
                            if (*password != 0){
                                for (ii = 0; ii < TELEPHONE_PASS_LENGTH; ii++)
                                    if ( UserConfig.TelephoneCfg.Password[ii] != *(password + ii) )
                                        break;

                                /* End of password */
                                if ( (UserConfig.TelephoneCfg.Password[ii] == '\0') &&
                                        (*(password + ii) == '#') ){
                                    printf("Entering LINE_OUTGOING...\n");
                                    /* wait for radio RX ending */
                                    while (RADIO_RX())
                                        usleep(1);
                                    /* pick up the phone */
                                    PICKUP_PHONE();
                                    /* Enable PTT (0 for enable!) */
                                    ENABLE_PTT();
                                    status = LINE_OUTGOING;
                                    /* record the time */
                                    time_status = time(NULL);
                                    /* remember the time */
                                    audio_start = time(NULL);
                                    break;
                                }
                            }
                            /* restart dtmf-detect */
                            dtmfDetecting_init();
                        }
                    }
                    else{
                        digitalWrite(GPIO_LED2, LOW);
                        dtmfDetecting_init();
                    }
                }
                break;
            case LINE_OUTGOING:
                if ((UserConfig.TelephoneCfg.Mode == TELEPHONE_RADIO) ||
                        (UserConfig.TelephoneCfg.Mode == TELEPHONE_NOCOR) ) {
                    /* play telephone sound for 3 seconds */
                    if (time_after(time(NULL), audio_start + 3)){
                        printf("Entering LINE_DAILING...\n");
                        status = LINE_DAILING;
                        /* record the time */
                        time_status = time(NULL);
                        audio_start = time(NULL);
                        /* Disable PTT */
                        DISABLE_PTT();
                        break;
                    }
                    /* copy phone input to radio output */
                    memcpy( OutputBuf[RADIO], InputBuf[PHONE], SPEEX_SAMPLES * 2 ); //重点!!!
                }
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_GATEWAY){
                    /* play telephone sound for 5 seconds */
                    if (time_after(time(NULL), audio_start + 3)){
                        printf("Entering LINE_DAILING...\n");
                        status = LINE_DAILING;
                        /* record the time */
                        time_status = time(NULL);
                        audio_start = time(NULL);
                        break;
                    }
                    send_audio(InputBuf[PHONE], 1024, 0, 0);
                }
                break;
            case LINE_DAILING: //拨号,之后进入LINE_TALKING
                if (UserConfig.TelephoneCfg.Mode == TELEPHONE_RADIO){
                    /* after radio RX */
                    if (RADIO_RX()){
                        printf("Entering LINE_TALKING...\n");
                        /* record the time */
                        time_status = time(NULL);
                        status = LINE_TALKING;
                        /* restart dtmf-detect */
                        dtmfDetecting_init();
                        break;
                    }
                    /* copy radio input to phone output */
                    memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 ); //重点!!!
                }
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_GATEWAY){
                    /* transfer dial tones to telephone channel */
                    if (GetAudioFrame(&AudioFrame)){ //wave.h里的？
                        short SPXBuffer[SPEEX_SAMPLES];

                        /* decode */
                        for(i = 0; i<3; i++)
                            speex_dec(&AudioFrame.data[20*i], 20, &SPXBuffer[160*i]); //wave.h里的？

                        /* copy radio input to phone output */
                        memcpy( OutputBuf[PHONE], &SPXBuffer, sizeof(SPXBuffer));

                        audio_start = time(NULL);
                    }
                    /* no data in 1 second */
                    else if (time_after(time(NULL), audio_start + 3)){
                        printf("Entering LINE_TALKING...\n");
                        status = LINE_TALKING;
                        /* record the time */
                        time_status = time(NULL);
                        audio_radio = time(NULL);
                        break;
                    }
                }
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_NOCOR){
                    /* after radio RX */
                    if (sound_speech(InputBuf[RADIO], 0)){
                        printf("Entering LINE_TALKING...\n");
                        /* record the time */
                        time_status = time(NULL);
                        status = LINE_TALKING;
                        break;
                    }
                    /* copy radio input to phone output */
                    memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 ); //重点
                }
                break;
            case LINE_INCOMING:
            case LINE_RETRY:
                /* dtmf-detect on the phone channel */
                password = dtmfDetecting((short*)InputBuf[PHONE]);
                /* password got */
                if (password != NULL){
                    printf("password = %s\n", password);
                    /* valid password received */
                    if (*password != 0){
                        for (ii = 0; ii < TELEPHONE_PASS_LENGTH; ii++)
                            if ( UserConfig.TelephoneCfg.Password[ii] != *(password + ii) )
                                break;

                        /* End of password */
                        if ( (UserConfig.TelephoneCfg.Password[ii] == '\0') &&
                                (*(password + ii) == '#') ){
                            printf("Entering LINE_TALKING...\n");
                            /* record the time */
                            time_status = time(NULL);
                            status = LINE_TALKING;
                            audio_telephone = 10;
                            /* PTT always on in repeater mode */
                            if (UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER)
                                ENABLE_PTT();
                            break;
                        }
                    }
                    if (retry++ > 2){
                        /* record the time */
                        time_status = time(NULL);
                        status = LINE_ENDING;
                        /* play ending words */
                        sound_play_wave("file4.wav");
                        break;
                    }

                    status = LINE_RETRY;
                    /* record the time */
                    time_status = time(NULL);
                    printf("Entering LINE_RETRY...\n");
                    /* play retry prompt */
                    sound_play_wave("file3.wav");
                    /* restart dtmf-detect */
                    dtmfDetecting_init();
                }

                /* file to play */
                if (sound_file_stopping){
                    printf("Play stopped, file closed.\n");
                    /* clear phone output */
                    memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);
                    if (sound_file_playing)
                        fclose(sound_file_playing);
                    sound_file_playing = NULL;
                    sound_file_stopping = 0;
                    continue;
                }

                /* Playing file to phone channel */
                if (sound_file_playing){
                    rc = playWave_data(sound_file_playing, OutputBuf[PHONE], SPEEX_SAMPLES * 2);
                    if (rc != 1){
                        printf("Play end, file closed.\n");
                        fclose(sound_file_playing);
                        sound_file_playing = NULL;
                    }
                }

                break;
            case LINE_ENDING:
                /* Playing file to phone channel */
                if (sound_file_playing){
                    rc = playWave_data(sound_file_playing, OutputBuf[PHONE], SPEEX_SAMPLES * 2);
                    if (rc != 1){
                        printf("Play end, file closed.\n");
                        fclose(sound_file_playing);
                        sound_file_playing = NULL;
                        /* clear line after play ending */
                        clear_line();
                    }
                }
                break;
            case LINE_TALKING:
                if (UserConfig.TelephoneCfg.Mode == TELEPHONE_RADIO){
                    /* radio active */
                    if (RADIO_RX()){
                        /* disable telephone, if it's speaking */
                        if (audio_telephone){
                            audio_telephone = 0;
                            DISABLE_PTT();
                            printf("radio start talking...\n");
                            /* record the time */
                            time_status = time(NULL);
                        }
                        /* dtmf-detect on the radio channel */
                        password = dtmfDetecting((short*)InputBuf[RADIO]);
                        /* waiting for password */
                        if (password != NULL){
                            printf("password = %s\n", password);
                            if ( (*password == '*') && (*(password+1) == '#') ) {
                                clear_line();
                                break;
                            }
                        }
                        /* copy radio input to phone output */
                        memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 );
                        /* clear radio output */
                        memset( OutputBuf[RADIO], 0, SPEEX_SAMPLES * 2);
#ifdef MARK
                        writeWave_data(fp, InputBuf[RADIO], SPEEX_SAMPLES * 2);
#endif
                    }
                    else {
                        dtmfDetecting_init();

                        /* wait 480x2=960 samples(120ms) to empty the sound play buffer */
                        if (audio_telephone ++ < 2){
                            /* clear phone output */
                            memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);
                            /* clear radio output */
                            memset( OutputBuf[RADIO], 0, SPEEX_SAMPLES * 2);
                            break;
                        }

                        if (sound_speech(InputBuf[PHONE], 1)){
                            /* tot x 1000 / 60ms  => tot*17 */
                            if ((audio_telephone > 2) && (audio_telephone <= (UserConfig.TelephoneCfg.Tot*17))){
                                /* Enable PTT (0 for enable!) */
                                ENABLE_PTT();
                                //							printf("telephone start talking...\n");
                                /* record the time */
                                time_status = time(NULL);
                            }
                            /* tot~tot+5 second blocked */
                            if ((audio_telephone > (UserConfig.TelephoneCfg.Tot*17)) &&
                                    (audio_telephone <= (UserConfig.TelephoneCfg.Tot*17 + 80))){
                                DISABLE_PTT();
                            }
                            /* >tot+5 second, restart counter */
                            if (audio_telephone > (UserConfig.TelephoneCfg.Tot*17 + 80)){
                                ENABLE_PTT();
                                audio_telephone = 2;
                            }
                        }
                        else{
                            DISABLE_PTT();
                            audio_telephone = 2;
                        }

                        /* copy phone input to radio output */
                        memcpy( OutputBuf[RADIO], InputBuf[PHONE], SPEEX_SAMPLES * 2 );
                        /* clear phone output */
                        memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);
                    }
                } /* Mode radio */
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_GATEWAY){
                    /* get audio from gateway */
                    if (GetAudioFrame(&AudioFrame)){
                        short SPXBuffer[SPEEX_SAMPLES];

                        /* decode */
                        for(i = 0; i<3; i++)
                            speex_dec(&AudioFrame.data[20*i], 20, &SPXBuffer[160*i]);

                        /* copy radio input to phone output */
                        memcpy( OutputBuf[PHONE], &SPXBuffer, sizeof(SPXBuffer));
                        /* disable telephone */
                        audio_telephone = 0;
                        /* dtmf-detect on the radio channel */
                        password = dtmfDetecting(SPXBuffer);
                        /* waiting for password */
                        if (password != NULL){
                            if ( (*password == '*') && (*(password+1) == '#') ) {
                                clear_line();
                                break;
                            }
                        }
                    }
                    /* no data received */
                    else
                        audio_telephone ++;

                    /* 480samples/loop = 60ms/loop, 8x60ms = 480ms */
                    /* data received in 480ms */
                    if (audio_telephone < 8)
                        break;

                    /* restart dtmf detector*/
                    dtmfDetecting_init();

                    /* detect sound from telephone */
                    if (sound_speech(InputBuf[PHONE], 1)){
                        /* send speech to gateway */
                        send_audio(InputBuf[PHONE], 1024, 0, 0);
                    }
                } /* Mode Gateway */
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_TRBO){
                } /* ModeC */
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER){
#if 1
                    /* dtmf-detect on the radio channel */
                    password = dtmfDetecting((short*)InputBuf[RADIO]);
                    /* waiting for password */
                    if (password != NULL){
                        printf("password = %s\n", password);
                        if ( (*password == '*') && (*(password+1) == '#') ) {
                            clear_line();
                            break;
                        }
                        dtmfDetecting_init();
                    }

                    memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 );
                    //memset( OutputBuf[RADIO], 0, SPEEX_SAMPLES * 2);
                    memcpy( OutputBuf[RADIO], InputBuf[PHONE], SPEEX_SAMPLES * 2 );
                    /* record the time */
                    time_status = time(NULL);

#ifdef MARK
                    writeWave_data(fp, InputBuf[RADIO], SPEEX_SAMPLES * 2);
#endif

#else

                    /* if phone speaking, refresh the time */
                    if (sound_speech(InputBuf[PHONE], 1))
                        /* record the time */
                        time_status = time(NULL);

                    /* radio active */
                    if (RADIO_RX()){
                        audio_telephone = 0;
                        /* dtmf-detect on the radio channel */
                        password = dtmfDetecting((short*)InputBuf[RADIO]);
                        /* waiting for password */
                        if (password != NULL){
                            printf("password = %s\n", password);
                            if ( (*password == '*') && (*(password+1) == '#') ) {
                                clear_line();
                                break;
                            }
                        }
                        /* copy radio input to phone output */
                        memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 );
                        /* clear radio output */
                        memset( OutputBuf[RADIO], 0, SPEEX_SAMPLES * 2);
                    }
                    else {
                        dtmfDetecting_init();

                        /* wait 480x2=960 samples(120ms) to empty the sound play buffer */
                        if (audio_telephone ++ < 2){
                            /* clear phone output */
                            memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);
                            /* clear radio output */
                            memset( OutputBuf[RADIO], 0, SPEEX_SAMPLES * 2);
                            break;
                        }

                        /* copy phone input to radio output */
                        memcpy( OutputBuf[RADIO], InputBuf[PHONE], SPEEX_SAMPLES * 2);
                        /* clear phone output */
                        memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);
                    }
#endif
                }
                else if (UserConfig.TelephoneCfg.Mode == TELEPHONE_NOCOR){
                    /* radio active */
                    if (sound_speech(InputBuf[RADIO], 0)){
                        digitalWrite(GPIO_LED2, HIGH);
                        /* disable telephone, if it's speaking */
                        if (audio_telephone){
                            audio_telephone = 0;
                            DISABLE_PTT();
                            /* record the time */
                            time_status = time(NULL);
                            printf("radio start talking...\n");
                        }
                        /* dtmf-detect on the radio channel */
                        password = dtmfDetecting((short*)InputBuf[RADIO]);
                        /* waiting for password */
                        if (password != NULL){
                            if ( (*password == '*') && (*(password+1) == '#') ) {
                                clear_line();
                                break;
                            }
                        }
                        /* copy radio input to phone output */
                        memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 );
                        /* clear radio output */
                        memset( OutputBuf[RADIO], 0, SPEEX_SAMPLES * 2);
                    }
                    else {
                        digitalWrite(GPIO_LED2, LOW);
                        dtmfDetecting_init();

                        /* wait 480x2=960 samples(120ms) to empty the sound play buffer */
                        if (audio_telephone ++ < 2){
                            /* clear phone output */
                            memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);
                            /* clear radio output */
                            memset( OutputBuf[RADIO], 0, SPEEX_SAMPLES * 2);
                            break;
                        }

                        if (sound_speech(InputBuf[PHONE], 1)){
                            /* tot x 1000 / 60ms  => tot*17 */
                            if ((audio_telephone > 2) && (audio_telephone <= (UserConfig.TelephoneCfg.Tot*17))){
                                /* Enable PTT (0 for enable!) */
                                ENABLE_PTT();
                                //							printf("telephone start talking...\n");
                                /* record the time */
                                time_status = time(NULL);
                            }
                            /* tot~tot+5 second blocked */
                            if ((audio_telephone > (UserConfig.TelephoneCfg.Tot*17)) &&
                                    (audio_telephone <= (UserConfig.TelephoneCfg.Tot*17 + 80))){
                                DISABLE_PTT();
                            }
                            /* >tot+5 second, restart counter */
                            if (audio_telephone > (UserConfig.TelephoneCfg.Tot*17 + 80)){
                                ENABLE_PTT();
                                audio_telephone = 2;
                            }
                        }
                        else{
                            DISABLE_PTT();
                            audio_telephone = 2;
                        }

                        /* copy phone input to radio output */
                        memcpy( OutputBuf[RADIO], InputBuf[PHONE], SPEEX_SAMPLES * 2 );
                        /* clear phone output */
                        memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);
                    }
                } /* Mode radio without COR */
                break;
        }

        sound_combine(OutBuf, OutputBuf[RADIO], OutputBuf[PHONE] , SPEEX_SAMPLES * 2);

        frames = sound_write(whandle, OutBuf, SPEEX_SAMPLES) ;

        if (frames < 0) {
            printf("Failed to write speech buffer.\n");
            usleep(1);
            continue;
        }
    }

    sound_delete();
    printf("Sound task exit!\n");
    return NULL;
}

