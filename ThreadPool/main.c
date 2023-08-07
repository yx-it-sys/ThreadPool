#include "threadpool.h"
#include <pthread.h>

// 任务结构体
typedef struct Task{
	void (*func)(void* arg); //无返回类型的函数指针
	void* arg;
}Task;

// 线程池结构体
struct ThreadPool {
	/*任务队列*/
	Task* TaskQ;
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
};
int main() {
	printf("%s 向你问好！\n", "threadpool");
	
	return 0;
}