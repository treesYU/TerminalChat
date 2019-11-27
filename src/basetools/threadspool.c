/**********************************************************************
 *  Descripttion: 
 *  version: 
 *  Author: trees.yu
 *  Date: 2019-11-25 22:57:27
 *  LastEditors: trees.yu
 *  LastEditTime: 2019-11-26 17:59:51
 **********************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <malloc.h>
#include "threadspool.h"

#define DEFAULT_TIME 10         // 管理线程扫描时间 单位秒
#define MIN_TASK_WAIT 50        // 任务等待数
#define DEFAULT_THREAD_NUM 10   // 单次新增/减少线程数

typedef struct
{
    int *(*m_func)(void *);
    void *m_arg;
} thd_TTask;

struct thd_private
{
    pthread_mutex_t m_lock;
    pthread_mutex_t m_threadCounter;
    pthread_cond_t m_queueNotFull;  // 任务队列非满信号量
    pthread_cond_t m_queueNotEmpty; // 任务队列非空信号量

    pthread_t *m_pthreads;   // 子线程队列
    pthread_t m_manageTid;   // 管理线程句柄
    thd_TTask *m_ptaskQueue; // 任务队列

    /* info */
    int m_thrMin;      // 线程数维持最小值
    int m_thrMax;      // 线程数维持最大值
    int m_thrLive;     // 当前存活的线程数
    int m_thrBusy;     // 当前忙的线程数
    int m_thrWaitExit; // 当前等待退出线程数

    /* status*/
    int m_queueFront;
    int m_queueRear;
    int m_queueSize;
    int m_queueMaxSize;

    int m_shutdown;
};

