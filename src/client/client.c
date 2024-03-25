#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include "../include/proto.h"
#include "client.h"

/*
 * -M --mgroup 指定多播组
 * -P --port   指定接收端口
 * -H --help   显示帮助
 * -p --player 指定播放器
 *
 * */


//长格式命令行解析配置项
 struct option opt[] = {{"port", 1, NULL, 'P'}, {"mgroup", 1, NULL, 'M'}, {"player", 1, NULL, 'p'},\
        {"help", 0, NULL, 'H'}, {NULL, 0, NULL, 0}};

//初始化配置项
struct client_conf_st client_conf = {\
    .rcvport = DEFAULT_PCVPORT,\
        .mgroup = DEFAULT_MGROUP,\
        .playercmd = DEFAULT_PLAYER};

// 打印帮助信息
static void print_help()
{
    printf("-M --mgroup 指定多播组\n");
    printf("-P --port   指定接收端口\n");
    printf("-H --help   显示帮助\n");
    printf("-p --player 指定播放器\n");

}
//写够多少个字节的函数，防止写的字节不够
static size_t writen(int fd, const char *buff, size_t size)
{
    int pos = 0, len;
    do
    {
        len = write(fd, buff + pos, len);
        if(len < 0)
        {
            if(errno == EINTR)
                continue;
            perror("write()");
            return -1;
        }
        pos += len;
        len = size - pos;
    }while(len);
    return size;
}

