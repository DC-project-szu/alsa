#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 1024

void usage(char *command)
{
    printf("usage :%s ipaddr portnum filename\n", command);
    exit(0);
}


int main(int argc,char **argv)
{
    struct sockaddr_in serv_addr;
    char buf[MAXLINE];
    int sockfd;
    int read_len;
    int send_len;
    FILE *fp;
    int i_ret;

    if (argc != 4) {
        usage(argv[0]);
    }

    if ((fp = fopen(argv[3],"r")) == NULL) {
        printf("Open %s failed\n", argv[3]);
        exit(0);
    }

    /* 创建套接字 */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Create socket failed\n");
        exit(0);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

    /* 请求连接 */
    i_ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    if (-1 == i_ret) {
        printf("Connect socket failed\n");
        return -1;
    }

    bzero(buf, MAXLINE);
    /* 打开要发送的文件并发送 */
    while ((read_len = fread(buf, sizeof(char), MAXLINE, fp)) > 0 ) {
        send_len = send(sockfd, buf, read_len, 0);
        if (send_len < 0) {
            printf("Send file failed\n");
            exit(0);
        }
        bzero(buf, MAXLINE);
    }

    fclose(fp);
    close(sockfd);
    printf("Send Finish\n");
    return 0;
}
