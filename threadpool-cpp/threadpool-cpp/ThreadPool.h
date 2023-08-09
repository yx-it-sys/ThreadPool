#pragma once
#include "TaskQueue.h"
#include "TaskQueue.cpp"

template<typename T>
class ThreadPool
{
private:
	/*任务队列*/
	TaskQueue<T>* taskQ;

	pthread_t managerID; //管理者线程ID
	pthread_t* threadIDs; //工作的线程ID

	int minNum; // 最小线程数
	int maxNum; // 最大线程数
	int busyNum; // 正在工作线程个数
	int liveNum; // 存活的线程
	int exitNum; // 销毁线程数
	pthread_mutex_t mutexpool; // 锁整个线程池

	bool shutdown; // 是否销毁线程池，销毁1，不销毁0
	pthread_cond_t notEmpty; //任务队列是否空了，当任务队列为空阻塞消费者线程
	static const int NUMBER = 2;

public:
	// 创建线程池并初始化，返回值为地址类型避免复制
	ThreadPool(int min, int max);

	// 销毁线程池
	~ThreadPool();

	// 给线程池添加任务
	void addTask( Task<T> task);

	// 当前线程池中忙碌线程的个数
	int getBusyNum();

	// 获取线程池中存活的线程的个数
	int getAliveNum();

private:
	static void* worker(void* arg); //静态成员在类没有实例化的时候便有地址，可以传入pthread_create函数
	static void* manager(void* arg);
	void threadExit();
};

