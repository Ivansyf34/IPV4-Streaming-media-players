#ifndef THR_CHANNEL_H__
#define THR_CHANNEL_H__
#include "medialib.h"


//创建频道线程
int thr_channel_create(struct mlib_listentry_st *);

//销毁频道线程
int thr_channel_destroy(struct mlib_listentry_st * );

//销毁所有频道线程
int thr_channel_destroyall(void);

#endif
