#include "thread_pool.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

const int NUMBER=2;

void threadexit(struct Threadpool* pool){
    pthread_t tid=pthread_self();
    for(int i=0;i<pool->maxNUM;i++){
        if(pool->threadIDs[i]==tid){
            pool->threadIDs[i]=0;
            printf("threadexit() called,%ld exiting...\n",tid);
            break;
        }
    }
    pthread_exit(NULL);
}

void* manager(void * arg){
    struct Threadpool* pool=(struct Threadpool*)arg;
    while(!pool->shutdown){
        //设置检测频率
        sleep(3);
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize=pool->queueSize;
        int liveNum=pool->liveNUM;
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum=pool->busyNUM;
        pthread_mutex_unlock(&pool->mutexBusy);

        //添加线程
        if(queueSize>liveNum-busyNum&&liveNum<pool->maxNUM){
            pthread_mutex_lock(&pool->mutexPool);
            int count=0;
            for(int i=0;i<pool->maxNUM&&count<NUMBER&&liveNum<pool->maxNUM;i++){
                if(pool->threadIDs[i]==0){
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    count++;
                    pool->liveNUM++;
                }

            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        //销毁线程
        if(busyNum*2<liveNum&&liveNum>pool->minNUM){
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNUM=NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            //工作线程自杀
            for(int i=0;i<NUMBER;i++){
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return NULL;
}

void* worker(void *arg){
    struct Threadpool * pool=(struct Threadpool*)arg;
    while(1){
        pthread_mutex_lock(&pool->mutexPool);
        while(pool->queueSize==0&&!pool->shutdown){
            pthread_cond_wait(&pool->notEmpty,&pool->mutexPool);
            //判断是否需要自杀
            if(pool->exitNUM>0){
                pool->exitNUM--;
                if(pool->liveNUM>pool->minNUM){
                    pool->liveNUM--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadexit(pool);
                }
            }
        }
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutexPool);
            threadexit(pool);
        }
        struct Task task;
        task.fuction=pool->taskqueue[pool->queueFront].fuction;
        task.arg=pool->taskqueue[pool->queueFront].arg;
        pool->queueFront=(pool->queueFront+1)%pool->queueCapacity;
        pool->queueSize--;
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);

        printf("thread %ld start working...\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNUM++;
        pthread_mutex_unlock(&pool->mutexBusy);

        task.fuction(task.arg);
        free(task.arg);
        task.arg=NULL;
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNUM--;
        pthread_mutex_unlock(&pool->mutexBusy);
        printf("thread %ld end working...\n",pthread_self());

    }
    return NULL;
}

void threadPoolAdd(struct Threadpool* pool,void(*function) (void*),void* arg){
    pthread_mutex_lock(&pool->mutexPool);
    while(pool->queueSize==pool->queueCapacity&&!pool->shutdown){
        //阻塞生产者
        pthread_cond_wait(&pool->notFull,&pool->mutexPool);
    }
    if(pool->shutdown){
        pthread_mutex_unlock(&pool->mutexPool);
        return ;
    }
    //添加任务
    pool->taskqueue[pool->queueRear].fuction=function;
    pool->taskqueue[pool->queueRear].arg=arg;
    pool->queueRear=(pool->queueRear+1)%pool->queueCapacity;
    pool->queueSize++;
    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

struct Threadpool *Thread_init(int min,int max,int queuesize){
    struct Threadpool* pool=(struct Threadpool*)malloc(sizeof(struct Threadpool));
    do{
        if(pool==NULL){
        printf("malloc threadpool failed...\n");
        break;
    }
    pool->threadIDs=(pthread_t*)malloc(sizeof(pthread_t)*max);
    if(pool->threadIDs==NULL){
        printf("malloc threadIDs faild...\n");
        break;
    }
    memset(pool->threadIDs,0,sizeof(pthread_t)*max);
    pool->maxNUM=max;
    pool->minNUM=min;
    pool->busyNUM=0;
    pool->liveNUM=min;
    pool->exitNUM=0;
    
    if(pthread_mutex_init(&pool->mutexPool,NULL)!=0||
       pthread_mutex_init(&pool->mutexBusy,NULL)!=0||
       pthread_cond_init(&pool->notEmpty,NULL)!=0||
       pthread_cond_init(&pool->notFull,NULL)!=0 ){
        printf("mutex or cond failed...\n");
        break;
       }
    
    pool->taskqueue=(struct Task*)malloc(sizeof(struct Task)* queuesize);
    pool->queueCapacity=queuesize;
    pool->queueSize=0;
    pool->queueFront=0;
    pool->queueRear=0;
    pool->shutdown=0;

    pthread_create(&pool->managerID,NULL,manager,pool);
    for(int i=0;i<min;i++){
        pthread_create(&pool->threadIDs[i],NULL,worker,pool);
    }
    return pool;
    }while(0);

    if(pool&&pool->threadIDs) free(pool->threadIDs);
    if(pool&&pool->taskqueue) free(pool->taskqueue);
    if(pool) free(pool);

    return NULL;
}

int threadPoolBusyNum(struct Threadpool* pool){
    pthread_mutex_lock(&pool->mutexPool);
    int busyNum=pool->busyNUM;
    pthread_mutex_unlock(&pool->mutexPool);
    return busyNum;
}

int threadPoolLiveNum(struct Threadpool* pool){
    pthread_mutex_lock(&pool->mutexPool);
    int liveNum=pool->liveNUM;
    pthread_mutex_unlock(&pool->mutexPool);
    return liveNum;
}

int threadPoolDestory(struct Threadpool* pool){
    if(pool==NULL){
        return -1;
    }
    pool->shutdown=1;

    /* 广播唤醒所有阻塞在条件变量上的工作线程 */
    pthread_mutex_lock(&pool->mutexPool);
    pthread_cond_broadcast(&pool->notEmpty);
    pthread_cond_broadcast(&pool->notFull);
    pthread_mutex_unlock(&pool->mutexPool);

    /* 回收管理者线程 */
    pthread_join(pool->managerID, NULL);

    /* 回收所有工作线程 (包括已退出和仍在运行的) */
    for (int i = 0; i < pool->maxNUM; i++) {
        if (pool->threadIDs[i] != 0) {
            pthread_join(pool->threadIDs[i], NULL);
            pool->threadIDs[i] = 0;
        }
    }

    /* 释放堆内存 */
    if (pool->taskqueue) {
        free(pool->taskqueue);
    }
    if (pool->threadIDs) {
        free(pool->threadIDs);
    }
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;

    return 0;
}