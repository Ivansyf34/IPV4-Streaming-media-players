#include "medialib.h"
#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <sys/syslog.h>
#include <syslog.h>
#include "server_conf.h"
#include <stdlib.h>
#include <glob.h>
#include "mytbf.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define PATHSIZE 1024                  //路径长度
#define DESCSIZE 1024                  //描述文件长度
#define MP3_BITRATE 1024* 64           //MP3比特率
#define DESC_TEXT_NAME   "desc.text"   //描述文件名
#define MEDIA_EXTEND     "*.mp3"       //媒体文件扩展名

//一个媒体文件的信息
struct channel_context_st
{
    chnid_t chnid;   //频道号
    char *desc;      //描述信息
    glob_t mp3glob;  //媒体文件路径
    int pos;         //当前播放位置(如有多个媒体文件, 记录当前播放的是第几个)
    int fd;          //文件描述符
    off_t offset;    //文件偏移量
    mytbf_t *tbf;    //令牌桶
};


static struct channel_context_st channels[MAXCHINID +  1];   //频道数组(频道号从1开始, 0号不用)
static int count = 0;                                        //读取的频道数量

//根据路径获取频道信息
static struct channel_context_st* path2entry(char *path)
{
    chnid_t id;                       //频道号
    FILE *fd;                         //文件描述符
    char newpath[PATHSIZE];           //拼接路径
    char linebuf[DESCSIZE];           //描述信息
    struct channel_context_st *me;    //频道信息结构体指针
    static chnid_t cur_id = MINCHNID; //当前频道号

    //清空缓冲区
    memset(linebuf, 0 ,DESCSIZE);
    memset(newpath, 0, PATHSIZE);

    //加上描述文件名
    snprintf(newpath, PATHSIZE, "%s/%s", path, DESC_TEXT_NAME);
    //打开描述文件并读取第一行
    fd = fopen(newpath, "r");
    if(fd < 0)
    {
        syslog(LOG_INFO, "[medialib.c][path2entry]fopen(): %s", strerror(errno));
        fclose(fd);
        return  NULL;
    }
    if(fgets(linebuf, DESCSIZE, fd) == NULL)
    {
        syslog(LOG_INFO, "[medialib.c][path2entry]fgets(): %s", strerror(errno));
        fclose(fd);
        return NULL;
    }
    fclose(fd);

    //开辟空间并初始化
    me = malloc(sizeof(*me));
    if(me == NULL)
    {
        syslog(LOG_INFO, "[medialib.c][path2entry]malloc(): %s", strerror(errno));
        return NULL;
    }

    //初始化令牌桶
    me->tbf = mytbf_init(MP3_BITRATE / 8, MP3_BITRATE / 8 * 10);
    if(me -> tbf == NULL)
    {
        syslog(LOG_INFO, "[medialib.c][path2entry]mytbf_init(): failed");
        free(me);
        return NULL;
    }
    //深复制描述信息(strdup是通过malloc开辟空间并复制字符串, 所以需要free)
    me->desc = strdup(linebuf);
    if(me->desc == NULL)
    {
        syslog(LOG_INFO, "[medialib.c][path2entry]strdup(): %s", strerror(errno));
        free(me);
        return NULL;
    }

    //清空缓冲区
    memset(newpath, 0 ,PATHSIZE);
    //加上媒体文件扩展名
    snprintf(newpath, PATHSIZE, "%s/%s", path, MEDIA_EXTEND);
    //获取该路径下的所有媒体文件
    if(glob(newpath, 0, NULL, &me->mp3glob))
    {
        syslog(LOG_WARNING, "[medialib.c][path2entry]glob(): the %s file is not found", MEDIA_EXTEND);
        free(me->desc);
        free(me);
        return NULL;
    }

    //初始化频道信息
    me->pos = 0;
    me->offset = 0;
    me->fd = open(me->mp3glob.gl_pathv[me->pos], O_RDONLY);
    if(me->fd < 0)
    {
        syslog(LOG_ERR, "[medialib.c][path2entry]open(): %s", strerror(errno));
        free(me->desc);
        free(me);
        return NULL;
    }
    me->chnid = cur_id;
    //频道号自增
    cur_id ++;
    return me;
}

//释放频道信息
static int free_path2entry_val(struct channel_context_st *ptr)
{
    free(ptr->desc);
    free(ptr);
    return 0;
}


