#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>

const int NUMBER = 2;

// ����ṹ��
typedef struct Task {
	void (*func)(void* arg); //�޷������͵ĺ���ָ��
	void* arg;
}Task;

// �̳߳ؽṹ��
typedef struct ThreadPool {
	/*�������*/
	Task* taskQ;
	int queueCapacity;	// ����
	int queueSize;		// ��ǰ�������
	int queueFront;		// ��ͷ->ȡ����
	int queueRear;		// ��β->������

	pthread_t managerID; //�������߳�ID
	pthread_t* threadIDs; //�������߳�ID

	int minNum; // ��С�߳���
	int maxNum; // ����߳���
	int busyNum; // ���ڹ����̸߳���
	int liveNum; // �����߳�
	int exitNum; // �����߳���
	pthread_mutex_t mutexpool; // �������̳߳�
	pthread_mutex_t mutexBusy; // ��busyNum

	int shutdown; // �Ƿ������̳߳أ�����1��������0
	pthread_cond_t notFull; // ��������ǲ�������
	pthread_cond_t notEmpty; //��������Ƿ����
}ThreadPool;

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	//ʹ��malloc�������ڴ棬��ֹ�Զ��ͷ�
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


		// �������
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = pool->queueRear = 0;
		pool->shutdown = 0;

		// �����߳�
		pthread_create(&pool->managerID, NULL, manager, pool);

		for (int i = 0; i < min; i++) {
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}
		return pool;
	} while (0);

	//�˳�ѭ���������쳣���ͷ���Դ
	if (pool && pool->threadIDs) free(pool->threadIDs);
	if (pool && pool->taskQ) free(pool->taskQ);
	return NULL;
}

void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;

	while (1) {
		pthreead_mutex_lock(&pool->mutexpool);
		// ��ǰ��������Ƿ�Ϊ��
		while (pool->queueSize == 0 && !pool->shutdown) {
			//���������߳�
			pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

			// �ж��Ƿ�Ҫ�����߳�
			if (pool->exitNum > 0) {
				pool->exitNum--;
				pthread_mutex_unlock(&pool->mutexpool);
				phread_exit(NULL);
			}
		}
		// �ж��߳��Ƿ񱻹ر�
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexpool);
			pthread_exit(NULL);
		}
		// �����������ȡ��һ������
		Task task;
		task.func = pool->taskQ[pool->queueFront].func;
		task.arg = pool->taskQ[pool->queueFront].arg;

		//ά�����ζ���
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;

		pthread_mutex_unlock(&pool->mutexpool);

		printf("thread %ld start working...\n");
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		task.func(task.arg);
		//(*task.func)(task.arg)
		free(task.arg); //�ֶ��ͷŶ��ڴ�
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
		// ÿ�����Ӽ��һ��
		sleep(3);

		// ȡ���̳߳�����������͵�ǰ�̵߳�����
		// Ҫ��������Ϊ�ڶ�ȡ��Щ���ݵ�ʱ����ܱ���߳���д��Щ����
		pthread_mutex_lock(&pool->mutexpool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexpool);

		// ȡ��æ���߳�����
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// ����߳�
		// ��ǰ�������������̸߳��� && ����̸߳���������̸߳���
		if (pool->queueSize > pool->liveNum && pool->liveNum < pool->maxNum) {
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
	return NULL;
}
