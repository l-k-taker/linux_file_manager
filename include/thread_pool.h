#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <pthread.h>
//任务结构体
struct Task
{
    /* data */
    void (*fuction)(void* arg);
    void *arg;
};

//线程池
struct Threadpool
{
    /* data */
    struct Task *taskqueue;
    int queueCapacity; //容量
    int queueSize;  //任务个数
    int queueFront;
    int queueRear;

    //管理线程
    pthread_t managerID;
    pthread_t *threadIDs;
    int minNUM;
    int maxNUM;
    int busyNUM;
    int liveNUM;
    int exitNUM; //杀死线程数
    pthread_mutex_t mutexPool;
    pthread_mutex_t mutexBusy;
    pthread_cond_t notFull;
    pthread_cond_t notEmpty;

    int shutdown;//是否销毁线程池
};

//线程池初始化
struct Threadpool *Thread_init(int min,int max,int queuesize);
//销毁
int threadPoolDestory(struct Threadpool* pool);
//给线程池添加任务
void threadPoolAdd(struct Threadpool* pool,void(*function) (void*),void* arg);
//获取工作的线程个数
int threadPoolBusyNum(struct Threadpool* pool);
//获取活着的线程个数
int threadPoolLiveNum(struct Threadpool* pool);

///////////
void* worker(void *arg);
void* manager(void * arg);
#endif

