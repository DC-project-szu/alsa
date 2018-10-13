#include <alsa/asoundlib.h>
#include <stdio.h>
#include <unistd.h>

#define SPEEX_SAMPLES	480

void set_volume();
int sound_init(void);
void sound_delete();
int sound_write(	snd_pcm_t * 	pcm, void * 	bufs, snd_pcm_uframes_t 	size );
static int xrunRecovery(snd_pcm_t *rc, int err);

snd_pcm_t *rhandle, *whandle;
snd_pcm_hw_params_t *rparams, *wparams;

void main(int argc, char* argv[])
{
    int rc;
    rc=open(argv[1],O_RDONLY);
    printf("%s,%s,%s\n",argv[0],argv[1],argv[2]);
    void	*OutBuf;
    int frames;
    int i;
    ssize_t flag;

    sound_init();
    OutBuf= alloca(SPEEX_SAMPLES * 2 * 2);
    while(1)
    {
        flag=read(rc,OutBuf,SPEEX_SAMPLES * 2 * 2);
        if(flag<=0)
            break;
        //OutputBuf[RADIO]  		= alloca(SPEEX_SAMPLES * 2);
        //OutputBuf[PHONE] 			= alloca(SPEEX_SAMPLES * 2);
        frames = sound_write(whandle, OutBuf, SPEEX_SAMPLES) ;

        if (frames < 0)
        {
            printf("Failed to write speech buffer.\n");
        }
    }
    sound_delete();
}

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

int sound_init(void)
{
    int rc;
    int dir = 0;

    unsigned int val;
    snd_pcm_uframes_t frames;

    /* Init the capture handle */
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

    //set_volume();

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
