#include <stdio.h>
#include <error.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SPEEX_SAMPLES (480*5)
/* 可以修改文件来看看结果有什么不同 */
/* #define FILENAME "./tone1.wav" */

/* 函数前置声明 */
int toneDetecting(int input_array[]);

int main(int argc, char *argv[])
{
    int simple[SPEEX_SAMPLES];
    int fd;
    ssize_t numRead;

    /* 打开文件 */
    fd = open(argv[1], O_RDONLY);
    if(fd == -1)
        perror("open error\n");

    /* 读取文件 */
    while((numRead = read(fd, simple, SPEEX_SAMPLES)) > 0)
    {
        int ret = toneDetecting(simple);
        printf("ToneChanges = %d\n", ret);
    }

    if(numRead == -1)
        perror("read error\n");

    if(close(fd) == -1)
        perror("close error\n");

    return 0;
}
