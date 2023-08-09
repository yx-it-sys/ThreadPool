#include "ThreadPool.h"
#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>

using namespace std;

template<typename T>
ThreadPool<T>::ThreadPool(int min, int max)
{
	do
	{
		// 实例化任务队列
		taskQ = new TaskQueue<T>;
		if (taskQ == nullptr) {
			cout << "memory allocation failed..\n";
		}
		threadIDs = new pthread_t[max];
		if (threadIDs == nullptr) {
			cout << "memory allocation failed...\n";
			break;
		}

		memset(threadIDs, 0, sizeof(pthread_t) * max);
		minNum = min;
		maxNum = max;
		busyNum = 0;
		liveNum = min;
		exitNum = 0;


		if (pthread_mutex_init(&mutexpool, NULL) != 0 ||
			pthread_cond_init(&notEmpty, NULL) != 0)
		{
			cout << "mutex or condition fail...\n";
			break;
		}

		shutdown = false;

		// 创建线程
		pthread_create(&managerID, NULL, manager, this); // this指向当前实例化的对象的指针

		for (int i = 0; i < min; i++) {
			pthread_create(&threadIDs[i], NULL, worker, this);
		}

		return;
	} while (0);

	//退出循环，发生异常，释放资源
	if (threadIDs) delete[] threadIDs;
	if (taskQ) delete taskQ;
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
	// 关掉线程池
	shutdown = true;
	// 阻塞回收管理者线程
	pthread_join(managerID, NULL);
	// 唤醒阻塞消费者线程
	for (int i = 0; i < liveNum; i++) {
		pthread_cond_broadcast(&notEmpty);
	}

	// 释放堆内存
	if (taskQ)
	{
		delete taskQ;
	}
	if (threadIDs) {
		delete[] threadIDs;
	}

	pthread_mutex_destroy(&mutexpool);
	pthread_cond_destroy(&notEmpty);
}

template<typename T>
void ThreadPool<T>::addTask( Task<T> task)
{
	pthread_mutex_lock(&mutexpool);
	if (shutdown) {
		pthread_mutex_unlock(&mutexpool);
		return;
	}
	// 添加任务
	taskQ->addTask(task);

	pthread_cond_signal(&notEmpty);
	pthread_mutex_unlock(&mutexpool);

}

template<typename T>
int ThreadPool<T>::getBusyNum()
{
	pthread_mutex_lock(&mutexpool);
	int busyNum = this->busyNum;
	pthread_mutex_unlock(&mutexpool);

	return busyNum;
}

template<typename T>
int ThreadPool<T>::getAliveNum()
{
	pthread_mutex_lock(&mutexpool);
	int liveNum = this->liveNum;
	pthread_mutex_unlock(&mutexpool);

	return liveNum;
}

template<typename T>
void* ThreadPool<T>::worker(void* arg)
{
	ThreadPool* pool = static_cast<ThreadPool*>(arg);

	while (true) {
		pthread_mutex_lock(&pool->mutexpool);
		// 当前任务队列是否为空
		while (pool->taskQ->taskNumber() == 0 && !pool->shutdown) {
			//阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

			// 判断是否要销毁线程
			if (pool->exitNum > 0) {
				pool->exitNum--;
				if (pool->liveNum > pool->minNum) {
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexpool);
					pool->threadExit();
				}
			}
		}
		// 判断线程是否被关闭
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexpool);
			pool->threadExit();
		}
		// 从任务队列中取出一个任务
		 Task<T> task;
		 task = pool->taskQ->getTask();

		pool->busyNum++;

		// 解锁
		pthread_mutex_unlock(&pool->mutexpool);

		cout << "thread " << to_string(pthread_self()) << " start working...\n";

		task.function(task.arg);
		delete task.arg; //手动释放堆内存
		task.arg = nullptr;

		cout << "thread " << to_string(pthread_self()) << " end working...\n";
		pthread_mutex_lock(&pool->mutexpool);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexpool);


	}
	return nullptr;

}

template<typename T>
void* ThreadPool<T>::manager(void* arg)
{
	ThreadPool* pool = static_cast<ThreadPool*>(arg);
	while (!pool->shutdown) {
		// 每三秒钟检测一次
		sleep(3);

		// 取出线程池当前线程的数量
		// 要加锁，因为在读取这些数据的时候可能别的线程在写这些数据
		pthread_mutex_lock(&pool->mutexpool);
		int liveNum = pool->liveNum;

		// 取出忙的线程数量
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexpool);

		// 添加线程
		// 当前任务个数＞存活线程个数 && 存活线程个数＜最大线程个数
		if (pool->taskQ->taskNumber() > pool->liveNum && pool->liveNum < pool->maxNum) {
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
	return nullptr;
}

template<typename T>
void ThreadPool<T>::threadExit()
{
	pthread_t tid = pthread_self();

	for (int i = 0; i < maxNum; i++) {
		if (threadIDs[i] == tid) {
			threadIDs[i] = 0;
			cout << "threadExit() called, " << to_string(tid) << " exiting...\n";
			break;
		}
	}
	pthread_exit(NULL);
}
