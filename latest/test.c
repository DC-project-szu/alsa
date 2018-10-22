#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
void *jianhao(void *arg);
int main()
{
    pthread_t liujianhao;
    pthread_create(&liujianhao, NULL, jianhao, NULL);

    int i;
    for(i=0;i<100;i++)
    {
        printf("liujianhao niubi\n");
        usleep(1);
    }
        

    pthread_join(liujianhao, NULL);
    return 0;
}

void *jianhao(void *arg)
{
    int i;
    for(i=0;i<100;i++)
    {
        printf("chenjinzhuo niubi\n");
        usleep(1);
    }
        
}