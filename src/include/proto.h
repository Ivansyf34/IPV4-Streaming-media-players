#ifndef PROTO_H__
#define PROTO_H__

#include "site_type.h"
#define DEFAULT_MGROUP  "224.2.2.2"             //组播ip
#define DEFAULT_PCVPORT "1989"                  //组播端口
#define CHNNR           100                     //频道数量
#define LISTCHNID       0                       //列表频道id
#define MINCHNID        1                       //最小频道id
#define MAXCHINID       (MINCHNID + CHNNR - 1)  // 最大频道id

#define MSG_CHANNEL_MAX (65536 - 20 - 8)                     //一个频道数据包的最大数据量
#define MAX_DATA        (MSG_CHANNEL_MAX - sizeof(chnid_t))  //一个频道数据包包含有效数据的最大数据量

#define MSG_LIST_MAX    (65536 - 20 - 8)                     //一个频道列表数据包的最大数据量
#define MAX_ENTRY       (MSG_LIST_MAX - sizeof(chnid_t))     //一个频道列表数据包有效数据的最大数据量

//频道数据结构体
struct msg_channel_st
{
    chnid_t chnid;
    uint8_t data[1];
}__attribute__((packed));

//频道列表项结构体
struct msg_listentry_st
{
    chnid_t chnid;
    uint16_t len;
    uint8_t desc[1];
}__attribute__((packed));

//频道列表结构体
struct msg_list_st
{
    chnid_t chnid;
    struct msg_listentry_st entry[1];
}__attribute__((packed));

#endif 
