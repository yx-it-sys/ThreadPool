#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>

const int NUMBER = 2;

// 任务结构体
typedef struct Task {
	void (*func)(void* arg); //无返回类型的函数指针
	void* arg;
}Task;

// 线程池结构体
typedef struct ThreadPool {
	/*任务队列*/
	Task* taskQ;
	int queueCapacity;	// 容量
	int queueSize;		// 当前任务个数
	int queueFront;		// 队头->取数据
	int queueRear;		// 队尾->放数据

	pthread_t managerID; //管理者线程ID
	pthread_t* threadIDs; //工作的线程ID

	int minNum; // 最小线程数
	int maxNum; // 最大线程数
	int busyNum; // 正在工作线程个数
	int liveNum; // 存活的线程
	int exitNum; // 销毁线程数
	pthread_mutex_t mutexpool; // 锁整个线程池
	pthread_mutex_t mutexBusy; // 锁busyNum

	int shutdown; // 是否销毁线程池，销毁1，不销毁0
	pthread_cond_t notFull; // 任务队列是不是满了
	pthread_cond_t notEmpty; //任务队列是否空了
}ThreadPool;

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	//使用malloc创建堆内存，防止自动释放
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));

	do
	{
		if (pool = NULL) {
			printf("ERROR: malloc threadpool fail...\n");
			break;
		}

		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->threadIDs == NULL) {
			printf("malloc threadIDs fail...\n");
			break;
		}

		memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;
		pool->exitNum = 0;


		if (pthread_mutex_init(&pool->mutexpool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_mutex_init(&pool->notEmpty, NULL) != 0 ||
			pthread_mutex_init(&pool->notFull, NULL) != 0)
		{
			printf("mutex or condition fail...\n");
			break;
		}


		// 任务队列
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = pool->queueRear = 0;
		pool->shutdown = 0;

		// 创建线程
		pthread_create(&pool->managerID, NULL, manager, pool);

		for (int i = 0; i < min; i++) {
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}
		return pool;
	} while (0);

	//退出循环，发生异常，释放资源
	if (pool && pool->threadIDs) free(pool->threadIDs);
	if (pool && pool->taskQ) free(pool->taskQ);
	return NULL;
}

void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;

	while (1) {
		pthreead_mutex_lock(&pool->mutexpool);
		// 当前任务队列是否为空
		while (pool->queueSize == 0 && !pool->shutdown) {
			//阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

			// 判断是否要销毁线程
			if (pool->exitNum > 0) {
				pool->exitNum--;
				pthread_mutex_unlock(&pool->mutexpool);
				phread_exit(NULL);
			}
		}
		// 判断线程是否被关闭
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexpool);
			pthread_exit(NULL);
		}
		// 从任务队列中取出一个任务
		Task task;
		task.func = pool->taskQ[pool->queueFront].func;
		task.arg = pool->taskQ[pool->queueFront].arg;

		//维护环形队列
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;

		pthread_mutex_unlock(&pool->mutexpool);

		printf("thread %ld start working...\n");
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		task.func(task.arg);
		//(*task.func)(task.arg)
		free(task.arg); //手动释放堆内存
		task.arg = NULL;

		printf("thread %ld end working...\n");
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);


	}
	return NULL;
}

void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutdown)
	{
		// 每三秒钟检测一次
		sleep(3);

		// 取出线程池任务的数量和当前线程的数量
		// 要加锁，因为在读取这些数据的时候可能别的线程在写这些数据
		pthread_mutex_lock(&pool->mutexpool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexpool);

		// 取出忙的线程数量
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// 添加线程
		// 当前任务个数＞存活线程个数 && 存活线程个数＜最大线程个数
		if (pool->queueSize > pool->liveNum && pool->liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexpool);
			int count = 0;
			for (int i = 0; i < pool->maxNum && count < NUMBER
				&& pool->liveNum < pool->maxNum; ++i)
			{
				if (pool->threadIDs[i] == 0) // 没有存储线程
				{
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					count++;
					pool->liveNum++;
				}
				
			}
			pthread_mutex_unlock(&pool->mutexpool);
		}
		// 销毁线程
		// 忙的线程*2＜存活的线程数 && 存活的线程数大于最小线程数
		if (busyNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexpool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexpool);
			
			// 引导工作线程自毁
			// 因为管理者无法直接销毁worker
			for (int i = 0; i < NUMBER; ++i) {
				pthread_cond_signal(&pool->notEmpty);
			}


		}
	}
	return NULL;
}