int main(int argc, char **argv)
{
    int sd;                                   //socket id
    int index;                                //长格式参数的索引
    int val;                                  //setsockopt()的参数
    int res;                                  //返回值
    int pd[2];                                //管道
    signed char c;                            //getopt_long()的返回值
    pid_t pid;                                //子进程的pid
    ssize_t len;                              //recvfrom接受的数据真实长度
    int chosenid;                             //选择的频道id
    struct ip_mreqn ipmreq;                   //加入多播组结构体
    struct sockaddr_in laddr;                 //本地地址
    struct msg_list_st *msg_list;             //频道列表
    struct sockaddr_in serveraddr, raddr;     //服务器地址，发送端地址
    socklen_t serverlen, raddrlen;            //服务器地址长度，发送端地址长度
    struct msg_listentry_st *pos;             //频道列表的指针
    struct msg_channel_st *msg_channel;       //频道结构体
    /*
     * 初始化
     * 级别：默认值，配置文件，环境变量， 命令行参数
     * */
    while(1)
    {
        c = getopt_long(argc, argv, "P:M:p:H", opt, &index);       //解析命令行参数
        if(c < 0)
            break;
        switch(c)
        {
            case 'P':
                client_conf.rcvport = optarg;
                break;
            case 'M':
                client_conf.mgroup = optarg;
                break;
            case 'p':
                client_conf.playercmd = optarg;
                break;
            case 'H':
                print_help();
                exit(0);
                break;
            default:
                abort();
                break;
        }
    }
    //创建报式socket(udp), AF_INET 指定了 IPv4 地址族，SOCK_DGRAM 表示该套接字是一个数据报套接字，用于支持无连接的 UDP 通信
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0)
    {
        perror("[client.c][mian]socket()");
        exit(1);
    }

    //加入多播组
    res = inet_pton(AF_INET, client_conf.mgroup, &ipmreq.imr_multiaddr); //多播地址
    if(res < 0)
    {
        perror("[client.c][main]inet_pton()");
        exit(1);
    }
    res = inet_pton(AF_INET, "0.0.0.0", &ipmreq.imr_address); //本地地址
    if(res < 0)
    {
        perror("[client.c][main]inet_pton()");
        exit(1);
    }
    ipmreq.imr_ifindex = if_nametoindex(IF_INDEX); //网卡索引
    res = setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &ipmreq, sizeof(ipmreq));
    if(res < 0)
    {
        perror("[client.c][main]setsockopt()");
        exit(1);
    }

    //设置回环
    val = 1;
    res = setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val));
    if(res < 0)
    {
        perror("[client.c][main]setsockopt()");
        exit(1);
    }

    //绑定本机地址和端口
    laddr.sin_port = htons(atoi(client_conf.rcvport)); //端口
    laddr.sin_family = AF_INET; //地址族
    res = inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr); //地址
    if(res < 0)
    {
        perror("[client.c][main]inet_pton()");
        exit(1);
    }
    res = bind(sd, (void *) &laddr, sizeof(laddr));
    if(res < 0)
    {
        perror("[client.c][main]bind()");
        exit(1);
    }
    //套接字可以接收到发送到多播组的数据，并且绑定了本地地址和端口，以便应用程序可以在该地址和端口上接收数据

    //创建管道
    if(pipe(pd) < 0)
    {
        perror("[client.c][main]pipe()");
        exit(1);
    }

    //创建子进程
    pid = fork();
    if(pid < 0)
    {
        perror("[client.c][main]fork()");
        exit(1);
    }
    //子进程：调用解码器
    if(pid == 0)
    {
        close(sd);           //子进程不需要socket
        close(pd[1]);        //子进程不需要写管道
        dup2(pd[0], 0);      //将管道的读端重定向到标准输入
        if(pd[0] > 0)        //关闭原来的读端
            close(pd[0]);
        execl("/bin/sh", "sh", "-c", client_conf.playercmd, NULL); //调用解码器
        perror("[client.c][main]execl"); //execl调用失败(如果调用成功，不会执行到这里)
    }

    //父进程：从网络上收包，发送给子进程
    else
    {
        //关闭写端
        close(pd[0]);

        //收节目单
        msg_list = malloc(MSG_LIST_MAX);
        if(msg_list == NULL)
        {
            perror("[client.c][main]malloc()");
            exit(1);
        }
        //接受协议结构体地址的长度(一定要初始化，否则会出现错误)
        serverlen = sizeof(serveraddr);
        while(1)
        {
            //接受节目单列表
            len = recvfrom(sd, msg_list, MSG_LIST_MAX, 0, (void *)&serveraddr, &serverlen);
            //判断数据长度是否合法
            if(len < sizeof(struct msg_list_st))
            {
                fprintf(stderr, "[client.c][main]message is too small\n");
                continue;
            }
             //判断是否是节目单频道id
            if(msg_list->chnid != LISTCHNID)
            {
                fprintf(stderr, "[client.c][main]chnid is not match\n");
                continue;
            }
            break;
        }

        //打印选择频道
        for(pos = msg_list->entry; (char *)pos < ((char *)msg_list + len); pos = (struct msg_listentry_st *)((char*)pos + ntohs(pos->len)))
        {
            printf("[client.c][main]channel %d: %s\n", pos->chnid, pos->desc);
        }
        free(msg_list);

        //选择频道
        while(1)
        {
            res = scanf("%d", &chosenid);
            if(res == 1) break;
            if(res != 1)
                exit(1);
        }
        printf("[client.c][main]-chosenid:%d\n", chosenid);


        //收频道包，发送给子进程
        msg_channel = malloc(MSG_CHANNEL_MAX);
        if(msg_channel == NULL)
        {
            perror("[client.c][main]malloc()");
            exit(1);
        }
       //接受协议结构体地址的长度(一定要初始化，否则会出现错误)
        raddrlen = sizeof(raddr);
        while(1)
        {
            //接受频道包
            len = recvfrom(sd, msg_channel, MSG_CHANNEL_MAX, 0, (void *)&raddr, &raddrlen);
            if(len < 0) perror("[client.c][main]recvfrom()" );
            printf("[client.c][main]recvfrom ------------------msg-len: %ld------------------\n",len);
            //判断源地址和端口和之前收到的节目单频道是否一致
            if(raddr.sin_addr.s_addr != serveraddr.sin_addr.s_addr || raddr.sin_port != serveraddr.sin_port)
            {
                fprintf(stderr, "[client.c][main]Ignore: address not match.\n");
                continue;
            }
            //判断数据长度是否合法
            if(len < sizeof(struct msg_channel_st))
            {
                fprintf(stderr, "[client.c][main]Ignore: message too small . \n");
                continue;
            }
            //如果是之前选择的频道,把数据通过管道发给子进程
            if(msg_channel->chnid == chosenid)
            {
                fprintf(stdout, "[client.c][main]accepted msg:%d received.\n", msg_channel->chnid);
                res = writen(pd[1], (char *)msg_channel->data, len - sizeof(chnid_t));
                if(res < 0)
                {
                    exit(1);
                }
            }
        }
        free(msg_channel);
        close(sd);

    }
    exit(0);
}
