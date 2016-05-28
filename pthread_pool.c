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
}task_t;

task_t *task = NULL;

void *threadFunc(void *arg)
{
    int i = 0;
    int semValue = 0; 

    DBG_PRINT(DBG, "Func: start ... \n", NULL);
    pthread_info_t *tmp = (pthread_info_t*)arg;

    if (tmp == NULL) return NULL;

    i = tmp->id;

    while (1)
    {
        DBG_PRINT(DBG, "Func: wait \n", NULL);
        sem_wait(&task->sem_thread[i]); // 阻塞，等待输入
        semValue = 0; 
        sem_getvalue(&task->sem_dispath, &semValue); // 获得信号量当前的值

        tmp->idle = 1;
        DBG_PRINT(DBG, "set idle == 0: %d    semvalue=%d\n", tmp->idle, semValue);
        tmp->idle = 0;
        usleep(100);

        sem_post(&task->sem_dispath); // 输入完成，增加信号量的值
        semValue = 0; 
        sem_getvalue(&task->sem_dispath, &semValue); // 获得信号量当前的值
        DBG_PRINT(DBG, "[%d] sem_dispath count: %d\n", i, semValue);
    }
    pthread_exit(NULL);
}

int task_init(const int thread_num)
{
    int i = 0;

    if (thread_num <= 0) return ST_NG;

    task->max_thread_num = thread_num;

    task->thread_id = (pthread_t*)calloc(task->max_thread_num, sizeof(pthread_t));
    if (task->thread_id == NULL) {
        DBG_PRINT(ERR, "thread_id initialization failed. %s\n", strerror(errno));
        goto _end;
    }

    task->sem_thread = (sem_t*)calloc(task->max_thread_num, sizeof(sem_t));
    if (task->sem_thread == NULL) {
        DBG_PRINT(ERR, "sem initialization failed. %s\n", strerror(errno));
        goto _end;
    }

    /* init sem */
    for (i=0; i< task->max_thread_num; i++) {
        if (sem_init(&task->sem_thread[i], 0, 0) != 0) {
            DBG_PRINT(ERR, "Semaphore initialization failed. %s\n", strerror(errno));
            goto _end;
        }
    }

    task->pthread_detail = (pthread_info_t *)calloc(task->max_thread_num, sizeof(pthread_info_t));
    if (task->pthread_detail == NULL) {
        goto _end;
    }

    if (sem_init(&task->sem_dispath, 0, 0) != 0) {
        DBG_PRINT(ERR, "Sem dispath initialization failed. %s\n", strerror(errno));
        goto _end;
    }

    for (i=0; i< task->max_thread_num; i++) {
        task->pthread_detail[i].id = i;
        if (pthread_create(&task->thread_id[i], NULL, threadFunc, (void*)&task->pthread_detail[i]) != 0) {
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

    for (i=0; i< task->max_thread_num; i++) {
        pthread_join(task->thread_id[i], NULL);
    }

    for (i=0; i< task->max_thread_num; i++) {
        sem_destroy(&task->sem_thread[i]);
    }

    free(task->thread_id);
    free(task->sem_thread);
    free(task->pthread_detail);

    return ST_OK;
}

int main()
{
    int i = 0;

    task = (task_t*)calloc(1, sizeof(task_t));
    if (task == NULL) {
        DBG_PRINT(ERR, "malloc task failed\n", NULL);
        goto _error;
    }

    if (task_init(2) != ST_OK) {
        DBG_PRINT(ERR, "task init failed\n", NULL);
        goto _error;
    }


    DBG_PRINT(DBG, "\n init ok\n", NULL);

    sleep(1);

    while (1)
    {
        for (i=0; i< task->max_thread_num; i++) {
            if (task->pthread_detail[i].idle == 0) {
                sem_post(&task->sem_thread[i]); // 输入完成，增加信号量的值
                int semValue = 0; 
                sem_getvalue(&task->sem_thread[i], &semValue); // 获得信号量当前的值
                DBG_PRINT(DBG, "[%d] Semaphore count: %d\n", i, semValue);
            }
        }

        DBG_PRINT(DBG, "+++++++++++++ NO idle thread ++++++++++\n", NULL);
        sem_wait(&task->sem_dispath); // 再次阻塞，等待输入
        semValue = 0; 
        sem_getvalue(&task->sem_dispath, &semValue); // 获得信号量当前的值
        DBG_PRINT(DBG, "[%d] Sem dispath count: %d\n", i, semValue);
    }

    DBG_PRINT(DBG, "\nWaiting for thread to finish...\n", NULL);

    task_destory();

    exit(EXIT_SUCCESS);

_error:
        exit(EXIT_FAILURE);
}
