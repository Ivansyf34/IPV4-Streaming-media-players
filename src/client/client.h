#ifndef CLIENT_H__
#define CLIENT_H__
#define DEFAULT_PLAYER "/usr/bin/mpg123 - > /dev/null"
#define IF_INDEX "wlan0"
struct client_conf_st   //客户端配置选项
{
    char *rcvport;      //接收端口
    char *mgroup;       //多播组
    char *playercmd;    //播放器命令行
};


extern struct client_conf_st client_conf;   //声明客户端配置项选项

#endif
