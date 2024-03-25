#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include "medialib.h"
#include "server_conf.h"
#include "thr_channel.h"
#include "../include/proto.h"
#include "server_conf.h"
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/syslog.h>

//频道线程结构体
struct thr_channel_ent_st
{
    chnid_t chnid;  //频道id
    pthread_t tid;  //线程id
};

//频道线程数组
struct thr_channel_ent_st thr_channel[CHNNR];

//线程id数组下标
static int tid_nextpos = 0;

//发送频道数据线程
static void * thr_channel_snder(void *p)
{
    struct msg_channel_st *buf;         //频道数据结构体
    struct thr_channel_ent_st *me = p;  //频道线程结构体
    ssize_t len;                        //数据长度

    buf = malloc(MSG_CHANNEL_MAX);
    if(buf == NULL)
    {
        syslog(LOG_WARNING, "[thr_channel.c][thr_channel_snder]malloc() : %s", strerror(errno));
        exit(1);
    }

    //设置频道id
    buf->chnid = me->chnid;
    while(1)
    {
        //读取频道数据
        len = mlib_readchn(me->chnid, buf->data, MAX_DATA);

        //发送频道数据
        if(sendto(serversd, buf, len + sizeof(chnid_t), 0, (void *)&sndaddr, sizeof(sndaddr)) < 0)
        {
            syslog(LOG_WARNING, "[thr_channel.c][thr_channel_snder]sendto(); %s", strerror(errno));
        }
        //让出cpu
        sched_yield();
    }
    pthread_exit(NULL);
}


//创建频道线程
int thr_channel_create(struct mlib_listentry_st *ptr)
{
    int err;
    //创建线程
    err = pthread_create(&thr_channel[tid_nextpos].tid, NULL, thr_channel_snder, ptr);
    if(err)
    {
        syslog(LOG_WARNING, "[thr_channel.c][thr_channel_create] pthread_create(): %s", strerror(errno));
        return err;
    }
    thr_channel[tid_nextpos].chnid = ptr->chnid;
    //线程id数组下标自增
    tid_nextpos ++;
    return 0;
}

//销毁频道线程
int thr_channel_destroy(struct mlib_listentry_st * ptr)
{
    int i;
    for(i = 0; i < CHNNR; i ++)
    {
        if(thr_channel[i].chnid == ptr->chnid)
        {
            //取消线程
            if(pthread_cancel(thr_channel[i].tid) < 0)
            {
                syslog(LOG_ERR, "[thr_channel.c][thr_channel_destory]pthread_cancel() failed");
                return ESRCH;
            }
            //等待线程结束,回收资源
            pthread_join(thr_channel[i].tid, NULL);
            thr_channel[i].chnid = -1;
            return 0;
        }
    }
    return 0;
}

//销毁所有频道线程
int thr_channel_destroyall(void)
{
    int i;
    for(i = 0; i < tid_nextpos; i ++)
    {
        if(thr_channel[i].chnid > 0)
        {
            //取消线程
            if(pthread_cancel(thr_channel[i].tid) < 0)
            {
                syslog(LOG_ERR, "[thr_channel.c][thr_channel_destory]pthread_cancel() failed");
                return ESRCH;
            }
            //等待线程结束,回收资源
            pthread_join(thr_channel[i].tid, NULL);
            thr_channel[i].chnid = -1;
        }
    }
    return 0;
}

