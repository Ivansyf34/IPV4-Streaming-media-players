#ifndef MYTBF_H__
#define MYTBF_H__

#define TBFMAXSIZE 1024   //令牌桶最大容量
typedef void mytbf_t;     //令牌桶类型

//初始化令牌桶
mytbf_t *mytbf_init(int cps, int burst);

//获取令牌
int mytbf_fetchtoken(mytbf_t *, int token);

//归还令牌
int mytbf_returntoken(mytbf_t *, int token);

//销毁令牌桶
int mytbf_destroy(mytbf_t *);


#endif
