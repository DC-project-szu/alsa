#include <stdio.h>
#include <error.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SPEEX_SAMPLES (480*8)
/* 可以修改文件来看看结果有什么不同 */
/* #define FILENAME "tone1.wav" */

#define SIZE 30

/* 函数前置声明 */
int toneDetecting(int input_array[]);

int main(int argc, char *argv[])
{
    int simple[SPEEX_SAMPLES];
    /* int fd; */
    ssize_t numRead;

    int i = 0;
    int ret;
    /* int ret; */

    /* 打开文件 */
    /* fd = open(FILENAME, O_RDONLY); */
    /* fd = open(argv[1], O_RDONLY); */
    /* if(fd == -1) */
    /*     perror("open error\n"); */

    /* 读取文件 */
    while((numRead = read(0, simple, SPEEX_SAMPLES)) > 0)
    {
        if(i > SIZE-1)
            break;
        ret = toneDetecting(simple);
        /* printf("ToneChanges = %d\n", ret); */
        if(ret == 2){
            /* if(close(fd) == -1) */
            /*     perror("close error\n"); */
            printf("It is a busytone!!\n");
            return 0;
        }
    }

    if(numRead == -1)
        perror("read error\n");

    /* if(close(fd) == -1) */
    /*     perror("close error\n"); */


    /* for(int j = 0; j < i; j++) */
    /* { */
    /*     printf("ret[i] = %d\n", ret[j]); */
    /* } */

    printf("It is not a busytone!!\n");
    return 0;
}
