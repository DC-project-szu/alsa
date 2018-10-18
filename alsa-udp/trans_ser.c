#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define    FINISH_FLAG    "FILE_TRANSPORT_FINISH"
#define    MAXLINE        1024

/* use the newer ALSA_API */
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>


int open_pcm(char *filename)
{
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir;
    snd_pcm_uframes_t frames;
    int size;
    char *buffer;

    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open PCM device: %s\n",
                snd_strerror(rc));
        exit(1);

    }

    /* alloc hardware params object */
    snd_pcm_hw_params_alloca(&params);

    /* fill it with default values */
    snd_pcm_hw_params_any(handle, params);

    /* interleaved mode */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    /* signed 16 bit little ending format */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

    /* two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, 2);

    /* 44100 bits/second sampling rate (CD quality) */
    val = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    /* set period size t 32 frames */
    frames = 32;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    /* write params to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "unable to set hw params: %s\n",
                snd_strerror(rc));
        exit(1);
    }

    /* use buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 4; //2 bytes/sample, 2 channels
    buffer = (char *)malloc(size);

    snd_pcm_hw_params_get_period_time(params, &val, &dir);


    int filefd = open(filename, O_RDWR);
    if(filefd == -1)
        perror("open error\n");


    while (1) {
        rc = read(filefd, buffer, size);
        if (rc == 0) {
            fprintf(stderr, "end of file on input\n");
            break;
        } else if (rc != size) {
            fprintf(stderr, "short read: read %d bytes\n", rc);
        }

        rc = snd_pcm_writei(handle, buffer, frames);
        if (rc == -EPIPE) {
            /* fprintf(stderr, "underrun occured\n"); */
            snd_pcm_prepare(handle);
        } else if (rc < 0) {
            fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
        } else if (rc != (int)frames) {
            fprintf(stderr, "short write, write %d frames\n", rc);
        }
    }

    /* allow any pending sound samples to be transferred */
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return 0;
}



void usage(char *command)
{
    printf("usage :%s portnum filename\n", command);
    exit(0);
}


int main(int argc,char **argv)
{
    struct sockaddr_in serv_addr;
    struct sockaddr_in clie_addr;
    char buf[MAXLINE];
    int sock_id;
    int recv_len;
    int clie_addr_len;
    FILE *fp;

    if (argc != 3) {
        usage(argv[0]);
    }

    if ((fp = fopen(argv[2], "w")) == NULL) {
        perror("Creat file failed");
        exit(0);
    }

    if ((sock_id = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
        perror("Create socket failed\n");
        exit(0);
    }

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_id,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0 ) {
        perror("Bind socket faild\n");
        exit(0);
    }

    clie_addr_len = sizeof(clie_addr);
    bzero(buf, MAXLINE);
    while (recv_len = recvfrom(sock_id, buf, MAXLINE, 0,(struct sockaddr *)&clie_addr, &clie_addr_len)) {
        if(recv_len < 0) {
            printf("Recieve data from client failed!\n");
            break;
        }
        printf("#");
        /* 当接受到文件结束的标志就结束循环 */
        if (strstr(buf, FINISH_FLAG) != NULL) {
            printf("\nFinish receiver finish_flag\n");
            break;
        }
        int write_length = fwrite(buf, sizeof(char), recv_len, fp);
        if (write_length < recv_len) {
            printf("File write failed\n");
            break;
        }
        bzero(buf, MAXLINE);
    }

    printf("Finish recieve\n");

    open_pcm(argv[2]);

    fclose(fp);
    close(sock_id);
    return 0;
}
