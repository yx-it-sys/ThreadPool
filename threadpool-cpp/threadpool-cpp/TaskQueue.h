#pragma once
#include <queue>
#include <pthread.h>

using callback = void(*)(void* arg);

//任务结构体
template<typename T>
struct Task
{
	Task() {
		function = nullptr;
		arg = nullptr;
	}
	Task(callback f, void* arg) {
		this->arg = (T*)arg;
		function = f;
	}
	callback function;;
	T* arg;
};

template<typename T>
class TaskQueue
{
public:
	TaskQueue();
	~TaskQueue();

	// 添加任务
	void addTask(Task<T> task);
	void addTask(callback f, void* arg);
	// 取出任务
	Task<T> getTask();
	// 获取当前任务个数
	inline size_t taskNumber() {
		return m_TaskQ.size();
	}
private:
	pthread_mutex_t m_mutex;
	std::queue<Task<T>> m_TaskQ;
};

