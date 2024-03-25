# IPV4_流媒体广播

修改src/server/server_conf.h 中的DEFAULT_MEDIADIR为自己的媒体库路径
目录格式如下：
- ![image](https://github.com/Ivansyf34/IPV4-Streaming-media-players/assets/93926232/acdbf518-cbff-4c97-9033-fb1381bfc045)

## 客户端

#### 处理流程

- 解析命令行参数

- 创建报式`socket(udp)`、加入多播组、设置回环、绑定本机地址和端口

  ~~~ cpp
  //创建报式socket(udp), AF_INET 指定了 IPv4 地址族，SOCK_DGRAM 表示该套接字是一个数据报套接字，用于支持无连接的 UDP 通信
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  
  //加入多播组
  res = inet_pton(AF_INET, client_conf.mgroup, &ipmreq.imr_multiaddr); //多播地址
  //inet_pton函数是用于将点分十进制表示的IPv4地址转换为网络字节序的二进制表示形式
  
  res = inet_pton(AF_INET, "0.0.0.0", &ipmreq.imr_address); //本地地址
  
  ipmreq.imr_ifindex = if_nametoindex(IF_INDEX); //网卡索引
  res = setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &ipmreq, sizeof(ipmreq));
  //IP_ADD_MEMBERSHIP：该选项用于加入一个 IPv4 多播组。通过设置这个选项，套接字将加入到指定的多播组，以接收该多播组发送的数据
  
  //设置回环
  val = 1;
  res = setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val));
  //IP_MULTICAST_LOOP：这个选项用于控制是否将本地发送的多播数据包重新发送给本地接收者。当这个选项被设置为 1 时，本地发送的多播数据包也会被重新发送到本地接收器，即开启了回环。
  
  //绑定本机地址和端口
  laddr.sin_port = htons(atoi(client_conf.rcvport)); //端口
  laddr.sin_family = AF_INET; //地址族
  res = inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr); //地址
  res = bind(sd, (void *) &laddr, sizeof(laddr));
  //套接字可以接收到发送到多播组的数据，并且绑定了本地地址和端口，以便应用程序可以在该地址和端口上接收数据
  ~~~

- 创建管道

- 创建子进程

- 子进程：调用解码器

  ~~~c++
  close(sd);           //子进程不需要socket
  close(pd[1]);        //子进程不需要写管道
  dup2(pd[0], 0);      //将管道的读端重定向到标准输入
  if(pd[0] > 0)        //关闭原来的读端
      close(pd[0]);
  execl("/bin/sh", "sh", "-c", client_conf.playercmd, NULL); //调用解码器
  perror("[client.c][main]execl"); //execl调用失败(如果调用成功，不会执行到这里)
  ~~~

- 父进程：从网络上收包，发送给子进程

  - 关闭写端

  - 接受节目单列表

  - 打印选择频道、选择频道

  - 接收频道包，发送给子进程

    ~~~ C++
    //关闭写端
    close(pd[0]);
    
    //接受节目单列表
    len = recvfrom(sd, msg_list, MSG_LIST_MAX, 0, (void *)&serveraddr, &serverlen);
    
    //接受频道包
    len = recvfrom(sd, msg_channel, MSG_CHANNEL_MAX, 0, (void *)&raddr, &raddrlen);
    
    //数据通过管道发给子进程
    res = writen(pd[1], (char *)msg_channel->data, len - sizeof(chnid_t));
    ~~~



## 服务端

#### 处理流程

- 解析命令行参数

- 守护进程实现

- socket初始化

  ~~~c++
  // 创建报式套接字(UDP)
  serversd = socket(AF_INET, SOCK_DGRAM, 0);
  
  // 创建多播组
  mreq.imr_ifindex = if_nametoindex(server_conf.ifname);  // 设置网卡索引
  res = inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address); // 设置ip地址
  
  res = inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr); // 设置多播地址
  
  setsockopt(serversd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq))
  
  // 设置发送地址
  sndaddr.sin_family = AF_INET;                                     // 地址族
  sndaddr.sin_port = htons(atoi(server_conf.rcvport));              // 端口
  inet_pton(AF_INET, server_conf.mgroup, &sndaddr.sin_addr.s_addr); // ip地址(多播地址)
  ~~~

- 获取频道信息

- 创建节目单线程

- 创建频道线程

  

#### 守护进程

~~~c++
// 创建子进程
pid = fork();
// 父进程退出
if (pid > 0)
    exit(0);
// 将输出重定向到/dev/null
fd = open("/dev/null", O_RDWR);
dup2(fd, 0);
dup2(fd, 1);
dup2(fd, 2);

// 创建新会话,脱离终端
setsid();
// 改变工作目录
chdir("/");
// 重设文件掩码
umask(0);
~~~



#### 令牌桶

~~~c++
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
~~~

- `pthread_once(&init_once, module_load)`

  - 单次加载模块初始化，只会在第一次被调用
  - `atexit(module_unload)`最后注册卸载函数，取消线程，释放内存
  - `pthread_create(&tid, NULL, thr_alm, NULL)`创建令牌线程，`thr_alm`定时每秒向每个桶加入`cps`个令牌

