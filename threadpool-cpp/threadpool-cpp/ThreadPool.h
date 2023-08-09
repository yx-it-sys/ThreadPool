#pragma once
#include "TaskQueue.h"
#include "TaskQueue.cpp"

template<typename T>
class ThreadPool
{
private:
	/*�������*/
	TaskQueue<T>* taskQ;

	pthread_t managerID; //�������߳�ID
	pthread_t* threadIDs; //�������߳�ID

	int minNum; // ��С�߳���
	int maxNum; // ����߳���
	int busyNum; // ���ڹ����̸߳���
	int liveNum; // �����߳�
	int exitNum; // �����߳���
	pthread_mutex_t mutexpool; // �������̳߳�

	bool shutdown; // �Ƿ������̳߳أ�����1��������0
	pthread_cond_t notEmpty; //��������Ƿ���ˣ����������Ϊ�������������߳�
	static const int NUMBER = 2;

public:
	// �����̳߳ز���ʼ��������ֵΪ��ַ���ͱ��⸴��
	ThreadPool(int min, int max);

	// �����̳߳�
	~ThreadPool();

	// ���̳߳��������
	void addTask( Task<T> task);

	// ��ǰ�̳߳���æµ�̵߳ĸ���
	int getBusyNum();

	// ��ȡ�̳߳��д����̵߳ĸ���
	int getAliveNum();

private:
	static void* worker(void* arg); //��̬��Ա����û��ʵ������ʱ����е�ַ�����Դ���pthread_create����
	static void* manager(void* arg);
	void threadExit();
};

