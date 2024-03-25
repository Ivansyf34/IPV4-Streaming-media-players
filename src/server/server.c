#include "../include/proto.h"
#include "medialib.h"
#include "server_conf.h"
#include "thr_channel.h"
#include "thr_list.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
/*
 * -M  指定多播组
 * -P  指定端口
 * -F  前台运行
 * -D  指定媒体库位置
 * -I  指定网路设备
 * -H  显示帮助
 *
 * */

// 套接字id
int serversd;
// 发送地址(多播地址)
struct sockaddr_in sndaddr;
// 媒体库链表
static struct mlib_listentry_st *list;
// 服务端配置项
struct server_conf_st server_conf = {.rcvport = DEFAULT_PCVPORT, .ifname = DEFAULT_IF, .mediaDir = DEFAULT_MEDIADIR, .runmode = RUN_DAEMON, .mgroup = DEFAULT_MGROUP};
// 长格式命令行解析配置项
struct option opt[] = {
    {"mgroup", 1, NULL, 'M'},
    {"prot", 1, NULL, 'P'},
    {"FORE", 0, NULL, 'F'},
    {"mediadir", 1, NULL, 'D'},
    {"ifconfig", 1, NULL, 'I'},
    {"help", 0, NULL, 'H'},
    {NULL, 0, NULL, 0}};

// 打印帮助信息
static void print_help()
{
    printf("-M  指定多播组\n");
    printf("-P  指定端口\n");
    printf("-F  前台运行\n");
    printf("-D  指定媒体库位置\n");
    printf("-I  指定网路设备\n");
    printf("-H  显示帮助\n");
}

// 守护进程
static int daemonize()
{
    pid_t pid;
    int fd;
    // 创建子进程
    pid = fork();
    if (pid < 0)
    {
        // perror("fork()");
        syslog(LOG_ERR, "[server.c][daemonize]fork():%s", strerror(errno));
        return -1;
    }
    // 父进程退出
    if (pid > 0)
        exit(0);
    // 将输出重定向到/dev/null
    fd = open("/dev/null", O_RDWR);
    if (fd < 0)
    {
        // perror("open()");
        syslog(LOG_WARNING, "[server.c][daemonize]open(): %s", strerror(errno));
        return -2;
    }
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    if (fd > 2)
    {
        close(fd);
    }

    // 创建新会话,脱离终端
    setsid();
    // 改变工作目录
    chdir("/");
    // 重设文件掩码
    umask(0);
    return 0;
}

// 信号处理函数
static void daemon_exit(int s)
{
    // 销毁所有频道线程
    thr_channel_destroyall();
    // 销毁节目单线程
    thr_list_destroy();
    // 销毁媒体库链表
    mlib_freechnlist(list);
    syslog(LOG_WARNING, "signal-%d exit", s);
    // 关闭系统日志
    closelog();
    exit(0);
}

// 套接字初始化
static int socket_init()
{
    struct ip_mreqn mreq; // 多播地址结构体
    int res;              // 返回值

    // 创建报式套接字(UDP)
    serversd = socket(AF_INET, SOCK_DGRAM, 0);
    if (serversd < 0)
    {
        syslog(LOG_ERR, "[server.c][socket_init]socket(): %s", strerror(errno));
        exit(1);
    }

    // 创建多播组
    mreq.imr_ifindex = if_nametoindex(server_conf.ifname);  // 设置网卡索引
    res = inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address); // 设置ip地址
    if (res < 0)
    {
        syslog(LOG_ERR, "[server.c][socket_init]inet_pton():%s", strerror(errno));
        exit(1);
    }
    res = inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr); // 设置多播地址
    if (res < 0)
    {
        syslog(LOG_ERR, "[server.c][socket_init]inet_pton():%s", strerror(errno));
        exit(1);
    }
    if (setsockopt(serversd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0)
    {
        syslog(LOG_ERR, "[server.c][socket_init]setsockopt():%s", strerror(errno));
        exit(1);
    }

    // 设置发送地址
    sndaddr.sin_family = AF_INET;                                     // 地址族
    sndaddr.sin_port = htons(atoi(server_conf.rcvport));              // 端口
    inet_pton(AF_INET, server_conf.mgroup, &sndaddr.sin_addr.s_addr); // ip地址(多播地址)
    return 0;
}

int main(int argc, char **argv)
{
    int index;           // 长格式命令行解析索引
    int i;               // 循环变量
    int res;             // 返回值
    signed char c;       // 命令行解析返回值
    int list_size;       // 节目单大小
    struct sigaction sa; // 信号处理结构体

    // 设置信号处理函数(守护进程退出: SIGINT, SIGQUIT, SIGTERM)
    sa.sa_handler = daemon_exit;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGQUIT);
    sigaddset(&sa.sa_mask, SIGTERM);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    // 打开系统日志
    openlog("netradio", LOG_PID | LOG_PERROR, LOG_DAEMON);

    // 命令行分析
    while (1)
    {
        c = getopt_long(argc, argv, "M:P:FD:I:H", opt, &index);
        fflush(stdout);
        if (c < 0)
            break;
        switch (c)
        {
        case 'M':
            server_conf.mgroup = optarg;
            break;
        case 'P':
            server_conf.rcvport = optarg;
            break;
        case 'F':
            server_conf.runmode = RUN_FOREGROUND;
            break;
        case 'D':
            server_conf.mediaDir = optarg;
            break;
        case 'I':
            server_conf.ifname = optarg;
            break;
        case 'H':
            print_help();
            exit(0);
            break;
        case '?':
            fprintf(stderr, "[server.c][main]getopt_long ?\n");
            break;
        default:
            abort();
            break;
        }
    }

    // 实现守护进程
    if (server_conf.runmode == RUN_DAEMON)
    {
        if (daemonize() < 0)
        {
            exit(1);
        }
    }

    // 前台运行
    else if (server_conf.runmode == RUN_FOREGROUND)
    {
        /*
         * do nothing
         */
    }
    // 非法运行模式
    else
    {
        // fprintf(stderr, "EINVAL\n");
        syslog(LOG_ERR, "[server.c][main] server_conf.runmode EINVAL server_conf.runmode");
        exit(1);
    }

    // socket初始化
    socket_init();

    // 获取频道信息
    res = mlib_getchnlist(&list, &list_size);
    if (res)
    {
        syslog(LOG_ERR, "[server.c][main]mlib_getchnlist(): failed");
        exit(1);
    }
    // 创建节目单线程
    thr_list_create(list, list_size);
    // 创建频道线程
    for (i = 0; i < list_size; i++)
    {
        res = thr_channel_create(list + i);
        if (res)
        {
            printf("[server.c][main] thr_channel_create(): %d failed", i);
            syslog(LOG_ERR, "[server.c][main] thr_channel_create(): %d failed", i);
            exit(1);
        }
    }
    while (1)
        pause();
}