/**********************************************************************
 *  name    : _poolThread
 *  describe: 任务线程基本处理
 *  input   : _threadpool 线程池句柄 
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
void *_poolThread(void *_threadpool)
{
    CThreadPool *pool = (CThreadPool *)_threadpool;
    thd_TTask task;
    pthread_detach(pthread_self());
    while (1)
    {
        pthread_mutex_lock(&(pool->pprivate->m_lock));

        while ((pool->pprivate->m_queueSize == 0) && (pool->pprivate->m_shutdown != TRUE))
        {
            // 等待任务
            pthread_cond_wait(&(pool->pprivate->m_queueNotEmpty), &(pool->pprivate->m_lock));
            if (pool->pprivate->m_thrWaitExit > 0)
            {
                pool->pprivate->m_thrWaitExit--;
                if (pool->pprivate->m_thrLive > pool->pprivate->m_thrMin)
                {
                    // 存活数大于最小维持数时，线程销毁
                    pool->pprivate->m_thrLive--;
                    pthread_mutex_unlock(&(pool->pprivate->m_lock));
                    pthread_exit(NULL);
                }
            }
        }

        if (pool->pprivate->m_shutdown == TRUE)
        {
            // 线程池关闭时退出
            pthread_mutex_unlock(&(pool->pprivate->m_lock));
            pthread_exit(NULL);
        }

        // 取出任务队列中的任务
        task.m_func = pool->pprivate->m_ptaskQueue[pool->pprivate->m_queueFront].m_func;
        task.m_arg = pool->pprivate->m_ptaskQueue[pool->pprivate->m_queueFront].m_arg;
        pool->pprivate->m_queueFront = (pool->pprivate->m_queueFront + 1) % pool->pprivate->m_queueMaxSize;
        pool->pprivate->m_queueSize--;

        // 发送非满信号 允许新增任务
        pthread_cond_broadcast(&(pool->pprivate->m_queueNotFull));
        pthread_mutex_unlock(&(pool->pprivate->m_lock));

        printf("-- thread 0x%x start work!\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&(pool->pprivate->m_threadCounter));
        pool->pprivate->m_thrBusy++;
        pthread_mutex_unlock(&(pool->pprivate->m_threadCounter));

        (*(task.m_func))(task.m_arg); // 任务处理

        printf("-- thread 0x%x end work!\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&(pool->pprivate->m_threadCounter));
        pool->pprivate->m_thrBusy--;
        pthread_mutex_unlock(&(pool->pprivate->m_threadCounter));
    }
    pthread_exit(NULL);
    return NULL;
}

/**********************************************************************
 *  name    : _poolAddTask
 *  describe: 将任务加入线程池
 *  input   : _this 线程池句柄 
 *            _func 任务回调函数 int *(*task_func)(void *);
 *            _arg 传输给回调的参数
 *  return  : 0 成功 其他 失败
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
int _poolAddTask(CThreadPool *_pool, int *(*_func)(void *arg), void *_arg)
{
    pthread_mutex_lock(&(_pool->pprivate->m_lock));

    // 任务队列满时阻塞等待
    while ((_pool->pprivate->m_queueSize == _pool->pprivate->m_queueMaxSize) && (_pool->pprivate->m_shutdown != TRUE))
        pthread_cond_wait(&(_pool->pprivate->m_queueNotFull), &(_pool->pprivate->m_lock));

    if (_pool->pprivate->m_shutdown == TRUE)
    {
        // 线程池关闭
        pthread_mutex_unlock(&(_pool->pprivate->m_lock));
        return -1;
    }

    if (_pool->pprivate->m_ptaskQueue[_pool->pprivate->m_queueRear].m_arg != NULL)
    {
        free(_pool->pprivate->m_ptaskQueue[_pool->pprivate->m_queueRear].m_arg);
        _pool->pprivate->m_ptaskQueue[_pool->pprivate->m_queueRear].m_arg = NULL;
    }
    _pool->pprivate->m_ptaskQueue[_pool->pprivate->m_queueRear].m_func = _func;
    _pool->pprivate->m_ptaskQueue[_pool->pprivate->m_queueRear].m_arg = _arg;
    _pool->pprivate->m_queueRear = (_pool->pprivate->m_queueRear + 1) % _pool->pprivate->m_queueMaxSize;
    _pool->pprivate->m_queueSize++;

    // 发送非空信号 唤醒沉睡线程
    pthread_cond_signal(&(_pool->pprivate->m_queueNotEmpty));
    pthread_mutex_unlock(&(_pool->pprivate->m_lock));
    return 0;
}

/**********************************************************************
 *  name    : _isThreadAlive
 *  describe: 线程存活判断
 *  input   : 线程句柄
 *  return  : TRUE 存活 FALSE 无效
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
int _isThreadAlive(pthread_t _tid)
{
    int kill_rc = pthread_kill(_tid, 0);
    if (kill_rc == ESRCH)
    {
        return FALSE;
    }
    return TRUE;
}

/**********************************************************************
 *  name    : _manageThread
 *  describe: 线程池管理线程 
 *  input   : _threadpool 线程池句柄
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
void *_manageThread(void *_threadpool)
{
    int i;
    CThreadPool *pool = (CThreadPool *)_threadpool;
    while (pool->pprivate->m_shutdown != TRUE)
    {
        sleep(DEFAULT_TIME);

        pthread_mutex_lock(&(pool->pprivate->m_lock));
        int queueSize = pool->pprivate->m_queueSize;
        int thrLive = pool->pprivate->m_thrLive;
        pthread_mutex_unlock(&(pool->pprivate->m_lock));

        pthread_mutex_lock(&(pool->pprivate->m_threadCounter));
        int thrBusy = pool->pprivate->m_thrBusy;
        pthread_mutex_unlock(&(pool->pprivate->m_threadCounter));
        // printf("busy -%d/%d-\n",thrBusy,thrLive);
        if (queueSize >= MIN_TASK_WAIT && thrLive <= pool->pprivate->m_thrMax)
        {
            // 线程数不满足任务要求 新增线程
            pthread_mutex_lock(&(pool->pprivate->m_lock));
            int add = 0;
            for (i = 0; i < pool->pprivate->m_thrMax && add < DEFAULT_THREAD_NUM && pool->pprivate->m_thrLive < pool->pprivate->m_thrMax; i++)
            {
                if (pool->pprivate->m_pthreads[i] == 0 || (_isThreadAlive(pool->pprivate->m_pthreads[i]) == FALSE))
                {
                    pthread_create(&(pool->pprivate->m_pthreads[i]), NULL, _poolThread, (void *)pool);
                    add++;
                    pool->pprivate->m_thrLive++;
                }
            }
            pthread_mutex_unlock(&(pool->pprivate->m_lock));
        }

        if ((thrBusy * 2) < thrLive && thrLive > pool->pprivate->m_thrMin)
        {
            // 线程数大于任务要求 删除部分空余线程
            pthread_mutex_lock(&(pool->pprivate->m_lock));
            pool->pprivate->m_thrWaitExit = DEFAULT_THREAD_NUM;
            pthread_mutex_unlock(&(pool->pprivate->m_lock));

            for (i = 0; i < DEFAULT_THREAD_NUM; i++)
            {
                pthread_cond_signal(&(pool->pprivate->m_queueNotEmpty));
            }
        }
    }
    return NULL;
}

/**********************************************************************
 *  name    : _freeThreadPool
 *  describe: 释放CThreadPool资源
 *  input   : CThreadPool *线程池操作句柄
 *  return  : 0 成功 其他 失败
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
int _freeThreadPool(CThreadPool *_pool)
{
    int result = 0;
    do
    {
        if (_pool == NULL)
        {
            result = -1;
            break;
        }

        if (_pool->pprivate != NULL)
        {
            if (_pool->pprivate->m_ptaskQueue != NULL)
            {
                free(_pool->pprivate->m_ptaskQueue);
            }

            if (_pool->pprivate->m_pthreads != NULL)
            {
                free(_pool->pprivate->m_pthreads);
                pthread_mutex_lock(&(_pool->pprivate->m_lock));
                pthread_mutex_destroy(&(_pool->pprivate->m_lock));
                pthread_mutex_lock(&(_pool->pprivate->m_threadCounter));
                pthread_mutex_destroy(&(_pool->pprivate->m_threadCounter));
                pthread_cond_destroy(&(_pool->pprivate->m_queueNotFull));
                pthread_cond_destroy(&(_pool->pprivate->m_queueNotEmpty));
            }

            free(_pool->pprivate);
        }

        free(_pool);
        _pool = NULL;
    } while (0);
    return result;
}

/**********************************************************************
 *  name    : deleteThreadPool
 *  describe: 销毁CThreadPool实例
 *  input   : CThreadPool *线程池操作句柄
 *  return  : 0 成功 其他 失败
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
int deleteThreadPool(CThreadPool *_pool)
{
    int result = 0;
    int i;
    do
    {
        if (_pool == NULL)
        {
            result = -1;
            break;
        }

        _pool->pprivate->m_shutdown = TRUE;

        pthread_join(_pool->pprivate->m_manageTid, NULL);

        for (i = 0; i < _pool->pprivate->m_thrLive; i++)
        {
            pthread_cond_broadcast(&(_pool->pprivate->m_queueNotEmpty));
        }
        for (i = 0; i < _pool->pprivate->m_thrLive; i++)
        {
            pthread_join(_pool->pprivate->m_pthreads[i], NULL);
        }
        _freeThreadPool(_pool);
    } while (0);
    return result;
}

/**********************************************************************
 *  name    : newThreadPool
 *  describe: 创建CThreadPool实例
 *  input   : _minNum 最小线程数 _maxNum 最大线程数 _poolSize 线程池大小
 *  return  : CThreadPool *线程池操作句柄 NULL 失败
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
CThreadPool *newThreadPool(int _minNum, int _maxNum, int _poolSize)
{
    int i;
    CThreadPool *pool = NULL;
    do
    {
        pool = (CThreadPool *)malloc(sizeof(CThreadPool));
        if (pool == NULL)
        {
            printf("malloc pool error!\n");
            break;
        }
        pool->pprivate = (struct thd_private *)malloc(sizeof(struct thd_private));
        if (pool == NULL)
        {
            printf("malloc private error!\n");
            break;
        }
        pool->pprivate->m_thrMin = _minNum;
        pool->pprivate->m_thrMax = _maxNum;
        pool->pprivate->m_thrBusy = 0;
        pool->pprivate->m_thrLive = _minNum;
        pool->pprivate->m_thrWaitExit = 0;
        pool->pprivate->m_queueFront = 0;
        pool->pprivate->m_queueRear = 0;
        pool->pprivate->m_queueSize = 0;
        pool->pprivate->m_queueMaxSize = _poolSize;
        pool->pprivate->m_shutdown = FALSE;

        pool->pprivate->m_pthreads = (pthread_t *)malloc(sizeof(pthread_t) * _maxNum);
        if (pool->pprivate->m_pthreads == NULL)
        {
            printf("malloc threads error!\n");
            break;
        }
        memset(pool->pprivate->m_pthreads, 0x00, sizeof(pthread_t) * _maxNum);

        pool->pprivate->m_ptaskQueue = (thd_TTask *)malloc(sizeof(thd_TTask) * _poolSize);
        if (pool->pprivate->m_ptaskQueue == NULL)
        {
            printf("malloc taskQueue error!\n");
            break;
        }

        if ((pthread_mutex_init(&(pool->pprivate->m_lock), NULL) != 0) ||
            (pthread_mutex_init(&(pool->pprivate->m_threadCounter), NULL) != 0) ||
            (pthread_cond_init(&(pool->pprivate->m_queueNotFull), NULL) != 0) ||
            (pthread_cond_init(&(pool->pprivate->m_queueNotFull), NULL) != 0))
        {
            printf("init lock or cond error!\n");
            break;
        }

        for (i = 0; i < pool->pprivate->m_thrMin; i++)
        {
            pthread_create(&(pool->pprivate->m_pthreads[i]), NULL, _poolThread, (void *)pool);
            printf("start thread 0x%x\n", (unsigned int)pool->pprivate->m_pthreads[i]);
        }
        pthread_create(&(pool->pprivate->m_pthreads[i]), NULL, _manageThread, (void *)pool);

        pool->poolAddTask = _poolAddTask;
        return pool;
    } while (0);
    _freeThreadPool(pool);
    return NULL;
}
