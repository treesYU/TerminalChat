/**********************************************************************
 *  Descripttion: 线程池
 *  version: 
 *  Author: trees.yu
 *  Date: 2019-11-25 23:54:45
 *  LastEditors: trees.yu
 *  LastEditTime: 2019-11-26 17:35:55
 **********************************************************************/

#ifndef __PTHREAD_H_
#define __PTHREAD_H_

#include <pthread.h>

int *(*task_func)(void *); // 任务回调格式
struct thd_private; // 私有部分声明

/**********************************************************************
 *  name    : CThreadPool
 *  describe: 线程池类
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
typedef struct thd_classPools
{
    struct thd_private * pprivate;

    /**********************************************************************
     *  name    : poolAddTask
     *  describe: 将任务加入线程池
     *  input   : _this 线程池句柄 
     *            _func 任务回调函数 int *(*task_func)(void *);
     *            _arg 传输给回调的参数
     *  editlogs: 2019-11-26 17:07:45 yu create
     **********************************************************************/
    int (*poolAddTask)(struct thd_classPools *_this, int *(*_func)(void *arg), void *_arg);
} CThreadPool;

/**********************************************************************
 *  name    : newThreadPool
 *  describe: 创建CThreadPool实例
 *  input   : _minNum 最小线程数 _maxNum 最大线程数 _poolSize 线程池大小
 *  return  : CThreadPool *线程池操作句柄 NULL 失败
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
CThreadPool *newThreadPool(int _minNum, int _maxNum, int _poolSize);

/**********************************************************************
 *  name    : deleteThreadPool
 *  describe: 销毁CThreadPool实例
 *  input   : CThreadPool *线程池操作句柄
 *  return  : 0 成功 其他 失败
 *  editlogs: 2019-11-26 17:07:45 yu create
 **********************************************************************/
int deleteThreadPool(CThreadPool *_pool);

#endif // __PTHREAD_H_
