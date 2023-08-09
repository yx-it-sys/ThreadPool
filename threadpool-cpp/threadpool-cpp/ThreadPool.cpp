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
		// ʵ�����������
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

		// �����߳�
		pthread_create(&managerID, NULL, manager, this); // thisָ��ǰʵ�����Ķ����ָ��

		for (int i = 0; i < min; i++) {
			pthread_create(&threadIDs[i], NULL, worker, this);
		}

		return;
	} while (0);

	//�˳�ѭ���������쳣���ͷ���Դ
	if (threadIDs) delete[] threadIDs;
	if (taskQ) delete taskQ;
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
	// �ص��̳߳�
	shutdown = true;
	// �������չ������߳�
	pthread_join(managerID, NULL);
	// ���������������߳�
	for (int i = 0; i < liveNum; i++) {
		pthread_cond_broadcast(&notEmpty);
	}

	// �ͷŶ��ڴ�
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
	// �������
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
		// ��ǰ��������Ƿ�Ϊ��
		while (pool->taskQ->taskNumber() == 0 && !pool->shutdown) {
			//���������߳�
			pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

			// �ж��Ƿ�Ҫ�����߳�
			if (pool->exitNum > 0) {
				pool->exitNum--;
				if (pool->liveNum > pool->minNum) {
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexpool);
					pool->threadExit();
				}
			}
		}
		// �ж��߳��Ƿ񱻹ر�
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexpool);
			pool->threadExit();
		}
		// �����������ȡ��һ������
		 Task<T> task;
		 task = pool->taskQ->getTask();

		pool->busyNum++;

		// ����
		pthread_mutex_unlock(&pool->mutexpool);

		cout << "thread " << to_string(pthread_self()) << " start working...\n";

		task.function(task.arg);
		delete task.arg; //�ֶ��ͷŶ��ڴ�
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
		// ÿ�����Ӽ��һ��
		sleep(3);

		// ȡ���̳߳ص�ǰ�̵߳�����
		// Ҫ��������Ϊ�ڶ�ȡ��Щ���ݵ�ʱ����ܱ���߳���д��Щ����
		pthread_mutex_lock(&pool->mutexpool);
		int liveNum = pool->liveNum;

		// ȡ��æ���߳�����
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexpool);

		// ����߳�
		// ��ǰ�������������̸߳��� && ����̸߳���������̸߳���
		if (pool->taskQ->taskNumber() > pool->liveNum && pool->liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexpool);
			int count = 0;
			for (int i = 0; i < pool->maxNum && count < NUMBER
				&& pool->liveNum < pool->maxNum; ++i)
			{
				if (pool->threadIDs[i] == 0) // û�д洢�߳�
				{
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					count++;
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexpool);
		}
		// �����߳�
		// æ���߳�*2�������߳��� && �����߳���������С�߳���
		if (busyNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexpool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexpool);

			// ���������߳��Ի�
			// ��Ϊ�������޷�ֱ������worker
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