//获取频道列表
int mlib_getchnlist(struct mlib_listentry_st **result, int *size)
{
    char path[PATHSIZE];                  //路径
    int i;                                //循环变量
    glob_t globres;                       //glob结构体
    struct channel_context_st *res;       //频道信息结构体指针
    struct mlib_listentry_st *ptr;        //频道列表结构体指针

    //初始化频道数组(-1表示该频道未被使用)
    for(i = 0; i < MAXCHINID + 1; i ++)
    {
        channels[i].chnid = -1;
    }

    //拼接路径
    snprintf(path, PATHSIZE, "%s/*", server_conf.mediaDir);
    //获取路径下的所有文件
    if(glob(path, 0, NULL, &globres))
    {
        return -1;
    }

    //开辟空间
    ptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
    if(ptr == NULL)
    {
        syslog(LOG_ERR, "malloc(): error");
        return -1;
    }

    //遍历路径下的所有文件
    for(i = 0; i < globres.gl_pathc; i ++)
    {
        //获取单个频道信息
        res = path2entry(globres.gl_pathv[i]);
        if(res != NULL)
        {
            syslog(LOG_DEBUG, "[medialib.c][mlib_getchnlist] chnid: %d, desc: %s", res->chnid, res->desc);
            //将频道信息存入频道数组
            channels[count + 1] = *res;
            ptr[count].chnid = res->chnid;
            ptr[count].desc = strdup(res->desc);
            //释放频道信息
            free_path2entry_val(res);
            count ++;
        }
    }

    //以count的大小重新分配空间
    *result = realloc(ptr, sizeof(struct mlib_listentry_st) * count);
    if(*result == NULL)
    {
        syslog(LOG_ERR, "[medialib.c][mlib_getchnlist]realloc(): failed");
        return -1;
    }
    *size = count;
    return 0;
}


//释放频道列表
int mlib_freechnlist(struct mlib_listentry_st *result)
{
    int i = 0;
    for(i = 0; i < count; i ++)
    {
        free(result[i].desc);
    }
    free(result);
    count = 0;
    return 0;
}

//获取频道中下一个媒体文件
static int open_next(chnid_t chnid)
{
    //如果频道中只有一个媒体文件, 则不需要切换
    if(channels[chnid].mp3glob.gl_pathc == 1)
    {
        return 0;
    }
    channels[chnid].pos ++;
    //如果已经是最后一个媒体文件, 则切换到第一个
    if(channels[chnid].pos >= channels[chnid].mp3glob.gl_pathc)
    {
        channels[chnid].pos = 0;
    }
    //关闭当前媒体文件
    close(channels[chnid].fd);
    //打开下一个媒体文件
    channels[chnid].fd = open(channels[chnid].mp3glob.gl_pathv[channels[chnid].pos], O_RDONLY);
    if(channels[chnid].fd < 0)
    {
        syslog(LOG_ERR, "[medialib.c][open_next]open(): %s", strerror(errno));
        return -1;
    }
    //偏移量清零
    channels[chnid].offset = 0;
    return 0;
}


//读取频道中的媒体文件
ssize_t mlib_readchn(chnid_t chnid, void * buf, ssize_t size)
{
    int tbfsize;  //令牌桶中的令牌数
    int len;      //读取的字节数

    //如果频道文件打开失败, 则打开下一个媒体文件
    if(channels[chnid].fd < 0)
    {
        if(open_next(chnid))
        {
            syslog(LOG_ERR, "[medialib.c][mlib_readchn]open_next() failed");
            return -1;
        }
    }

    //获取令牌桶中的令牌数
    tbfsize = mytbf_fetchtoken(channels[chnid].tbf, size);
    //读取媒体文件
    while((len = pread(channels[chnid].fd, buf, tbfsize, channels[chnid].offset)) < 0)
    {
        if(errno != EINTR && errno != EAGAIN)
        {
            syslog(LOG_ERR, "[medialib.c][mlib_readchn]pread(): %s", strerror(errno));
            return -1;
        }
    }
    //如果读取的字节数为0, 则说明该媒体文件已经读取完毕, 则打开下一个媒体文件
    if(len == 0)
    {
        syslog(LOG_DEBUG, "[medialib.c][mlib_readchn] media file %s is read over.", channels[chnid].mp3glob.gl_pathv[channels[chnid].pos]);
        channels[chnid].offset = 0;
        if(open_next(chnid))
        {
            syslog(LOG_ERR, "[medialib.c][mlib_readchn]open_next() failed");
            return -1;
        }
    }
    //更新偏移量
    channels[chnid].offset += len;

    //如果令牌桶中的令牌数大于读取的字节数, 则将多余的令牌放回令牌桶中
    if(tbfsize > len)
    {
        mytbf_returntoken(channels[chnid].tbf, tbfsize - len);
    }
    return len;
}
