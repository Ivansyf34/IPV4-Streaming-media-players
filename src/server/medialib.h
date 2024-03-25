#ifndef MEDIALIB_H__
#define MEDIALIB_H__
#include "../include/proto.h"
#include "../include/site_type.h"
#include <stdio.h>

//媒体库包
struct mlib_listentry_st
{
    chnid_t chnid;   //通道ID
    char *desc;      //描述
};

//获取媒体库列表
int mlib_getchnlist(struct mlib_listentry_st **, int *);


//释放媒体库列表
int mlib_freechnlist(struct mlib_listentry_st *);


//根据通道ID获取媒体库信息
ssize_t mlib_readchn(chnid_t , void *, ssize_t);


#endif
