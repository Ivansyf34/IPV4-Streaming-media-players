#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <syslog.h>
#include "mytbf.h"

#define min(a,b)      (a > b ? b : a)

//令牌桶结构体
struct mytbf_st
{
    int cps;               //每秒钟加的令牌数
    int burst;             //桶的上限
    int token;             //桶中令牌数
    int pos;               //当前令牌桶在数组中的下标
    pthread_mutex_t mut;   //互斥锁
    pthread_cond_t cond;   //条件变量
};

//令牌桶数组互斥锁
static pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER;
//单次加载模块锁
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
//令牌桶数组
static struct mytbf_st *tong[TBFMAXSIZE];
//线程ID
static pthread_t tid;

//令牌桶线程
static void *thr_alm(void *p)
{
    int i;
    while(1)
    {
        pthread_mutex_lock(&mut_job);
        //遍历令牌桶数组并添加令牌
        for(i = 0; i < TBFMAXSIZE; i ++)
        {
            if(tong[i] != NULL)
            {
                pthread_mutex_lock(&tong[i]->mut);
                tong[i]->token += tong[i]->cps;
                if(tong[i]->token > tong[i]->burst)
                {
                    tong[i]->token = tong[i]->burst;
                }
                //唤醒等待条件变量的线程
                pthread_cond_broadcast(&tong[i]->cond);
                pthread_mutex_unlock(&tong[i]->mut);
            }
        }
        pthread_mutex_unlock(&mut_job);
        sleep(1);
    }
    return NULL;
}


//模块卸载函数
static void module_unload()
{
    int i;
    //取消线程并等待线程结束
    pthread_cancel(tid);
    //遍历令牌桶数组并释放内存
    pthread_join(tid, NULL);
    //释放令牌桶数组中的内存
    for(i = 0; i < TBFMAXSIZE; i++)
    {
        free(tong[i]);
    }
    return ;
}

//模块加载函数
static void module_load()
{
    int err;
    //创建令牌桶线程
    err = pthread_create(&tid, NULL, thr_alm, NULL);
    if(err)
    {
        syslog(LOG_ERR, "pthread_create() :%s", strerror(err));
        fprintf(stderr, "pthread_create():%s\n", strerror(err));
    }
    //注册模块卸载函数
    atexit(module_unload);
}

//获取令牌桶数组中的空闲位置
int get_free_pos_unlocked()
{
    int i;
    for(i = 0; i < TBFMAXSIZE; i ++)
    {
        if(tong[i] == NULL)
        {
            return i;
        }
    }
    return -1;
}


//初始化令牌桶
mytbf_t *mytbf_init(int cps, int burst)
{
    struct mytbf_st *me;
    int pos;

    //单次加载模块
    pthread_once(&init_once, module_load);
    me = malloc(sizeof(*me));
    if(me == NULL)
    {
        return NULL;
    }

    //初始化令牌桶
    me->cps = cps;
    me->burst = burst;
    me->token = 0;
    //初始化互斥锁和条件变量
    pthread_mutex_init(&me->mut, NULL);
    pthread_cond_init(&me->cond, NULL);
    pthread_mutex_lock(&mut_job);
    //获取令牌桶数组中的空闲位置
    pos = get_free_pos_unlocked();
    if(pos < 0)
    {
        pthread_mutex_unlock(&mut_job);
        free(me);
        return NULL;
    }
    me->pos = pos;
    tong[pos] = me;
    pthread_mutex_unlock(&mut_job);
    return me;
}

//获取令牌
int mytbf_fetchtoken(mytbf_t *ptr, int token)
{
    struct mytbf_st *me = ptr;
    int n;
    pthread_mutex_lock(&me->mut);
    //等待令牌桶中有令牌
    while(me->token <= 0)
    {
        pthread_cond_wait(&me->cond, &me->mut);
    }
    //获取令牌(获取令牌数和请求令牌数的最小值)
    n = min(me->token, token);
    me->token -= n;
    pthread_mutex_unlock(&me->mut);
    return n;
}

//归还令牌
int mytbf_returntoken(mytbf_t *ptr, int token)
{
    struct mytbf_st *me = ptr;
    pthread_mutex_lock(&me->mut);
    me->token = min(me->token + token, me->burst);
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->mut);
    return 0;
}


//销毁令牌桶
int mytbf_destroy(mytbf_t *ptr)
{
    struct mytbf_st *me = ptr;
    pthread_mutex_lock(&me->mut);
    tong[me->pos] = NULL;
    pthread_mutex_unlock(&me->mut);
    pthread_mutex_destroy(&me->mut);
    pthread_cond_destroy(&me->cond);
    return 0;
}



