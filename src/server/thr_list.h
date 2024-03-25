#ifndef THR_LIST_H__
#define THR_LIST_H__
#include "medialib.h"
#include <stdio.h>


//创建频道列表线程
int thr_list_create(struct mlib_listentry_st *, ssize_t);

//销毁频道列表线程
void thr_list_destroy(void);

#endif
