/*************************************************************************
  > File Name: thread_pool.c
  > Author: Danny
  > Mail: dannysdable@gmail.com 
  > Created Time: 2016-05-27 11:30 AM
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define DEBUG 1
#define ERR (-1)
#define DBG 0

#define DBG_PRINT(arg,fmt, ...)\
    do {\
        if (DEBUG) {\
            if (arg==ERR) {\
                fprintf(stderr, "ERR::%s:%d:%s() :"fmt, __FILE__,__LINE__,__func__,__VA_ARGS__);\
            }\
            else {\
                fprintf(stderr, "DBG::%s:%d:%s() :"fmt, __FILE__,__LINE__,__func__,__VA_ARGS__);\
            }\
        }\
    }while (0)

const int ST_OK = 0;
const int ST_NG = -1;
const int YES = 1;
const int NO = 0;


typedef struct _thread_info{
    int id;
    int idle;
    void *arg;
}pthread_info_t;

typedef struct _task_info {
    sem_t sem_dispath;
    pthread_t *thread_id;
    sem_t *sem_thread;
    pthread_info_t *pthread_detail;
    int max_thread_num;
    int shutdown;
    int reload;
}task_t;

task_t *g_task = NULL;

void *thread_worker(void *arg)
{
    int i = 0;
    int semvalue = 0; 

    DBG_PRINT(DBG, "Func: start ... \n", NULL);
    pthread_info_t *tmp = (pthread_info_t*)arg;

    if (tmp == NULL) return NULL;

    i = tmp->id;

    while (1)
    {
        DBG_PRINT(DBG, "Func: wait \n", NULL);
        /* 阻塞，等待分发线程通知 */
        sem_wait(&g_task->sem_thread[i]);

        if (g_task->shutdown) pthread_exit(NULL);

        semvalue = 0; 
        sem_getvalue(&g_task->sem_dispath, &semvalue); // 获得信号量当前的值

        /* 线程工作状态由调度线程置1，不应该由线程自己置位。
           防止工作线程置位的之前被抢占tmp->idle = 1;*/
        DBG_PRINT(DBG, "set idle == 0: %d    semvalue=%d\n", tmp->idle, semvalue);
        tmp->idle = 0;
        sleep(1);

        /* 唤醒分发线程 */
        sem_post(&g_task->sem_dispath);
        semvalue = 0; 
        sem_getvalue(&g_task->sem_dispath, &semvalue); // 获得信号量当前的值
        DBG_PRINT(DBG, "[%d] sem_dispath count: %d\n", i, semvalue);
    }

    pthread_exit(NULL);
}

int task_init(const int thread_num)
{
    int i = 0;

    if (thread_num <= 0) return ST_NG;

    g_task->max_thread_num = thread_num;
    g_task->shutdown = NO;
    g_task->reload = NO;

    g_task->thread_id = (pthread_t*)calloc(g_task->max_thread_num, sizeof(pthread_t));
    if (g_task->thread_id == NULL) {
        DBG_PRINT(ERR, "thread_id initialization failed. %s\n", strerror(errno));
        goto _end;
    }

    g_task->sem_thread = (sem_t*)calloc(g_task->max_thread_num, sizeof(sem_t));
    if (g_task->sem_thread == NULL) {
        DBG_PRINT(ERR, "sem initialization failed. %s\n", strerror(errno));
        goto _end;
    }

    /* init sem */
    for (i=0; i< g_task->max_thread_num; i++) {
        if (sem_init(&g_task->sem_thread[i], 0, 0) != 0) {
            DBG_PRINT(ERR, "Semaphore initialization failed. %s\n", strerror(errno));
            goto _end;
        }
    }

    g_task->pthread_detail = (pthread_info_t *)calloc(g_task->max_thread_num, sizeof(pthread_info_t));
    if (g_task->pthread_detail == NULL) {
        goto _end;
    }

    if (sem_init(&g_task->sem_dispath, 0, 0) != 0) {
        DBG_PRINT(ERR, "Sem dispath initialization failed. %s\n", strerror(errno));
        goto _end;
    }

    for (i=0; i< g_task->max_thread_num; i++) {
        g_task->pthread_detail[i].id = i;
        if (pthread_create(&g_task->thread_id[i], NULL, thread_worker, (void*)&g_task->pthread_detail[i]) != 0) {
            DBG_PRINT(ERR, "Thread creation failed. %s\n", strerror(errno));
            goto _end;
        }
    }

    return ST_OK;

_end:
    return ST_NG;
}

int task_destory()
{
    int i = 0;

    for (i=0; i< g_task->max_thread_num; i++) {
        pthread_join(g_task->thread_id[i], NULL);
    }

    for (i=0; i< g_task->max_thread_num; i++) {
        sem_destroy(&g_task->sem_thread[i]);
    }

    free(g_task->thread_id);
    free(g_task->sem_thread);
    free(g_task->pthread_detail);
    free(g_task);

    return ST_OK;
}

void *dispath_thread(void *arg)
{
    int i = 0;
    int semvalue = 0;

    while (1)
    {
        if (g_task->shutdown == YES) break;

        if (g_task->reload == YES) {
            /* 重载配置文件 */
        }

        /* 获取数据 */

        for (i=0; i< g_task->max_thread_num; i++) {
            if (g_task->pthread_detail[i].idle == 0) {
                /* 工作线程的参数 */
                g_task->pthread_detail[i].arg = NULL;
                /* 线程无锁，在调度的时候修改工作线程的状态 */
                g_task->pthread_detail[i].idle = 1;

                /* 通知工作线程 */
                sem_post(&g_task->sem_thread[i]);
                int semvalue = 0; 
                sem_getvalue(&g_task->sem_thread[i], &semvalue); // 获得信号量当前的值
                DBG_PRINT(DBG, "[%d] Semaphore count: %d\n", i, semvalue);
            }
        }

        DBG_PRINT(DBG, "+++++++++++++ NO idle thread ++++++++++\n", NULL);
        sem_wait(&g_task->sem_dispath); // 再次阻塞，等待输入
        semvalue = 0; 
        sem_getvalue(&g_task->sem_dispath, &semvalue); // 获得信号量当前的值
        DBG_PRINT(DBG, "[%d] Sem dispath count: %d\n", i, semvalue);
    }

    return NULL;
}

void reload_conf()
{
    g_task->reload = 1;
    DBG_PRINT(DBG, "reload conf\n", NULL);

    return;
}

int main()
{
    int i = 0;
    int semvalue = 0;
    pthread_t dispath_id;

    /* kill -SIGUSR1 pid */
    signal(SIGUSR1, reload_conf);

    g_task = (task_t*)calloc(1, sizeof(task_t));
    if (g_task == NULL) {
        DBG_PRINT(ERR, "malloc task failed\n", NULL);
        return NULL;
    }

    if (task_init(2) != ST_OK) {
        DBG_PRINT(ERR, "task init failed\n", NULL);
        goto _error;
    }
    DBG_PRINT(DBG, "\n init ok\n", NULL);

    sleep(1);

    /* 分发线程 */
    if (pthread_create(&g_task->thread_id[i], NULL, dispath_thread, (void*)g_task) != 0) {
        DBG_PRINT(ERR, "Thread creation failed. %s\n", strerror(errno));
        goto _error;
    }

    pthread_join(dispath_id, NULL);

    DBG_PRINT(DBG, "\nWaiting for thread to finish...\n", NULL);
    task_destory();

    exit(EXIT_SUCCESS);

_error:
        exit(EXIT_FAILURE);
}

