#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define    FINISH_FLAG    "FILE_TRANSPORT_FINISH"
#define    MAXLINE        1024

/* use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>

#include <sys/stat.h>
#include <signal.h>

/* 为了能在信号处理函数（即sigHandler）使用, 所以设为全局*/
pid_t childpid;

/* 信号处理函数 */
static void sigHandler(int sig)
{
    /* 杀死子进程 */
    kill(childpid, SIGINT);
    printf("after kill child\n");
}


static void sigHandlerChild(int sig)
{
    exit(0);
}

int write_pcm(char *filename)
{
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir;
    snd_pcm_uframes_t frames;
    char *buffer;
    int size;

    /* open PCM device for recording (capture) */
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* alloc a hardware params object */
    snd_pcm_hw_params_alloca(&params);

    /* fill it with default values */
    snd_pcm_hw_params_any(handle, params);

    /* interleaved mode */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    /* signed 16 bit little ending format */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

    /* two channels */
    snd_pcm_hw_params_set_channels(handle, params, 2);

    /* 44100 bits/second sampling rate (CD quality) */
    val = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    /* set period size to 32 frames */
    frames = 32;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "unable to set hw params: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 4;
    buffer = (char *)malloc(size);

    /* we want to loop for 5 seconds */
    snd_pcm_hw_params_get_period_time(params, &val, &dir);

    int filefd = open(filename, O_CREAT | O_RDWR, 0666);
    if(filefd == -1)
        perror("open error\n");

    /* 捕捉SIGINT信号（ctrl-c） */
    if(signal(SIGINT, sigHandlerChild) == SIG_ERR)
        perror("signal error\n");

    while (1) {
        rc = snd_pcm_readi(handle, buffer, frames);
        if (rc == -EPIPE) {
            /* EPIPE means overrun */
            fprintf(stderr, "overrun occured\n");
            snd_pcm_prepare(handle);
        } else if (rc < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        } else if (rc != (int)frames) {
            fprintf(stderr, "short read, read %d frames\n", rc);
        }

        /* 把数据写到标准输出 */
        rc = write(filefd, buffer, size);
        if (rc != size) {
            fprintf(stderr, "short write: wrote %d bytes\n", rc);
        }
    }

    printf("can't see me\n");

    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    close(filefd);
    free(buffer);
    return 0;
}



void usage(char *command)
{
    printf("usage :%s ipaddr portnum filename\n", command);
    exit(0);
}



int main(int argc,char **argv)
{
    FILE *fp;
    struct sockaddr_in serv_addr;
    char buf[MAXLINE];
    int sock_id;
    int read_len;
    int send_len;
    int serv_addr_len;
    int i_ret;
    int i;

    if (argc != 4) {
        usage(argv[0]);
    }

    /* 捕捉SIGINT信号（ctrl-c） */
    if(signal(SIGINT, sigHandler) == SIG_ERR)
        perror("signal error\n");

    pid_t pid = fork();
    if(pid == -1)
        perror("fork error\n");
    if(pid == 0){
        write_pcm(argv[3]);
    }

    childpid = pid;

    printf("father is waiting.....\n");
    waitpid(pid, NULL, 0);
    printf("after wait.....\n");


    if ((fp = fopen(argv[3],"r")) == NULL) {
        perror("Open file failed\n");
        exit(0);
    }

    /* 创建UDP套接字 */
    if ((sock_id = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Create socket failed");
        exit(0);
    }

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);
    serv_addr_len = sizeof(serv_addr);

    /* 这个连接函数与TCP下的结果不一样，详见UNPv1 P196*/
    /* 仅仅是为了在发送和接受函数中不用指定目的IP地址和端口号 */
    i_ret = connect(sock_id, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    if (-1 == i_ret) {
        perror("Connect socket failed!\n");
        exit(0);
    }

    bzero(buf, MAXLINE);
    while ((read_len = fread(buf, sizeof(char), MAXLINE, fp)) > 0) {
        send_len = send(sock_id, buf, read_len, 0);
        if (send_len < 0){
            perror("Send data failed\n");
            exit(0);
        }
        bzero(buf, MAXLINE);
    }
    fclose(fp);

    /* 在最后发送一个结束标志 */
    bzero(buf, MAXLINE);
    strcpy(buf, FINISH_FLAG);
    buf[strlen(buf)] = '\0';
    send_len = send(sock_id, buf, strlen(buf)+1, 0);
    if (send_len < 0) {
        printf("Finish send the end string\n");
    }
    close(sock_id);
    printf("Send finish\n");
    return 0;
}
