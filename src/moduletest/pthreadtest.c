/**********************************************************************
 *  Descripttion: pthread test
 *  version: 
 *  Author: trees.yu
 *  Date: 2019-11-26 00:57:07
 *  LastEditors: trees.yu
 *  LastEditTime: 2019-11-26 17:04:32
 **********************************************************************/

#include "threadspool.h"
#include <stdio.h>
#include <unistd.h>

int *task_test(void *_parg)
{
    printf("i am %ld\n", pthread_self());
    sleep(5);
    printf("%ld gone\n", pthread_self());
    return NULL;
}

int test_threadpool(void)
{
    CThreadPool *ptest = newThreadPool(10, 50, 100);
    for (int i = 0; i < 100; i++)
    {
        ptest->poolAddTask(ptest, task_test, NULL);
        sleep(1);
    }
    deleteThreadPool(ptest);
    return 0;
}

int main(int argc, char const *argv[])
{
    test_threadpool();
    return 0;
}
