#include <alsa/asoundlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <wiringPi.h>

#define RATE 22333
#define CHANNEL 2
#define FRAMES 128

#define SIZE CHANNEL*FRAMES*1

#define NBUFF 4

long counter = 0;
long second  = 2;

struct {
    char buffer[1024];
    sem_t nempty, nstored;
    char flag;
} shared;

char* pRead 	= shared.buffer;
char* pWrite	= shared.buffer;
void* captureData(void *arg);
void* playbackData(void *arg);
void* press(void *arg);

int main()
{
    pthread_t	tid_captureData, tid_playbackData, tid_press;

    //Initial
    sem_init(&shared.nempty, 0, NBUFF);
    sem_init(&shared.nstored, 0, 0);
    shared.flag = 1;
    //initiate gpio to make speaker working at receiving mode
    wiringPiSetup();
    //pinMode(0, OUTPUT);
    //digitalWrite(0, LOW);

    //create thread, capture data and save to buffer
    pthread_create(&tid_captureData, NULL, captureData, NULL);

    //create thread, get data from buffer and playback
    pthread_create(&tid_playbackData, NULL, playbackData, NULL);

    pthread_create(&tid_press, NULL, press, NULL);

    pthread_join(tid_playbackData, NULL);
    pthread_join(tid_captureData, NULL);
    pthread_join(tid_press, NULL);

    sem_destroy(&shared.nempty);
    sem_destroy(&shared.nstored);
    exit(0);
}

void* press(void *arg)
{
    while(1)
    {
        if(getchar() == '\n')
        {
            fflush(stdin);
            if(shared.flag == 0)
            {
                shared.flag = 1;
                usleep(1000);
                pinMode(0, OUTPUT);
                digitalWrite(0, LOW);
                printf("please speak");
            }
            else
            {
                shared.flag = 0;
                digitalWrite(0, HIGH);
                pinMode(0, INPUT);
                printf("please listen");
            }
        }
    }
}

void* captureData(void *arg)
{
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val = RATE;
    int dir;
    snd_pcm_uframes_t frames;

    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);

    if(rc < 0)
    {
        printf("unable to open pcm device:%s\n", snd_strerror(rc));
        exit(1);
    }
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
    snd_pcm_hw_params_set_channels(handle, params, 2);
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    rc = snd_pcm_hw_params(handle, params);
    if(rc < 0)
    {
        printf("unable to set hw parameters:%s\n", snd_strerror(rc));
        exit(1);
    }
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    snd_pcm_hw_params_get_period_time(params, &val, &dir);

    while(1)
    {
        frames = FRAMES;
        sem_wait(&shared.nempty);

        rc = snd_pcm_readi(handle, pWrite, frames);

        if(rc !=(int)frames)
        {
            usleep(1000);
            snd_pcm_prepare(handle);
            sem_post(&shared.nempty);
            continue;
        }

        pWrite += 256;
        if(pWrite - 1024 == shared.buffer)
            pWrite = shared.buffer;
        sem_post(&shared.nstored);
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
}

void* playbackData(void *arg)
{
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val = RATE;
    int dir;
    snd_pcm_uframes_t frames;

    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

    if(rc < 0)
    {
        printf("unable to open pcm device:%s\n", snd_strerror(rc));
        exit(1);
    }
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
    snd_pcm_hw_params_set_channels(handle, params, 2);
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    rc = snd_pcm_hw_params(handle, params);
    if(rc < 0)
    {
        printf("unable to set hw parameters:%s\n", snd_strerror(rc));
        exit(1);
    }
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    snd_pcm_hw_params_get_period_time(params, &val, &dir);

    while(1)
    {
        frames = FRAMES;
        sem_wait(&shared.nstored);

        rc = snd_pcm_writei(handle, pRead, frames);

        if(rc !=(int)frames)
        {
            usleep(1000);
            snd_pcm_prepare(handle);
            sem_post(&shared.nstored);
            continue;
        }

        pRead += 256;
        if(pRead - 1024 == shared.buffer)
            pRead = shared.buffer;
        sem_post(&shared.nempty);

        if(0 == ++counter % 175)
            second++, printf("%ld\n", second);
        if(0 == second % 50)
            digitalWrite(0, HIGH);
        if(0 == (second / 51) - (second % 51))
            digitalWrite(0, LOW);
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
}