- 初始化令牌桶` mytbf_t *mytbf_init(int cps, int burst)`

  - 初始化一个令牌桶，包括分配内存、设置参数、初始化互斥锁和条件变量，并将其加入令牌桶数组中

- 获取令牌` int mytbf_fetchtoken(mytbf_t *, int token)`

  - 令牌桶中获取指定数量的令牌，如果令牌不够，则等待条件变量唤醒

- 归还令牌` int mytbf_returntoken(mytbf_t *, int token)`

  - 归还指定数量的令牌到令牌桶中，并唤醒等待的线程

- 销毁令牌桶`int mytbf_destroy(mytbf_t *)`

  - 销毁令牌桶，包括从令牌桶数组中移除、销毁互斥锁和条件变量，并释放内存。

  

#### 媒体库

~~~c++
//媒体库包
struct mlib_listentry_st
{
    chnid_t chnid;   //通道ID
    char *desc;      //描述
};

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
~~~

* `static struct channel_context_st* path2entry(char *path)`

  * 根据传入的路径**获取频道信息**，包括频道号、描述信息、媒体文件路径等
  * 它**打开描述文件**，读取第一行描述信息，并分配内存保存
  * **初始化令牌桶**，并为描述信息和令牌桶分配内存
  * 使用 `glob` 函数获取路径下所有的媒体文件路径，并**初始化频道信息**
  * 返回频道信息的指针

* 获取媒体库列表`int mlib_getchnlist(struct mlib_listentry_st **result, int *size)`

  * 这个函数获取频道列表，遍历指定目录下的文件
  * 调用 `path2entry` 函数获取频道信息
  * 并保存到频道数组中

* 释放媒体库列表`mlib_freechnlist(struct mlib_listentry_st *)`

  * 释放频道列表的内存

* 根据通道ID获取媒体库信息`mlib_readchn(chnid_t , void *, ssize_t)`

  * 它从令牌桶中**获取令牌数**，并使用 `pread` 函数读取媒体文件

  * 如果读取完当前媒体文件，则调用 `open_next` 函数打开下一个媒体文件

  * 最后，它更新偏移量，并根据令牌桶中的情况，将多余的令牌返回给令牌桶

    

#### 频道列表

- `void thr_list_send(void *p)`：

  - 它首先计算频道列表数据的总长度，然后分配相应大小的内存空间。

  - 接着，设置频道号，并依次存储每个频道的信息，包括频道号、描述长度和描述内容。

  - 最后，**循环发送**频道列表数据到指定的目的地，每隔一秒发送一次。
  - `sendto(serversd, entlistp, totalsize, 0, (void *)&sndaddr, sizeof(sndaddr))`
    - `serversd`：表示要**发送数据的套接字描述符**。
    - `entlistp`：指向要发送的数据的指针，即频道列表数据。
    - `totalsize`：要发送的数据的总长度。
    - `0`：标志参数，通常设置为0。
    - `(void *)&sndaddr`：指向**目的地套接字地址的指针**，通常需要进行类型转换为 `void *` 类型。
    - `sizeof(sndaddr)`：目的地套接字地址结构体的大小。

- `thr_list_create(struct mlib_listentry_st *listptr, ssize_t size)`：

  - 它接收频道列表指针和列表长度作为参数，并将它们存储在全局变量中。

  - 然后，创建一个新线程`pthread_create(&tid, NULL, thr_list_send, NULL);`
  - 调用 **`thr_list_send` **函数作为线程入口函数，发送频道列表。

- `thr_list_destroy(void)`：

  - 它取消线程，并等待线程结束，以回收资源。

  

#### 频道数据

- `thr_channel_snder(void *p)`：

  - 它首先**分配内存**用于存储频道数据。

  - 然后，设置频道ID，并进入一个无限循环，在循环中调用`mlib_readchn`不断读取频道数据，**发送数据到指定的套接字地址**。

  - 在发送数据后，使用 `sched_yield()` 函数让出CPU，使得其他线程有机会执行。

    ~~~c++
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
    ~~~

  - 最后，线程在结束时调用 `pthread_exit()` 函数退出。

- `thr_channel_create(struct mlib_listentry_st *ptr)`：

  - 它接收一个指向频道信息的指针作为参数，创建一个新线程，并调用 `thr_channel_snder` 函数作为线程入口函数，**发送频道数据**。
  - 创建线程成功后，将频道ID和线程ID保存到 `thr_channel` 数组中，并自增线程ID数组下标。

- `thr_channel_destroy(struct mlib_listentry_st * ptr)`：

  - 销毁频道线程
  - 它接收一个指向频道信息的指针作为参数，遍历线程数组，找到与指定**频道ID匹配**的线程，并取消该线程，等待线程结束并回收资源。

- `thr_channel_destroyall(void)`：

  - 销毁所有频道线程
  - 它遍历线程数组，取消所有未销毁的线程，等待线程结束并回收资源。
